#pragma once


#include "core/io.hxx"
#include "core/tcp/ssl_context.hxx"
#include "core/tcp/tcp_session.hxx"
#include "core/tcp/ssl_server.hxx"

#include <atomic>
#include <memory>
#include <system_error>

namespace CxxServer::Core::SSL {
    class Server;

    class Session : public Tcp::Session {
    public:
        friend class Server;

        Session(const std::shared_ptr<Tcp::Server> &server, const std::shared_ptr<SSL::Context> &context);
        virtual ~Session() = default;

        //! Get associated socket
        asio::ip::tcp::socket &socket() noexcept override { return _stream.next_layer(); }

        //! Get associated socket as a constant
        const asio::ip::tcp::socket &socket() const noexcept override { return _stream.next_layer(); }

        //! Get if the handshake has completed
        bool isHandshaked() const noexcept { return _handshaked; }

    protected:
        using Tcp::Session::onConnect;
        using Tcp::Session::onDisconnect;
        using Tcp::Session::onReceive;
        using Tcp::Session::onSend;
        using Tcp::Session::onEmpty;
        using Tcp::Session::onErr;

        virtual void onHandshaked() {}

    private:
        std::atomic<bool> _handshaked;
        asio::ssl::stream<asio::ip::tcp::socket> _stream;

        //! Async write some to IO
        virtual void asyncWriteSome(const void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler) override;

        //! Async read some from IO to buffer
        virtual void asyncReadSome(void *buffer, std::size_t size, HandlerFastMem<std::function<void(std::error_code, std::size_t)>> &handler) override;

        //! Connect session
        virtual void connect() override;
        
        //! Error notification
        virtual void err(std::error_code) override;

        //! Get executor for timers
        virtual asio::any_io_executor executor() override { return _stream.get_executor(); }

        //! Return if we have a valid connection that can send data i.e connected & handshaked
        virtual bool isConnectionComplete() const noexcept override { return isConnected() && isHandshaked(); }

        //! Read some from IO to buffer synchronously
        virtual std::size_t readSome(void *buffer, std::size_t size, std::error_code &err) override { return _stream.read_some(asio::buffer(buffer, size), err); }

        //! Write some to IO synchronously
        virtual size_t writeSome(const void *buffer, std::size_t size, std::error_code &err) override { return asio::write(_stream, asio::buffer(buffer, size), err); }
    };
}