#include "core/errors.hxx"
#include "core/service.hxx"

#include <cassert>
#include <cstddef>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>

namespace CxxServer::Core {
    void Service::serviceThread(const std::shared_ptr<Service> &service, const std::shared_ptr<asio::io_service> &io) {
        bool polling = service->isPolling();

        service->onThreadInit();

        try {
            asio::io_service::work work(*io);
            do {
                try {
                    if (polling) {
                        io->poll();
                        service->onIdle();
                    }
                    else {
                        io->run();
                        break;
                    }
                } catch (const asio::system_error &err) {
                    // skip disconnect errors
                    if (err.code() == asio::error::not_connected)
                        continue;

                    throw;
                }
            } while (service->isStarted());
        } catch (const asio::system_error &err) {
            auto sys_err = err.code();
            service->onErr(sys_err.value(), sys_err.category().name(), sys_err.message());
        }
        catch (const std::exception &err) {
            die(err.what());
        }
        catch (...) {
            die("IO thread terminated");
        }

        service->onThreadCleanup();
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        // Delete OpenSSL thread state
        OPENSSL_thread_stop();
#endif
    }

    Service::Service(std::size_t num_threads, bool own_io) : _strand_needed(false), _polling(false), _started(false), _rr_idx(0) {
        if (num_threads == 0)
        {
            // no threads => single IO service
            _services.emplace_back(std::make_shared<asio::io_service>());
        }
        else if (!own_io)
        {
            // IO service per thread
            for (std::size_t i = 0; i < num_threads; ++i)
            {
                _services.emplace_back(std::make_shared<asio::io_service>());
                _threads.emplace_back(std::thread());
            }
        }
        else
        {
            // One IO service per thread
            _services.emplace_back(std::make_shared<asio::io_service>());
            for (std::size_t i = 0; i < num_threads; ++i)
                _threads.emplace_back(std::thread());

            _strand = std::make_shared<asio::io_service::strand>(*_services[0]);
            _strand_needed = true;
        }
    }

    Service::Service(const std::shared_ptr<asio::io_service> &service, bool strands) : _strand_needed(strands), _polling(false), _started(false), _rr_idx(0) {
        assert((service != nullptr) && "IO service is invalid");
        if (service == nullptr)
            throw std::invalid_argument("IO service is invalid");

        _services.emplace_back(service);
        if (strands)
            _strand = std::make_shared<asio::io_service::strand>(*_services[0]);
    }

    bool Service::start(bool polling) {
        assert(!isStarted() && "IO service is already running");
        if (isStarted())
            return false;

        _polling = polling;
        _rr_idx = 0;

        auto self = this->shared_from_this();
        auto start_handler = [this, self]() {
            if (isStarted())
                return;

            _started = true;
            onStarted();
        };

        this->post(start_handler);

        for (std::size_t i = 0; i < _threads.size(); ++i) {
            _threads[i] = std::thread([this, self, i]() {
                serviceThread(self, _services[i % _services.size()]);
            });
        }

        while (!isStarted())
            std::this_thread::yield();

        return true;
    }

    bool Service::stop() {
        assert(isStarted() && "Service is not started");
        if (!isStarted())
            return false;

        auto self(this->shared_from_this());
        auto stop_handler = [this, self]() {
            if (!isStarted())
                return;

            for (auto &service : _services)
                service->stop();

            _started = false;
            onStopped();
        };

        if (_strand_needed)
            _strand->post(stop_handler);
        else
            _services[0]->post(stop_handler);

        for (auto &thread : _threads)
            thread.join();
        
        _polling = false;

        while (isStarted())
            std::this_thread::yield();

        return true;
    }

    bool Service::restart() {
        bool polling = isPolling();

        if (!stop())
            return false;

        // reinit all the IO services
        for (size_t service = 0; service < _services.size(); ++service)
            _services[service] = std::make_shared<asio::io_service>();
        if (_strand_needed)
            _strand = std::make_shared<asio::io_service::strand>(*_services[0]);

        return start(polling);
    }
}