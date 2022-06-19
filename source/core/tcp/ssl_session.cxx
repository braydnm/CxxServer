#include "core/tcp/ssl_session.hxx"
#include "core/tcp/ssl_context.hxx"
#include <memory>

namespace CxxServer::Core::SSL {
    Session::Session(const std::shared_ptr<Tcp::Server> &server, const std::shared_ptr<SSL::Context> &context) : 
        CxxServer::Core::Tcp::Session::Session(server),
        _stream(*_io, *context)
    {
        _socket.close();
    }

    void Session::asyncWriteSome(const void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler) {
        if (_strand_needed)
            _stream.async_write_some(asio::buffer(buffer, size), asio::bind_executor(_strand, handler));
        else
            _stream.async_write_some(asio::buffer(buffer, size), handler);
    }

    void Session::asyncReadSome(void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler) {
        if (_strand_needed)
            _stream.async_read_some(asio::buffer(buffer, size), asio::bind_executor(_strand, handler));
        else
            _stream.async_read_some(asio::buffer(buffer, size), handler);
    }

    void Session::connect() {
        this->socket().set_option(asio::ip::tcp::socket::keep_alive(_server->keepAlive()));
        this->socket().set_option(asio::ip::tcp::no_delay(_server->noDelay()));

        _receive_buff.resize(receiveBufferSize());
        _send_buff_main.reserve(sendBufferSize());
        _send_buff_flush.reserve(sendBufferSize());

        _bytes_sending = _bytes_sent = _bytes_pending = _bytes_received = 0;

        _connected = true;

        tryReceive();
        onConnect();

        auto session(this->shared_from_this());
        _server->onConnect(session);

        // Async SSL handshake with the handshake handler
        auto self(this->shared_from_this());
        auto async_handshake_handler = [this, self](std::error_code err)
        {
            if (isHandshaked())
                return;

            if (!err)
            {
                // Update the handshaked flag
                _handshaked = true;

                // Try to receive something from the client
                tryReceive();

                // Call the session handshaked handler
                onHandshaked();

                // TODO: this has a fairly large overhead, is there anyway around these casts?
                auto handshake_session = this->shared_from_this();
                std::static_pointer_cast<SSL::Server>(_server)->onHandshaked(handshake_session);

                // Call the empty send buffer handler
                if (_send_buff_main.empty())
                    onEmpty();
            }
            else
            {
                // Disconnect in case of the bad handshake
                this->err(err);
                disconnect(false);
            }
        };
        if (_strand_needed)
            _stream.async_handshake(asio::ssl::stream_base::server, bind_executor(_strand, async_handshake_handler));
        else
            _stream.async_handshake(asio::ssl::stream_base::server, async_handshake_handler);
    }

    void Session::err(std::error_code err) {
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