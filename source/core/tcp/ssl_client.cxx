#include "core/tcp/ssl_client.hxx"
#include "core/memory.hxx"
#include "core/tcp/tcp_client.hxx"

#include <functional>
#include <memory>
#include <openssl/err.h>
#include <system_error>

namespace CxxServer::Core::SSL {
        Client::Client(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const std::string &addr, unsigned int port) :
            Tcp::Client::Client(service, addr, port),
            _context(context),
            _stream(*_io, *context)
        {            
            assert((context != nullptr) && "SSL context is invalid");
            if (context == nullptr)
                throw std::invalid_argument("SSL context is invalid");
        }

        Client::Client(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const std::string &addr, const std::string &scheme) :
            Tcp::Client::Client(service, addr, scheme),
            _context(context),
            _stream(*_io, *context)
        {
            assert((context != nullptr) && "SSL context is invalid");
            if (context == nullptr)
                throw std::invalid_argument("SSL context is invalid");
        }

        Client::Client(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const asio::ip::tcp::endpoint &endpoint) :
            Tcp::Client::Client(service, endpoint),
            _context(context),
            _stream(*_io, *context)
        {
            assert((context != nullptr) && "SSL context is invalid");
            if (context == nullptr)
                throw std::invalid_argument("SSL context is invalid");
        }

        void Client::asyncWriteSome(const void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler) {
            if (_strand_needed)
                _stream.async_write_some(asio::buffer(buffer, size), asio::bind_executor(_strand, handler));
            else
                _stream.async_write_some(asio::buffer(buffer, size), handler);
        }

        //! Async read some from IO to buffer
        void Client::asyncReadSome(void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler) {
            if (_strand_needed)
                _stream.async_read_some(asio::buffer(buffer, size), asio::bind_executor(_strand, handler));
            else
                _stream.async_read_some(asio::buffer(buffer, size), handler);
        }

        bool Client::disconnect() {
            if (!isConnected() || _connecting || _handshaking)
                return false;

            _handshaked = false;
            _handshaking = false;

            Tcp::Client::disconnect();

            return true;
        }

        bool Client::disconnectAsync(bool dispatch) {
            if (!isConnected() || _connecting || _handshaking)
                return false;

            auto self = this->shared_from_this();
            auto disconnect_handler = HandlerFastMem(_connecting_storage, [this, self]() {
                if (!isConnected() || _connecting || _handshaking)
                    return;

                std::error_code err;
                socket().cancel(err);

                auto shutdown_handler = HandlerFastMem(_connecting_storage, [this, self](std::error_code shut_err) {
                    disconnect(); 
                });
                if (_strand_needed)
                    _stream.async_shutdown(asio::bind_executor(_strand, shutdown_handler));
                else
                    _stream.async_shutdown(shutdown_handler);
            });

            if (_strand_needed) {
                if (dispatch)
                    _strand.dispatch(disconnect_handler);
                else
                    _strand.post(disconnect_handler);
            }
            else {
                if (dispatch)
                    _io->dispatch(disconnect_handler);
                else
                    _io->post(disconnect_handler);
            }

            return true;
        }

        bool Client::connect() {
            std::error_code err;

            if (isConnected() || hasHandshaked() || _handshaking || _connecting)
                return false;

            // make a new stream as if we try to reconnect with the old one it will die
            _stream = asio::ssl::stream<asio::ip::tcp::socket>(*_io, *_context);

            socket().connect(_endpoint, err);

            if (err) {
                this->err(err);
                onDisconnect();
                return false;
            }

            socket().set_option(asio::ip::tcp::socket::keep_alive(_keep_alive));
            socket().set_option(asio::ip::tcp::no_delay(_no_delay));

            _receive_buff.resize(receiveBuffSize());
            _send_buff_main.reserve(sendBuffSize());
            _send_buff_flush.reserve(sendBuffSize());

            _bytes_pending = _bytes_sending = _bytes_received = _bytes_sent = 0;
            _connected = true;

            onConnect();

            _stream.handshake(asio::ssl::stream_base::client, err);
            if (err) {
                this->err(err);
                onDisconnect();
                return false;
            }

            _handshaked = true;
            onHandshaked();

            if (_send_buff_main.empty())
                onEmpty();

            return true;
        }

