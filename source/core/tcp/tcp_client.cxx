#include "core/tcp/tcp_client.hxx"
#include "core/memory.hxx"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

namespace CxxServer::Core::Tcp {

Client::Client(const std::shared_ptr<Service> &service, const std::string &addr, unsigned int port) :
    _id(GenUuid()),
    _service(service),
    _io(_service->getIoService()),
    _strand(*_io),
    _strand_needed(_service->isStrandNeeded()),
    _addr(addr),
    _port(port),
    _socket(*_io),
    _connecting(false),
    _connected(false),
    _bytes_pending(0),
    _bytes_sending(0),
    _bytes_sent(0),
    _bytes_received(0),
    _receiving(false),
    _receive_buff_limit(0),
    _sending(false),
    _send_buff_limit(0),
    _send_flush_offset(0),
    _keep_alive(false),
    _no_delay(false) 
{
    assert((service != nullptr) && "IO service is invalid");
    if (service == nullptr)
        throw std::invalid_argument("IO service is invalid");
}

Client::Client(const std::shared_ptr<Service> &service, const std::string &addr, const std::string &scheme) : Client(service, addr, 0)
{
        _scheme = scheme;
}

Client::Client(const std::shared_ptr<Service> &service, const asio::ip::tcp::endpoint &endpoint) : Client(service, endpoint.address().to_string(), endpoint.port())
{

}

size_t Client::receiveBuffSize() const {
    asio::socket_base::receive_buffer_size option;
    _socket.get_option(option);
    return option.value();
}

size_t Client::sendBuffSize() const {
    asio::socket_base::send_buffer_size option;
    _socket.get_option(option);
    return option.value();
}

void Client::setReceiveBuffSize(size_t size) {
    asio::socket_base::receive_buffer_size option(size);
    _socket.set_option(option);
}

void Client::setSendBuffSize(size_t size) {
    asio::socket_base::send_buffer_size option(size);
    _socket.set_option(option);
}

bool Client::connect() {
    if (isConnected())
        return false;

    asio::error_code err;

    _endpoint = asio::ip::tcp::endpoint(asio::ip::make_address(_addr), _port);

    _socket.connect(_endpoint, err);

    if (err) {
        this->err(err);
        onDisconnect();
        return false;
    }

    _socket.set_option(asio::ip::tcp::socket::keep_alive(_keep_alive));
    _socket.set_option(asio::ip::tcp::no_delay(_no_delay));

    _receive_buff.resize(receiveBuffSize());
    _send_buff_main.reserve(sendBuffSize());
    _send_buff_flush.reserve(sendBuffSize());

    _bytes_pending = _bytes_sending = _bytes_received = _bytes_sent = 0;
    _connected = true;

    onConnect();

    if (_send_buff_main.empty())
        onEmpty();

    return true;
}

bool Client::connectAsync() {
    if (isConnected() || _connecting)
        return false;

    auto self(this->shared_from_this());
    auto handler = [this, self]() {
        if (isConnected() || _connecting)
            return;

        _connecting = true;
        auto connected_handler = [this, self](std::error_code err) {
            _connecting = false;

            if (isConnected() || _connecting)
                return;

            if (!err) {
                _socket.set_option(asio::ip::tcp::socket::keep_alive(_keep_alive));
                _socket.set_option(asio::ip::tcp::no_delay(_no_delay));

                _receive_buff.resize(receiveBuffSize());
                _send_buff_main.reserve(sendBuffSize());
                _send_buff_flush.reserve(sendBuffSize());

                _bytes_pending = _bytes_sending = _bytes_received = _bytes_sent = 0;
                _connected = true;

                onConnect();

                tryReceive();

                onConnect();

                if (_send_buff_main.empty())
                    onEmpty();
            }
            else {
                this->err(err);
                onDisconnect();
            }
        };

        _endpoint = asio::ip::tcp::endpoint(asio::ip::make_address(_addr), _port);
        if (_strand_needed)
            _socket.async_connect(_endpoint, bind_executor(_strand, connected_handler));
        else
            _socket.async_connect(_endpoint, connected_handler);
    };


    if (_strand_needed)
        _strand.post(handler);
    else
        _io->post(handler);

    return true;
}

bool Client::disconnect() {
    if (!isConnected())
        return false;

    _socket.close();

    _connecting = false;
    _connected = false;

    _receiving = false;
    _sending = false;

    clearBuffs();
    onDisconnect();

    return true;
}

bool Client::disconnectAsync(bool dispatch) {
    if (!isConnected() || _connecting)
        return false;

    asio::error_code err;
    _socket.cancel(err);

    auto self(this->shared_from_this());
    auto handler = [this, self]() { disconnect(); };
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

bool Client::reconnect() {
    if (!disconnect())
        return false;

    return connect();
}

bool Client::reconnectAsync() {
    if (!disconnectAsync())
        return false;

    while (isConnected())
        std::this_thread::yield();

    return connectAsync();
}

size_t Client::send(const void *buffer, size_t size, std::chrono::nanoseconds timeout) {
    if (!isConnected() || size == 0)
        return 0;

    assert(buffer != nullptr && "Buffer should not be null");
    if (buffer == nullptr)
        return 0;

    asio::error_code err;
    size_t sent;

    if (timeout.count() == 0) {
        sent = asio::write(_socket, asio::buffer(buffer, size));
    }
    else {
        int done;
        std::mutex mtx;
        std::condition_variable cv;
        asio::system_timer timer(_socket.get_executor());

        auto handler = [&](asio::error_code e) {
            std::unique_lock<std::mutex> lock(mtx);
            if (done++ == 0) {
                err = e;
                _socket.cancel();
                timer.cancel();
            }
            cv.notify_one();
        };

        timer.expires_from_now(timeout);
        timer.async_wait([&](const asio::error_code &e) { handler(e ? e : asio::error::timed_out); });

        _socket.async_write_some(asio::buffer(buffer, size), [&](std::error_code e, size_t s) { handler(e); sent = s; });

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() { return done == 2; });
    }

    if (sent > 0) {
        _bytes_sent += sent;
        onSend(sent, _bytes_pending);
    }

    if (err && err != asio::error::timed_out) {
        this->err(err);
        disconnect();
    }

    return sent;
}

size_t Client::receive(void *buffer, size_t size, std::chrono::nanoseconds timeout) {
    if (!isConnected() || size == 0)
        return 0;

    assert(buffer != nullptr && "Pointer to receive buffer should not be null");
    if (buffer == nullptr)
        return 0;

    asio::error_code err;
    size_t received;

    if (timeout.count() == 0) {
        received = _socket.read_some(asio::buffer(buffer, size), err);
    }
    else {
        int done;
        std::mutex mtx;
        std::condition_variable cv;
        asio::system_timer timer(_socket.get_executor());

        auto handler = [&](asio::error_code ec) {
            std::unique_lock<std::mutex> lock(mtx);
            if (done++ == 0) {
                err = ec;
                _socket.cancel();
                timer.cancel();
            }

            cv.notify_one();
        };

        timer.expires_from_now(timeout);
        timer.async_wait([&](const asio::error_code &ec) { handler(ec ? ec : asio::error::timed_out); });

        received = 0;
        _socket.async_read_some(asio::buffer(buffer, size), [&](std::error_code ec, size_t read) { handler(ec); received  = read;});

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() { return done == 2; });
    }

