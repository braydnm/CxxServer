#include "core/memory.hxx"
#include "core/tcp/tcp_server.hxx"
#include "core/tcp/tcp_session.hxx"
#include "core/uuid.hxx"

#include <asio.hpp>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <system_error>

namespace CxxServer::Core::Tcp {
    Session::Session(const std::shared_ptr<Server> &server) :
        _id(CxxServer::Core::GenUuid()),
        _server(server),
        _io(server->service()->getIoService()),
        _strand(*_io),
        _strand_needed(server->_strand_needed),
        _socket(*_io),
        _connected(false),
        _bytes_pending(0),
        _bytes_sending(0),
        _bytes_sent(0),
        _bytes_received(0),
        _receiving(false),
        _sending(false),
        _send_flush_offset(0)
    {}

    size_t Session::receiveBufferSize() const {
        asio::socket_base::receive_buffer_size size;
        _socket.get_option(size);
        return size.value();
    }

    size_t Session::sendBufferSize() const {
        asio::socket_base::send_buffer_size size;
        _socket.get_option(size);
        return size.value();
    }

    void Session::setReceiveBuffSize(size_t size) {
        asio::socket_base::receive_buffer_size opt(size);
        _socket.set_option(opt);
    }

    void Session::setSendBuffSize(size_t size) {
        asio::socket_base::send_buffer_size opt(size);
        _socket.set_option(opt);
    }

    void Session::connect() {
        _socket.set_option(asio::ip::tcp::socket::keep_alive(_server->keep_alive()));
        _socket.set_option(asio::ip::tcp::no_delay(_server->no_delay()));

        _receive_buff.resize(receiveBufferSize());
        _send_buff_main.reserve(sendBufferSize());
        _send_buff_flush.reserve(sendBufferSize());

        _bytes_sending = _bytes_sent = _bytes_pending = _bytes_received = 0;

        _connected = true;

        tryReceive();
        onConnect();

        auto session(this->shared_from_this());
        _server->onConnect(session);

        if (_send_buff_main.empty())
            onEmpty();
    }

    bool Session::disconnect(bool dispatch) {
        if (!isConnected())
            return false;

        auto self(this->shared_from_this());
        auto handler = [this, self]() {
            if (!isConnected())
                return;

            _socket.close();

            _connected = _receiving = _sending = false;

            clearBuffs();
            onDisconnect();

            auto session(this->shared_from_this());
            _server->onDisconnect(session);

            auto unregister = [this, self]() {
                _server->unregisterSession(id());
            };

            if (_server->_strand_needed)
                _server->_strand.dispatch(unregister);
            else
                _server->_io->dispatch(unregister);
        };

        if (_strand_needed) {
            if (dispatch)
                _strand.dispatch(handler);
            else
                _strand.post(handler);
        }
        else {
            if (dispatch)
                _io->dispatch(handler);
            else
                _io->post(handler);
        }

        return true;
    }

    size_t Session::send(const void *buffer, size_t size, std::chrono::nanoseconds timeout) {
        if (!isConnected())
            return 0;

        if (size == 0)
            return 0;

        assert(buffer != nullptr && "Pointer to send must not be null");
        if (buffer == nullptr)
            return 0;

        asio::error_code err;
        size_t num_bytes_sent;

        // no timeout
        if (timeout.count() == 0) {
            num_bytes_sent = asio::write(_socket, asio::buffer(buffer, size), err);
        }
        else {
            int done = 0;
            std::mutex send_mtx;
            std::condition_variable cv;
            asio::system_timer timer(_socket.get_executor());

            auto handler = [&](asio::error_code code) {
                std::unique_lock<std::mutex> locker(send_mtx);

                if (done++ == 0) {
                    err = code;
                    _socket.cancel();
                    timer.cancel();
                }

                cv.notify_one();
            };

            timer.expires_from_now(timeout);
            timer.async_wait([&](const asio::error_code &code) { handler(err ? err : asio::error::timed_out); });

            _socket.async_write_some(asio::buffer(buffer, size), [&](std::error_code code, size_t write) {
                handler(code);
                num_bytes_sent = write;
            });

            std::unique_lock<std::mutex> locker(send_mtx);
            cv.wait(locker, [&]() { return done == 2; });
        }

        if (num_bytes_sent > 0) {
            _bytes_sent += num_bytes_sent;
            _server->_bytes_sent += num_bytes_sent;

            onSend(num_bytes_sent, _bytes_pending);
        }

        if (err && err != asio::error::timed_out) {
            this->err(err);
            disconnect();
        }

        return num_bytes_sent;
    }

    bool Session::sendAsync(const void *buffer, size_t size) {
        if (!isConnected())
            return false;

        if (size == 0)
            return true;

        assert(buffer != nullptr && "Pointer to send must not be null");
        if (buffer == nullptr)
            return false;

        {
            std::scoped_lock locker(_send_lock);

            bool multiple_sends = _send_buff_main.empty() || _send_buff_flush.empty();

            if ((_send_buff_main.size() + size) > _send_limit && _send_limit > 0) {
                err(asio::error::no_buffer_space);
                return false;
            }

            auto bytes = reinterpret_cast<const uint8_t*>(buffer);
            _send_buff_main.insert(_send_buff_main.end(), bytes, bytes + size);

            _bytes_pending = _send_buff_main.size();

            if (!multiple_sends)
                return true;
        }

        auto self(this->shared_from_this());
        auto handler = [this, self]() {
            trySend();
        };

        if (_strand_needed)
            _strand.dispatch(handler);
        else
            _io->dispatch(handler);

        return true;
    }