        void Client::connectEndpoint(const std::shared_ptr<Tcp::Client> &self, std::error_code err, const asio::ip::tcp::endpoint &endpoint) {
            // we have now (potentially) connected, check for errors, if connected then handshake
            _connecting = false;
            if (isConnected() || hasHandshaked() || _handshaking || _connecting)
                return;


            if (err) {
                this->err(err);
                onDisconnect();
                return;
            }

            _endpoint = endpoint;

            socket().set_option(asio::ip::tcp::socket::keep_alive(_keep_alive));
            socket().set_option(asio::ip::tcp::no_delay(_no_delay));

            _receive_buff.resize(receiveBuffSize());
            _send_buff_main.reserve(sendBuffSize());
            _send_buff_flush.reserve(sendBuffSize());

            _bytes_pending = _bytes_sending = _bytes_received = _bytes_sent = 0;
            _connected = true;

            onConnect();

            _handshaking = true;
            auto handshake_handler = HandlerFastMem(_connecting_storage, [this, self](std::error_code handshake_err) {
                _handshaking = false;

                if (hasHandshaked())
                    return;

                if (handshake_err) {
                    this->err(handshake_err);
                    disconnectAsync(true);
                    return;
                }

                _handshaked = true;
                onHandshaked();

                tryReceive();

                if (_send_buff_main.empty())
                    onEmpty();
            });

            if (_strand_needed)
                _stream.async_handshake(asio::ssl::stream_base::client, asio::bind_executor(_strand, handshake_handler));
            else
                _stream.async_handshake(asio::ssl::stream_base::client, handshake_handler);
        }

        bool Client::connectAsync() {
            if (isConnected() || hasHandshaked() || _handshaking || _connecting)
                return false;

            // make a new stream as if we try to reconnect with the old one it will die
            _stream = asio::ssl::stream<asio::ip::tcp::socket>(*_io, *_context);

            auto self = this->shared_from_this();
            auto connect_handler = HandlerFastMem(_connecting_storage, [this, self]() {
                if (isConnected() || hasHandshaked() || _handshaking || _connecting)
                    return;

                _connecting = true;
                auto connect_callback_handler = HandlerFastMem(_connecting_storage, [this, self](std::error_code err) { 
                    connectEndpoint(self, err,  _endpoint); 
                });
                _endpoint = asio::ip::tcp::endpoint(asio::ip::make_address(_addr), _port);
                if (_strand_needed)
                    socket().async_connect(_endpoint, asio::bind_executor(_strand, connect_callback_handler));
                else
                    socket().async_connect(_endpoint, connect_callback_handler);
            });

            if (_strand_needed)
                _strand.post(connect_handler);
            else
                _io->post(connect_handler);

            return true;            
        }

        void Client::err(std::error_code err) {
            // ignore disconnect errors
            if (err == asio::error::connection_aborted ||
            err == asio::error::connection_refused ||
            err == asio::error::connection_reset ||
            err == asio::error::eof ||
            err == asio::error::operation_aborted ||
            err == asio::ssl::error::stream_truncated)
                return;

            // Skip OpenSSL annoying errors
            if (err.category() == asio::error::get_ssl_category() && (
                ERR_GET_REASON(err.value()) == SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC ||
                ERR_GET_REASON(err.value()) == SSL_R_PROTOCOL_IS_SHUTDOWN ||
                ERR_GET_REASON(err.value()) == SSL_R_WRONG_VERSION_NUMBER)
            )
                return;

            onErr(err.value(), err.category().name(), err.message());   
        }
}