#pragma once

#include "core/io.hxx"
#include "core/memory.hxx"
#include "core/tcp/ssl_context.hxx"
#include "core/tcp/tcp_client.hxx"

#include <atomic>
#include <memory>
#include <system_error>

namespace CxxServer::Core::SSL {
    class Client : public Core::Tcp::Client {
    public:
        //! Initialize client with given IO service, address & port
        /*!
         * \param io - IO service
         * \param context - SSL context
         * \param addr - Address to connect to
         * \param port - Port to connect on
         */
        Client(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const std::string &addr, unsigned int port);

        //! Initialize client with given IO service, address & scheme
        /*!
         * \param io - IO service
         * \param context - SSL context
         * \param addr - Address to connect to
         * \param scheme - Scheme to connect with
         */
        Client(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const std::string &addr, const std::string &scheme);

        //! Initialize client with given IO service & endpoint
        /*!
         * \param io - IO service
         * \param context - SSL context
         * \param endpoint - Endpoint to use
         */
        Client(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const asio::ip::tcp::endpoint &endpoint);

        Client(const Client&) = delete;
        Client(Client &&) = delete;
        Client &operator=(const Client &) = delete;
        Client &operator=(Client &&) = delete;

        //! Get associated socket
        asio::ip::tcp::socket &socket() noexcept override { return _stream.next_layer(); }

        //! Get associated socket as a constant
        const asio::ip::tcp::socket &socket() const noexcept override { return _stream.next_layer(); }

        bool hasHandshaked() const noexcept { return _handshaked; }

        //! Connect to endpoint
        /*! Note this will not start receiving data until receive(Async) is called
         * \return true iff connection is successful
         */
        virtual bool connect() override;

        //! Connect async
        /*!
         * \return true iff connected
         */
        virtual bool connectAsync() override;

        //! Disconnect from endpoint
        /*!
         * \return true iff disconnect is successful
         */
        virtual bool disconnect() override;

        //! Disconnect from client async
        /*!
         * \param dispatch - Dispatch request or post(defaults to post)
         * \return true iff disconnect is successful
         */
        virtual bool disconnectAsync(bool dispatch = false) override;

        virtual bool isReady() const noexcept override { return isConnected() && hasHandshaked(); }

    protected:
        using Tcp::Client::onConnect;
        using Tcp::Client::onDisconnect;
        using Tcp::Client::onReceive;
        using Tcp::Client::onSend;
        using Tcp::Client::onEmpty;
        using Tcp::Client::onErr;

        virtual void onHandshaked() {}

    private:
        std::atomic<bool> _handshaking;
        std::atomic<bool> _handshaked;

        const std::shared_ptr<Context> _context;
        asio::ssl::stream<asio::ip::tcp::socket> _stream;
        HandlerMemory<> _connecting_storage;

        //! Async write some to IO
        void asyncWriteSome(const void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler) override;

        //! Async read some from IO to buffer
        void asyncReadSome(void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler) override;

        //! Async connect to an endpoint
        void connectEndpoint(const std::shared_ptr<Tcp::Client> &self, std::error_code err, const asio::ip::tcp::endpoint &endpoint);

        //! Handle errors
        void err(std::error_code) override;

        //! Get executor for timers
        asio::any_io_executor executor() override { return _socket.get_executor(); }

        //! Read some from IO to buffer synchronously
        std::size_t readSome(void *buffer, std::size_t size, std::error_code &err) override { return _stream.read_some(asio::buffer(buffer, size), err); }

        //! Write some to IO synchronously
        size_t writeSome(const void *buffer, std::size_t size, std::error_code &err) override { return asio::write(_stream, asio::buffer(buffer, size), err); }
    };
}