    size_t Session::receive(void *buffer, size_t size, std::chrono::nanoseconds timeout) {
        if (!isConnected())
            return 0;

        if (size == 0)
            return 0;

        assert(buffer != nullptr && "Pointer to send must not be null");
        if (buffer == nullptr)
            return 0;

        asio::error_code err;
        size_t num_bytes_received;

        // no timeout
        if (timeout.count() == 0) {
            num_bytes_received = _socket.read_some(asio::buffer(buffer, size), err);
        }
        else {
            int done = 0;
            std::mutex send_mtx;
            std::condition_variable cv;
            asio::system_timer timer(_socket.get_executor());

            auto handler = [&](asio::error_code code) {
                std::unique_lock<std::mutex> locker(send_mtx);

                if (done++ == 0) {
                    err = code;
                    _socket.cancel();
                    timer.cancel();
                }

                cv.notify_one();
            };

            timer.expires_from_now(timeout);
            timer.async_wait([&](const asio::error_code &code) { handler(err ? err : asio::error::timed_out); });

            _socket.async_read_some(asio::buffer(buffer, size), [&](std::error_code code, size_t write) {
                handler(code);
                num_bytes_received = write;
            });

            std::unique_lock<std::mutex> locker(send_mtx);
            cv.wait(locker, [&]() { return done == 2; });
        }

        if (num_bytes_received > 0) {
            _bytes_received += num_bytes_received;
            _server->_bytes_received += num_bytes_received;

            onReceived(buffer, num_bytes_received);
        }

        if (err && err != asio::error::timed_out) {
            this->err(err);
            disconnect();
        }

        return num_bytes_received;
    }

    std::string Session::receive(size_t size, std::chrono::nanoseconds timeout) {
        std::string text(size, 0);
        text.resize(receive(text.data(), text.size(), timeout));
        return text;
    }

    void Session::receiveAsync() {
        tryReceive();
    }

    void Session::tryReceive() {
        if (_receiving || !isConnected())
            return;

        _receiving = true;
        auto self(this->shared_from_this());
        auto handler = HandlerFastMem(_receive_storage, [this, self](std::error_code err, size_t size) {
            _receiving = false;

            if (!isConnected())
                return;
            
            if (size > 0) {
                _bytes_received += size;
                _server->_bytes_received += size;

                onReceived(_receive_buff.data(), size);

                if (_receive_buff.size() == size) {
                    if (size * 2 > _receive_limit && _receive_limit > 0) {
                        this->err(asio::error::no_buffer_space);
                        disconnect(true);
                        return;
                    }

                    _receive_buff.resize(size * 2);
                }
            }

            if (!err) {
                tryReceive();
            }
            else {
                this->err(err);
                disconnect(true);
            }
        });

        if (_strand_needed)
            _socket.async_read_some(asio::buffer(_receive_buff.data(), _receive_buff.size()), asio::bind_executor(_strand, handler));
        else
            _socket.async_read_some(asio::buffer(_receive_buff.data(), _receive_buff.size()), handler);
    }

    void Session::trySend() {
        if (_sending || !isConnected())
            return;

        if (_send_buff_flush.empty()) {
            std::scoped_lock locker(_send_lock);

            _send_buff_flush.swap(_send_buff_main);
            _send_flush_offset = 0;

            _bytes_pending = 0;
            _bytes_sending += _send_buff_flush.size();
        }

        if (_send_buff_flush.empty()) {
            onEmpty();
            return;
        }

        _sending = true;
        auto self(this->shared_from_this());
        auto handler = HandlerFastMem(_send_storage, [this, self](std::error_code err, size_t size) {
            _sending = false;

            if (!isConnected())
                return;

            if (size > 0) {
                _bytes_sending -= size;
                _bytes_sent += size;

                _server->_bytes_sent += size;

                _send_flush_offset += size;

                if (_send_flush_offset == _send_buff_flush.size()) {
                    _send_buff_flush.clear();
                    _send_flush_offset = 0;
                }

                onSend(size, _bytes_pending);
            }

            if (!err) {
                trySend();
            }
            else {
                this->err(err);
                disconnect(true);
            }
        });

        if (_strand_needed)
            _socket.async_write_some(
                asio::buffer(_send_buff_flush.data() + _send_flush_offset, _send_buff_flush.size() - _send_flush_offset), 
                asio::bind_executor(_strand, handler)
            );
        else
            _socket.async_write_some(
                asio::buffer(_send_buff_flush.data() + _send_flush_offset, _send_buff_flush.size() - _send_flush_offset), 
                handler
            );
    }

    void Session::clearBuffs() {
        std::scoped_lock locker(_send_lock);

        _send_buff_main.clear();
        _send_buff_flush.clear();

        _send_flush_offset = _bytes_pending = _bytes_sending = 0;
    }

    void Session::resetServer() {
        // reset cycle references
        _server.reset();
    }

    void Session::err(std::error_code err) {
        if (err == asio::error::connection_aborted || err == asio::error::connection_refused
        || err == asio::error::connection_reset || err == asio::error::eof || err == asio::error::operation_aborted)
            return;

        onError(err.value(), err.category().name(), err.message());
    }
}