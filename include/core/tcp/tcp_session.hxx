#pragma once

#include "core/io.hxx"
#include "core/memory.hxx"
#include "core/properties.hxx"
#include "core/service.hxx"
#include "core/uuid.hxx"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <system_error>
#include <vector>


// TODO: Clean up messy forward declarations
namespace CxxServer::Core::SSL {
    class Session;
}

namespace CxxServer::Core::Tcp {
    class Server;

    class Session : public std::enable_shared_from_this<Session>, private noncopyable, private nonmovable{
        friend class Server;
        friend class CxxServer::Core::SSL::Session;

    public:
        Session(const std::shared_ptr<Server> &);
        virtual ~Session() = default;

        //! Get session ID;
        const Uuid &id() const noexcept { return _id; }

        //! Get IO
        std::shared_ptr<asio::io_service> &io() noexcept { return _io; }

        //! Get Asio strand
        asio::io_service::strand &strand() noexcept { return _strand; }

        //! Get associated socket
        virtual asio::ip::tcp::socket &socket() noexcept { return _socket; }

        //! Get associated socket as a constant
        virtual const asio::ip::tcp::socket &socket() const noexcept { return _socket; }

        //! Get # of bytes pending
        uint64_t bytesPending() const noexcept { return _bytes_pending; }
        //! Get # of bytes sent
        uint64_t bytesSent() const noexcept { return _bytes_sent; }
        //! Get # of bytes received
        uint64_t bytesReceived() const noexcept { return _bytes_received; }

        //! Get receive buffer limit
        size_t receiveBufferLimit() const noexcept { return _receive_limit; }
        //! Get receive buffer size
        size_t receiveBufferSize() const;
        //! Get send buffer limit
        size_t sendBufferLimit() const noexcept { return _send_limit; }
        //! Get send buffer size
        size_t sendBufferSize() const;

        //! Is session connected?
        bool isConnected() const noexcept { return _connected; }

        //! Disconnect the session
        /*!
         * \return true iff session disconnect is successful
         */
        virtual bool disconnect() { return this->disconnect(false); }

        //! Send data synchronously
        /*!
         * \param buffer - Buffer to send
         * \param size - Buffer size
         * \param timeout - Timeout for request (defaults to 0 or no timeout)
         * \return number of bytes sent
         */
        virtual size_t send(const void *buffer, size_t size, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0L));

        //! Send text to client synchronously
        /*!
         * \param text - text to send
         * \param timeout - Timeout for request (defaults to 0 or no timeout)
         * \return number of bytes sent
         */
        virtual size_t send(std::string_view text, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0L)) { return send(text.data(), text.size(), timeout); }

        //! Async data send
        /*!
         * \param buffer - Buffer to send
         * \param size - Buffer size
         * \return true if sent successfully, false if not connected
         */
        virtual bool sendAsync(const void *buffer, size_t size);
        
        //! Async text send
        /*!
         * \param text - text to send
         * \return true if sent successfully, false if not connected
         */
        virtual bool sendAsync(std::string_view text) { return sendAsync(text.data(), text.size()); }

        //! Receive data synchronously
        /*!
         * \param buffer - Buffer to receive
         * \param size - Number of bytes to receive
         * \param timeout - Timeout for request (defaults to 0 or no timeout)
         * \return number of bytes received
         */
        virtual size_t receive(void *buffer, size_t size, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0L));

        //! Receive a string synchronously
        /*!
         * \param size - Number of bytes to receive
         * \param timeout - Timeout for request (defaults to 0 or no timeout)
         * \return text received
         */
        virtual std::string receive(size_t size, std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0L));

        //! Receive data asynchronously
        virtual void receiveAsync();

        //! Set receive buffer limit
        /*!
         * Note: The session will be disconnected if this limit is reached, the default is unlimited
         * \param limit - receive buffer limit
         */
        void setReceiveBuffLimit(size_t limit) noexcept { _receive_limit = limit; }
        //! Set send buffer limit
        /*!
         * Note: The session will be disconnected if this limit is reached, the default is unlimited
         * \param limit - send buffer limit
         */
        void setSendBuffLimit(size_t limit) noexcept { _send_limit = limit; }
        //! Set receive buffer size
        /*!
         * Note: This will setup SO_RCVBUF
         * \param size - Receive buffer size
         */
        void setReceiveBuffSize(size_t size);
        //! Set send buffer size
        /*!
         * Note: This will setup SO_SNDBUF
         * \param size - Send buffer size
         */
        void setSendBuffSize(size_t size);

    protected:
        virtual void onConnect() {}
        virtual void onDisconnect() {}

        //! Callback when data is received
        /*!
         * \param buffer - buffer containing data received
         * \param size - number of bytes received
         */
        virtual void onReceive(const void *buffer, size_t size) {}

        //! Callback when data is sent
        /*!
         * \param sent - size of data sent
         * \param remaining - number of bytes waiting to be sent
         */
        virtual void onSend(size_t sent, size_t remaining) {}

        //! Callback when send buffer is empty and more data can be sent
        virtual void onEmpty() {}

        //! Handle errors
        /*!
         * \param error - Error code
         * \param category - Error category
         * \param message - Error message
         */
        virtual void onErr(int err, const std::string &category, const std::string &message) {}

    private:
        Uuid _id;

        std::shared_ptr<Server> _server;
        std::shared_ptr<asio::io_service> _io;
        asio::io_service::strand _strand;
        bool _strand_needed;

        asio::ip::tcp::socket _socket;
        std::atomic<bool> _connected;

        uint64_t _bytes_pending;
        uint64_t _bytes_sending;
        uint64_t _bytes_sent;
        uint64_t _bytes_received;

        bool _receiving;
        size_t _receive_limit = 0;
        std::vector<uint8_t> _receive_buff;
        HandlerMemory<> _receive_storage;

        bool _sending;
        std::mutex _send_lock;
        size_t _send_limit = 0;
        std::vector<uint8_t> _send_buff_main;
        std::vector<uint8_t> _send_buff_flush;
        size_t _send_flush_offset;
        HandlerMemory<> _send_storage;

        //! Async write some to IO
        virtual void asyncWriteSome(const void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler);

        //! Async read some from IO to buffer
        virtual void asyncReadSome(void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler);

        //! Clear all associated buffers
        void clearBuffs();

        //! Close server
        virtual void close() { socket().close(); }

        //! Connect session
        virtual void connect();
        
        //! Disconnect session
        /*!
         * \param dispatch - dispatch session on disconnect
         * \return true if disconnect is successful
        */
        virtual bool disconnect(bool dispatch);

        //! Error notification
        virtual void err(std::error_code);

        //! Get executor for timers
        virtual asio::any_io_executor executor() { return _socket.get_executor(); }

        virtual bool isConnectionComplete() const noexcept { return isConnected(); }

        //! Try receive data
        void tryReceive();

        //! Try send data
        void trySend();

        //! Reset the server
        void resetServer();

        //! Read some from IO to buffer synchronously
        virtual std::size_t readSome(void *buffer, std::size_t size, std::error_code &err) { return _socket.read_some(asio::buffer(buffer, size), err); }

        //! Write some to IO synchronously
        virtual size_t writeSome(const void *buffer, std::size_t size, std::error_code &err) { return asio::write(this->socket(), asio::buffer(buffer, size), err); }
    };
}