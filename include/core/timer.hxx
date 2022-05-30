#pragma once

#include "asio/error.hpp"
#include "asio/io_service.hpp"
#include "core/service.hxx"

#include <asio.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <system_error>

namespace CxxServer::Core {
//! Timer
/*!
 * Timer is used to plan delayed operations
 * 
 * Thread safe
 */
class Timer : public std::enable_shared_from_this<Timer> {
public:
    //! Init timer with a given service
    /*!
     * \param service - IO service
     */
    Timer(const std::shared_ptr<Service> &service);

    //! Init timer with a given service & expiry time
    /*!
     * \param service - IO Service
     * \param time - Absolute time to expire
     */
    Timer(const std::shared_ptr<Service> &service, const std::chrono::time_point<std::chrono::system_clock> &time);

    //! Init timer with an expiry time relative to now
    /*!
     * \param service - IO service
     * \param timespan - Relative timespan in nanoseconds
     */
    Timer(const std::shared_ptr<Service> &service, const std::chrono::nanoseconds &timespan);

    //! Init timer with action function
    /*!
     * \param service - IO Service
     * \param action - Action function
     */
    Timer(const std::shared_ptr<Service> &service, const std::function<void(bool)> &action);

    //! Init timer with a given service & expiry time & action
    /*!
     * \param service - IO Service
     * \param action - Action function
     * \param time - Absolute time to expire
     */
    Timer(const std::shared_ptr<Service> &service, const std::function<void(bool)> &action, const std::chrono::time_point<std::chrono::system_clock> &time);

    //! Init timer with an expiry time relative to now & action
    /*!
     * \param service - IO service
     * \param action - Action function
     * \param timespan - Relative timespan in nanoseconds
     */
    Timer(const std::shared_ptr<Service> &service, const std::function<void(bool)> &action, const std::chrono::nanoseconds &timespan);

    virtual ~Timer() = default;

    Timer(const Timer&) = delete;
    Timer(Timer&&) = delete;

    Timer &operator=(const Timer &) = delete;
    Timer &operator=(Timer &&) = delete;

    //! Get Service
    std::shared_ptr<Service> &service() noexcept { return _service; }
    //! Get IO
    std::shared_ptr<asio::io_service> &io() noexcept {return _io; }

    //! Get strand
    asio::io_service::strand &strand() noexcept { return _strand; }

    //! Get expiry time as absolute time
    std::chrono::time_point<std::chrono::system_clock> expiry_time() const;
    //! Get expiry time as timespan
    std::chrono::nanoseconds expiry_timespan() const;

    //! Setup timer with absolute expiry
    /*!
     * \param time - Aboslute time
     * \return true if successful setup, false otherwise
     */
    virtual bool setup(const std::chrono::time_point<std::chrono::system_clock> &time);

    //! Setup timer with expiry time relative to now
    /*!
     * \param timespan - Relative timespan
     * \return true if successful setup, false otherwise
     */
    virtual bool setup(const std::chrono::nanoseconds &timespan);

    //! Setup timer with action function
    /*!
     * \param action - Action function
     * \return true if successful setup, false otherwise
     */
    virtual bool setup(const std::function<void(bool)> &action);

    //! Init timer with a given service & expiry time & action
    /*!
     * \param action - Action function
     * \param time - Absolute time to expire
     * \return true if successful setup, false otherwise
     */
    virtual bool setup(const std::function<void(bool)> &action, const std::chrono::time_point<std::chrono::system_clock> &time);

    //! Init timer with an expiry time relative to now & action
    /*!
     * \param action - Action function
     * \param timespan - Relative timespan in nanoseconds
     * \return true if successful setup, false otherwise
     */
    virtual bool setup(const std::function<void(bool)> &action, const std::chrono::nanoseconds &timespan);

    //! Async wait for timer
    /*!
     * \return true if timer expired, false for any error
     */
    virtual bool waitAsync();

    //! Sync wait for timer
    /*!
     * \return true if timer expired, false for any error
     */
    virtual bool waitSync();

    //! Cancel operations on the timer
    /*!
     * \return true if successful cancel, false otherwise
     */
    virtual bool cancel();

protected:
    virtual void onTimer(bool canceled) {}

    //! Handle error
    /*!
     * \param error - Error code
     * \param category - Error category
     * \param message - Error message
     */
    virtual void onError(int error, const std::string &category, const std::string &message) {}

private:
    std::shared_ptr<Service> _service;
    std::shared_ptr<asio::io_service> _io;
    asio::io_service::strand _strand;
    bool _strand_needed;
    asio::system_timer _timer;
    // action function to be called
    std::function<void(bool)> _action;

    void timerNotify(bool);
    inline void err(std::error_code err) {
        // skip abort error
        if (err == asio::error::operation_aborted)
            return;
        
        onError(err.value(), err.category().name(), err.message());
    }
};
}