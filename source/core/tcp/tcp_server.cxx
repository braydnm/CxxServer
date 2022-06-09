#include "core/tcp/tcp_server.hxx"
#include "asio/bind_executor.hpp"
#include "asio/detail/socket_option.hpp"
#include "asio/error.hpp"
#include "asio/ip/address.hpp"
#include "asio/ip/tcp.hpp"
#include "core/memory.hxx"
#include "core/uuid.hxx"

#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace CxxServer::Core::Tcp {
    Server::Server(const std::shared_ptr<Service> &service, unsigned int port, InternetProtocol proto) :
        _id(CxxServer::Core::GenUuid()),
        _service(service),
        _io(service->getIoService()),
        _strand(*_io),
        _strand_needed(service->isStrandNeeded()),
        _port(port),
        _acceptor(*_io),
        _started(false),
        _bytes_pending(0),
        _bytes_received(0),
        _bytes_sent(0),
        _keep_alive(false),
        _no_delay(false),
        _reuse_addr(false),
        _reuse_port(false)
    {
        assert((service) && "Invalid IO service");
        if (service == nullptr)
            throw std::invalid_argument("Invalid IO service");

        switch (proto) {
            case InternetProtocol::IPv4:
                _endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port);
                break;
            case InternetProtocol::IPv6:
                _endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port);
                break;
        }
    }

    Server::Server(const std::shared_ptr<Service> &service, const std::string &addr, unsigned int port) :
        _id(CxxServer::Core::GenUuid()),
        _service(service),
        _io(service->getIoService()),
        _strand(*_io),
        _strand_needed(service->isStrandNeeded()),
        _addr(addr),
        _port(port),
        _acceptor(*_io),
        _started(false),
        _bytes_pending(0),
        _bytes_received(0),
        _bytes_sent(0),
        _keep_alive(false),
        _no_delay(false),
        _reuse_addr(false),
        _reuse_port(false)
    {
        assert((service) && "Invalid IO service");
        if (service == nullptr)
            throw std::invalid_argument("Invalid IO service");

        _endpoint = asio::ip::tcp::endpoint(asio::ip::make_address(addr), port);
    }

    Server::Server(const std::shared_ptr<Service> &service, const asio::ip::tcp::endpoint &endpoint) :
        _id(CxxServer::Core::GenUuid()),
        _service(service),
        _io(service->getIoService()),
        _strand(*_io),
        _strand_needed(service->isStrandNeeded()),
        _addr(endpoint.address().to_string()),
        _port(endpoint.port()),
        _acceptor(*_io),
        _started(false),
        _bytes_pending(0),
        _bytes_received(0),
        _bytes_sent(0),
        _keep_alive(false),
        _no_delay(false),
        _reuse_addr(false),
        _reuse_port(false)
    {
        assert((service) && "Invalid IO service");
        if (service == nullptr)
            throw std::invalid_argument("Invalid IO service");
    }

    bool Server::start() {
        assert(!isStarted() && "TCP server is already running");
        if (isStarted())
            return false;

        auto self(this->shared_from_this());
        auto handler = [this, self]() {
            if (isStarted())
                return;

            _acceptor = asio::ip::tcp::acceptor(*_io);
            _acceptor.open(_endpoint.protocol());
            
            if (_reuse_addr)
                _acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));

            if (_reuse_port)
                _acceptor.set_option(
                    asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT>(true)
                );

            _acceptor.bind(_endpoint);
            _acceptor.listen();
            
            _bytes_pending = _bytes_received = _bytes_sent = 0;
            _started = true;

            onStart();

            // perform first server accept
            accept();
        };

        if (_strand_needed)
            _strand.post(handler);
        else
            _io->post(handler);

        return true;
    }

    bool Server::stop() {
        if (!isStarted())
            return false;

        auto self(this->shared_from_this());
        auto handler = [this, self] {
            if (!isStarted())
                return;
            
            _acceptor.close();
            _session.reset();

            disconnectAll();

            _started = false;

            clearMulticastBuffs();
            
            onStop();
        };

        if (_strand_needed)
            _strand.post(handler);
        else
            _io->post(handler);

        return true;
    }

    bool Server::restart() {
        if (!stop())
            return false;

        while (!isStarted())
            std::this_thread::yield();

        return start();
    }

    void Server::accept() {
        if (!isStarted())
            return;

        auto self(this->shared_from_this());
        auto handler = HandlerFastMem(_acceptor_storage, [this, self] {
            if (!isStarted())
                return;

            _session = newSession(self);

            auto async_handler = HandlerFastMem(_acceptor_storage, [this, self](std::error_code err) {
                if (!err) {
                    registerSession();
                    _session->connect();
                }
                else {
                    this->err(err);
                }

                accept();
            });

            if (_strand_needed)
                _acceptor.async_accept(_session->socket(), asio::bind_executor(_strand, async_handler));
            else
                _acceptor.async_accept(_session->socket(), async_handler);
        });

        if (_strand_needed)
            _strand.dispatch(handler);
        else
            _io->dispatch(handler);
    }

    bool Server::multicast(const void *buffer, size_t size) {
        if (!isStarted())
            return false;

        if (size == 0)
            return true;

        assert(buffer != nullptr && "Pointer to buffer to send should not be null");
        if (buffer == nullptr)
            return false;

        std::shared_lock<std::shared_mutex> locker(_sessions_lock);

        for (auto &s : _sessions)
            s.second->sendAsync(buffer, size);

        return true;
    }

    bool Server::disconnectAll() {
        if (!isStarted())
            return false;

        auto self(this->shared_from_this());
        auto disconnect = [this, self]() {
            if (!isStarted())
                return;

            std::shared_lock<std::shared_mutex> locker(_sessions_lock);
            for (auto &s : _sessions)
                s.second->disconnect();
        };

        if (_strand_needed)
            _strand.dispatch(disconnect);
        else
            _io->dispatch(disconnect);

        return true;
    }

    std::shared_ptr<Session> Server::findSession(const Uuid &id) {
        std::shared_lock<std::shared_mutex> locker(_sessions_lock);

        auto it = _sessions.find(id);
        return it != _sessions.end() ? it->second : nullptr;
    }

    void Server::registerSession() {
        std::unique_lock<std::shared_mutex> locker(_sessions_lock);

        _sessions.emplace(_session->id(), _session);
    }

    void Server::unregisterSession(const Uuid &id) {
        auto it = _sessions.find(id);
        if (it != _sessions.end())
            _sessions.erase(it);
    }

    void Server::clearMulticastBuffs() {
        _bytes_pending = 0;
    }

    void Server::err(std::error_code err) {
        if (err == asio::error::connection_aborted || err == asio::error::connection_refused
        || err == asio::error::connection_reset || err == asio::error::eof || err == asio::error::operation_aborted)
            return;

        onErr(err.value(), err.category().name(), err.message());
    }
}