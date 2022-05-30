#include "core/timer.hxx"
#include "asio/error.hpp"

#include <cassert>
#include <stdexcept>
#include <system_error>

namespace CxxServer::Core {
    Timer::Timer(const std::shared_ptr<Service> &service) :
        _service(service),
        _io(service->getIoService()),
        _strand(*_io),
        _strand_needed(_service->isStrandNeeded()),
        _timer(*_io) 
    {
        assert((service != nullptr) && "IO service is invalid");
        if (service == nullptr)
            throw std::invalid_argument("IO service is invalid");
    }

    Timer::Timer(const std::shared_ptr<Service> &service, const std::chrono::time_point<std::chrono::system_clock> &time) :
        _service(service),
        _io(service->getIoService()),
        _strand(*_io),
        _strand_needed(_service->isStrandNeeded()),
        _timer(*_io, time) 
    {
        assert((service != nullptr) && "IO service is invalid");
        if (service == nullptr)
            throw std::invalid_argument("IO service is invalid");
    }

    Timer::Timer(const std::shared_ptr<Service> &service, const std::chrono::nanoseconds &timespan) :
        _service(service),
        _io(service->getIoService()),
        _strand(*_io),
        _strand_needed(_service->isStrandNeeded()),
        _timer(*_io, std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds(timespan))) 
    {
        assert((service != nullptr) && "IO service is invalid");
        if (service == nullptr)
            throw std::invalid_argument("IO service is invalid");
    }

    Timer::Timer(const std::shared_ptr<Service> &service, const std::function<void(bool)> &action) :
        _service(service),
        _io(service->getIoService()),
        _strand(*_io),
        _strand_needed(_service->isStrandNeeded()),
        _timer(*_io),
        _action(action)
    {
        assert((service != nullptr) && "IO service is invalid");
        if (service == nullptr)
            throw std::invalid_argument("IO service is invalid");
    }

    Timer::Timer(const std::shared_ptr<Service> &service, const std::function<void(bool)> &action, const std::chrono::time_point<std::chrono::system_clock> &time) :
        _service(service),
        _io(service->getIoService()),
        _strand(*_io),
        _strand_needed(_service->isStrandNeeded()),
        _timer(*_io, time),
        _action(action) 
    {
        assert((service != nullptr) && "IO service is invalid");
        if (service == nullptr)
            throw std::invalid_argument("IO service is invalid");
    }

    Timer::Timer(const std::shared_ptr<Service> &service, const std::function<void(bool)> &action, const std::chrono::nanoseconds &timespan) :
        _service(service),
        _io(service->getIoService()),
        _strand(*_io),
        _strand_needed(_service->isStrandNeeded()),
        _timer(*_io, std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds(timespan))),
        _action(action)
    {
        assert((service != nullptr) && "IO service is invalid");
        if (service == nullptr)
            throw std::invalid_argument("IO service is invalid");
    }

    std::chrono::time_point<std::chrono::system_clock> Timer::expiry_time() const {
        return _timer.expires_at();
    }

    std::chrono::nanoseconds Timer::expiry_timespan() const {
        return _timer.expires_from_now();
    }

    bool Timer::setup(const std::chrono::time_point<std::chrono::system_clock> &time) {
        asio::error_code ec;
        _timer.expires_at(time, ec);

        if (ec) {
            err(ec);
            return false;
        }

        return true;
    }

    bool Timer::setup(const std::chrono::nanoseconds &timespan) {
        asio::error_code ec;
        _timer.expires_from_now(std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds(timespan)), ec);

        if (ec) {
            err(ec);
            return false;
        }

        return true;
    }

    bool Timer::setup(const std::function<void(bool)> &action) {
        assert((action) && "Action function is invalid");
        if (!action)
            return false;

        _action = action;
        return true;
    }

    bool Timer::setup(const std::function<void(bool)> &action, const std::chrono::time_point<std::chrono::system_clock> &time) {
        if (!setup(action))
            return false;

        return setup(time);        
    }

    bool Timer::setup(const std::function<void(bool)> &action, const std::chrono::nanoseconds &timespan) {
        if (!setup(action))
            return false;

        return setup(timespan);
    }

    bool Timer::waitAsync() {
        auto self = this->shared_from_this();
        auto async_wait_handler = [this, self](const std::error_code &err) {
            if (err == asio::error::operation_aborted)
                timerNotify(true);

            if (err) {
                this->err(err);
                return;
            }

            timerNotify(false);
        };

        if (_strand_needed)
            _timer.async_wait(asio::bind_executor(_strand, async_wait_handler));
        else
            _timer.async_wait(async_wait_handler);

        return true;
    }

    bool Timer::waitSync() {
        asio::error_code err;
        
        _timer.wait(err);

        if (err == asio::error::operation_aborted)
            timerNotify(true);

        if (err) {
            this->err(err);
            return false;
        }

        timerNotify(false);
        return true;
    }

    bool Timer::cancel() {
        asio::error_code err;
        _timer.cancel(err);

        if (err) {
            this->err(err);
            return false;
        }

        return true;
    }
}