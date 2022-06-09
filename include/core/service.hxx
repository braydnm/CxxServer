#pragma once

#include "core/io.hxx"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace CxxServer::Core {

//! Core service based on Asio
/*!
 * Core servie is the backbone of all underlying clients/servers, abstracting away the Asio C++ library.
 * It uses 1+ threads to perform all async IO & communication operations
 * 
 * 
 * Services have 2 design patterns:
 * 1) Each thread gets their own io-service. In this case each handler will be dispatched
 *    sequentially without strands
 * 2) All threads in this service are bounded to an singularIO pool & strnads will be required
 *    to serialize handler execution
 * 
 * Thread safe
 */

class Service : public std::enable_shared_from_this<Service> {
public:
    //! Initialize service from # of threads & threading model
    /*!
     * \param num_threads - Working thread count (defaults to 1)
     * \param own_io - Does each thread get its own IO (defaults to false)
     */
    explicit Service(std::size_t num_threads = 1, bool own_io = false);

    //! Intialize service from IO service & is strands is required
    /*!
     * \param service - IO service
     * \param strands - IO service strands required (default false)
     */
    explicit Service(const std::shared_ptr<asio::io_service> &service, bool strands = false);

    // Default destructor
    virtual ~Service() = default;

    // No copying or moving of service
    Service(const Service &) = delete;
    Service(Service &&) = delete;
    Service &operator=(const Service&) = delete;
    Service &operator=(Service &&) = delete;

    //! Get # of working threads
    std::size_t numThreads() const noexcept { return _threads.size(); }

    //! Does require strands to serialize handler execution
    bool isStrandNeeded() const noexcept { return _strand_needed; }
    //! Is the service in polling loop mode
    bool isPolling() const noexcept { return _polling; }
    //! Is the service started
    bool isStarted() const noexcept { return _started; }

    //! Start the service
    /*!
     * \param polling - Run the service in a polling loop (defaults to false)
     * \return if the service started successfully
     */
    virtual bool start(bool polling = false);

    //! Stop the service
    /*!
     * \return if the service stopped successfully
     */
    virtual bool stop();

    //! Restart the service
    /*!
     * \return if the service successfully restarted
     */
    virtual bool restart();

    //! Get the next available IO handle
    /*!
     * \return the next IO service scheduled using the Round Robin algo
     */
    virtual std::shared_ptr<asio::io_service> &getIoService() noexcept {
        return _services[++_rr_idx % _services.size()]; 
    }

    //! Dispatch the given handler
    /*!
     * The given handler may be executed immediately or enqueued into the pending operations queue
     * 
     * \param handler - handler to be dispatched
     * \return async result of the handler
     */
    template<typename Handler>
    ASIO_INITFN_RESULT_TYPE(Handler, void()) dispatch(ASIO_MOVE_ARG(Handler) handler) {
        if (_strand_needed)
            return _strand->dispatch(handler);
        else
            return _services[0]->dispatch(handler);
    }

    //! Enqueue the handler to an IO service
    /*!
     * \param handler - Handler to enqueue to the IO service
     * \return the async result of the handler
     */
    template<typename Handler>
    ASIO_INITFN_RESULT_TYPE(Handler, void()) post(ASIO_MOVE_ARG(Handler) handler) {
        if (_strand_needed)
            return _strand->post(handler);
        else
            return _services[0]->post(handler);
    }

protected:
    //! Initialize thread handler
    /*!
     * Can be used to initialize property (priority / affinity etc.) of service thread
     */
    virtual void onThreadInit() {}

    //! Cleanup service thread
    /*!
     * Cleanup property (priority / affinity etc.) of service thread
     */
    virtual void onThreadCleanup() {}

    //! Service started callback
    virtual void onStarted() {}

    //! Service stopped callback
    virtual void onStopped() {}
    
    //! Handle service idle
    virtual void onIdle() { std::this_thread::yield(); }

    //! Handle errors
    /*!
     * \param error - Error code
     * \param category - Error category
     * \param message - Error message
     */
    virtual void onErr(int error, const std::string &category, const std::string &message) {}

private:
    std::vector<std::shared_ptr<asio::io_service>> _services;
    std::vector<std::thread> _threads;
    std::shared_ptr<asio::io_service::strand> _strand;

    std::atomic<bool> _strand_needed;
    std::atomic<bool> _polling;
    std::atomic<bool> _started;
    // Round robin index
    std::atomic<std::size_t> _rr_idx;

    static void serviceThread(const std::shared_ptr<Service> &service, const std::shared_ptr<asio::io_service> &io);
};
}