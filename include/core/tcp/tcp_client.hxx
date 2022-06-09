#pragma once

#include "core/memory.hxx"
#include "core/service.hxx"
#include "core/uuid.hxx"

#include "core/io.hxx"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace CxxServer::Core::Tcp {
//! TCP Client
/*!
 * TCP client used to read / write from connected server
 * 
 * Thread safe
 */
class Client : public std::enable_shared_from_this<Client> {
public:
    //! Initialize client with given IO service, address & port
    /*!
     * \param io - IO service
     * \param addr - Address to connect to
     * \param port - Port to connect on
     */
    Client(const std::shared_ptr<Service> &service, const std::string &addr, unsigned int port);

    //! Initialize client with given IO service, address & scheme
    /*!
     * \param io - IO service
     * \param addr - Address to connect to
     * \param scheme - Scheme to connect with
     */
    Client(const std::shared_ptr<Service> &service, const std::string &addr, const std::string &scheme);

    //! Initialize client with given IO service & endpoint
    /*!
     * \param io - IO service
     * \param endpoint - Endpoint to use
     */
    Client(const std::shared_ptr<Service> &service, const asio::ip::tcp::endpoint &endpoint);

    Client(const Client&) = delete;
    Client(Client &&) = delete;
    Client &operator=(const Client &) = delete;
    Client &operator=(Client &&) = delete;

    virtual ~Client() = default;

    //! Get client Id
    const CxxServer::Core::Uuid &id() const noexcept { return _id; }

    //! Get client IO
    std::shared_ptr<Service> &service() noexcept { return _service; }

    //! Get client strand
    std::shared_ptr<asio::io_service> &io() noexcept { return _io; }

    //! Get client endpoint
    asio::ip::tcp::endpoint &endpoint() noexcept { return _endpoint; }

    //! Get client socket
    asio::ip::tcp::socket &socket() noexcept { return _socket; }

    //! Get client address
    const std::string &addr() const noexcept { return _addr; }

    //! Get client scheme
    const std::string &scheme() const noexcept { return _scheme; }

    //! Get client port
    unsigned int port() const noexcept { return _port; }

    //! Get number of bytes remaining
    size_t numBytesPending() const noexcept { return _bytes_pending; }

    //! Get number of bytes sent
    size_t numBytesSent() const noexcept { return _bytes_sent; }

    //! Get number of bytes received
    size_t numBytesReceived() const noexcept { return _bytes_received; }

    //! Is keep alive enabled
    bool &isKeepAlive() noexcept { return _keep_alive; }

    //! Is no delay enabled
    bool &isNoDelay() noexcept { return _no_delay; }

    //! Get receive buffer limit
    size_t &receiveBuffLimit() noexcept { return _receive_buff_limit; }

    //! Get receive buffer size
    size_t receiveBuffSize() const;

    //! Set receive buffer size
    void setReceiveBuffSize(size_t);

    //! Get send buffer limit
    size_t &sendBuffLimit() noexcept { return _send_buff_limit; }
    
    //! Get send buffer size
    size_t sendBuffSize() const;

    //! Set send buffer size
    void setSendBuffSize(size_t size);

    //! Is the client connected
    bool isConnected() const noexcept { return _connected; }

    //! Connect to endpoint
    /*! Note this will not start receiving data until receive(Async) is called
     * \return true iff connection is successful
     */
    virtual bool connect();

    //! Connect async
    /*!
     * \return true iff connected
     */
    virtual bool connectAsync();

    //! Disconnect from endpoint
    /*!
     * \return true iff disconnect is successful
     */
    virtual bool disconnect();

    //! Disconnect from client async
    /*!
     * \param dispatch - Dispatch request or post(defaults to post)
     * \return true iff disconnect is successful
     */
    virtual bool disconnectAsync(bool dispatch = false);

    //! Reconnect to endpoint
    /*!
     * \return true iff reconnect is successful
     */
    virtual bool reconnect();

