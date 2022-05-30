#pragma once

#include "core/memory.hxx"
#include "core/service.hxx"
#include "core/uuid.hxx"

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <system_error>
#include <vector>

namespace CxxServer::Core::Tcp {
    class Server;

    class Session : public std::enable_shared_from_this<Session> {
        friend class Server;

    public:
        explicit Session(const std::shared_ptr<Server> &);
        virtual ~Session() = default;

        Session(const Session&) = delete;
        Session(Session &&) = delete;
        Session &operator=(const Session&) = delete;
        Session &operator=(Session &&) = delete;

        //! Get session ID;
        const Uuid &id() const noexcept { return _id; }

        //! Get Server
        std::shared_ptr<Server> &server() noexcept { return _server; }

        //! Get IO
        std::shared_ptr<asio::io_service> &io() noexcept { return _io; }

        //! Get Asio strand
        asio::io_service::strand &strand() noexcept { return _strand; }

        //! Get associated socket
        asio::ip::tcp::socket &socket() noexcept { return _socket; }

        //! Get # of bytes pending
        uint64_t bytes_pending() const noexcept { return _bytes_pending; }
        //! Get # of bytes sent
        uint64_t bytes_sent() const noexcept { return _bytes_sent; }
        //! Get # of bytes received
        uint64_t bytes_received() const noexcept { return _bytes_received; }

        //! Get receive buffer limit
        size_t receive_buffer_limit() const noexcept { return _receive_limit; }
        //! Get receive buffer size
        size_t receive_buffer_size() const;
        //! Get send buffer limit
        size_t send_buffer_limit() const noexcept { return _send_limit; }
        //! Get send buffer size
        size_t send_buffer_size() const;

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
        virtual void onReceived(const void *buffer, size_t size) {}

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
        virtual void onErr(int err, const std::string &category, const std::string &message);

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

        //! Connect session
        void connect();
        
        //! Disconnect session
        /*!
         * \param dispatch - dispatch session on disconnect
         * \return true if disconnect is successful
        */
        bool disconnect(bool dispatch);

        //! Try receive data
        void tryReceive();
        //! Try send data
        void trySend();

        //! Clear all associated buffers
        void clearBuffs();
        //! Reset the server
        void resetServer();

        //! Error notification
        void err(std::error_code);
    };
}