    if (received > 0) {
        _bytes_received += received;
        onReceive(buffer, received);
    }

    if (err && err != asio::error::timed_out) {
        this->err(err);
        disconnect();
    }

    return received;
}

std::string Client::receive(size_t size, std::chrono::nanoseconds timeout) {
    std::string ret(size, 0);
    ret.resize(receive(ret.data(), size, timeout));
    return ret;
}

void Client::tryReceive() {
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

            onReceive(_receive_buff.data(), size);
            if (_receive_buff.size() == size) {
                if (size * 2 > _receive_buff_limit && _receive_buff_limit > 0) {
                    this->err(asio::error::no_buffer_space);
                    disconnectAsync(true);
                    return;
                }

                _receive_buff.resize(2 * size);
            }
        }

        // try to receive again if we can
        if (!err) {
            tryReceive();
        }
        else {
            this->err(err);
            disconnectAsync(true);
        }
    });

    if (_strand_needed)
        _socket.async_read_some(asio::buffer(_receive_buff.data(), _receive_buff.size()), bind_executor(_strand, handler));
    else
        _socket.async_read_some(asio::buffer(_receive_buff.data(), _receive_buff.size()), handler);
}

void Client::trySend() {
    if (_sending || !isConnected())
        return;

    if (_send_buff_flush.empty()) {
        std::scoped_lock lock(_send_lock);

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
            disconnectAsync(true);
        }
    });

    if (_strand_needed)
        _socket.async_write_some(asio::buffer(_send_buff_flush.data(), _send_buff_flush.size()), bind_executor(_strand, handler));
    else
        _socket.async_write_some(asio::buffer(_send_buff_flush.data(), _send_buff_flush.size()), handler);
}

void Client::clearBuffs() {
    std::scoped_lock locker(_send_lock);

    _send_buff_main.clear();
    _send_buff_flush.clear();

    _bytes_sending = _bytes_pending = _send_flush_offset = 0;
}

void Client::err(std::error_code err) {
    if (err == asio::error::connection_aborted || err == asio::error::connection_refused ||
        err == asio::error::connection_reset || err == asio::error::eof ||
        err == asio::error::operation_aborted)
            return;

    onErr(err.value(), err.category().name(), err.message());
}

}