    //! Reconnect to endpoint async
    /*!
     * \return true iff reconnect is successful
     */
    virtual bool reconnectAsync();

    //! Send data to the server synchronously
    /*!
     * \param buffer - Buffer to send
     * \param size - Buffer size
     * \param timeout - Seconds until request timeout (defaults to 0 or no timeout)
     * \return number of bytes sent
     */
    virtual size_t send(const void *buffer, size_t size, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0));

    //! Send text to server synchronously
    /*!
     * \param text - Text to send
     * \param timeout - Seconds until request timeout (defaults to 0 or no timeout)
     * \return number of bytes(characters) sent
     */
    virtual size_t send(std::string_view text, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0)) { return send(text.data(), text.size(), timeout); }

    //! Send data to the server asynchronously
    /*!
     * \param buffer - Buffer to send
     * \param size - Buffer size
     * \return number of bytes sent
     */
    virtual bool sendAsync(const void *buffer, size_t size);

    //! Send text to server asynchronously
    /*!
     * \param text - Text to send
     * \return number of bytes(characters) sent
     */
    virtual bool sendAsync(std::string_view text) { return sendAsync(text.data(), text.size()); }

    //! Receive data from server
    /*!
     * \param buffer - Buffer to write received data to
     * \param size - Buffer size
     * \param timeout - Seconds until request timeout (defaults to 0 or no timeout)
     * \return number of bytes received
     */
    virtual size_t receive(void *buffer, size_t size, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0));

    //! Receive text from server
    /*!
     * \param size - Size of text to receive
     * \param timeout - Seconds until request timeout (defaults to 0 or no timeout)
     * \return text received
     */
    virtual std::string receive(size_t size, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0));

    //! Receive data from server asynchronously
    virtual void receiveAsync();


protected:
    //! On Connect callback
    virtual void onConnect() {}

    //! On disconnect callback
    virtual void onDisconnect() {}

    //! On Data receive callback
    /*!
     * \param buffer - Received data
     * \param size - data size
     */
    virtual void onReceive(const void *buffer, size_t size) {}

    //! On data send callback
    /*!
     * \param sent - Number of bytes sent
     * \param remaining - Number of bytes left to send
     */
    virtual void onSend(size_t sent, size_t remaining) {}

    //! Callback when there is no data to send (idle)
    virtual void onEmpty() {}

    //! On error callback
    /*!
     * \param err - Error code
     * \param category - Error category
     * \param msg - Error message
     */
    virtual void onErr(int err, const std::string &category, const std::string &msg) {}

private:
    CxxServer::Core::Uuid _id;
    std::shared_ptr<Service> _service;
    std::shared_ptr<asio::io_service> _io;
    asio::io_service::strand _strand;
    bool _strand_needed;

    std::string _addr;
    std::string _scheme;
    unsigned int _port;

    asio::ip::tcp::endpoint _endpoint;
    asio::ip::tcp::socket _socket;

    std::atomic<bool> _connecting;
    std::atomic<bool> _connected;

    uint64_t _bytes_pending;
    uint64_t _bytes_sending;
    uint64_t _bytes_sent;
    uint64_t _bytes_received;

    bool _receiving;
    size_t _receive_buff_limit;
    std::vector<uint8_t> _receive_buff;
    HandlerMemory<> _receive_storage;

    bool _sending;
    std::mutex _send_lock;
    size_t _send_buff_limit;
    std::vector<uint8_t> _send_buff_main;
    std::vector<uint8_t> _send_buff_flush;
    size_t _send_flush_offset;
    HandlerMemory<> _send_storage;

    bool _keep_alive;
    bool _no_delay;

    //! Try to read new data
    void tryReceive();

    //! Try to send data
    void trySend();

    //! Clear buffers
    void clearBuffs();

    //! Handle errors
    void err(std::error_code);
};
}