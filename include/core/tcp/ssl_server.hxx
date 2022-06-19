#pragma once

#include "core/tcp/tcp_server.hxx"
#include "core/tcp/ssl_context.hxx"
#include "core/tcp/ssl_session.hxx"
#include "core/tcp/tcp_session.hxx"

#include <memory>

namespace CxxServer::Core::SSL {

    class Server : public CxxServer::Core::Tcp::Server {
    public:
        friend Tcp::Server;
        friend class Session;

        //! Init server with IO & port #
        /*!
         * \param io - IO service
         * \param context - SSL context
         * \param port - port #
         * \param proto - Internet protocol to use (defaults to IPv4)
         */
        Server(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, unsigned int port, InternetProtocol proto = InternetProtocol::IPv4);

        //! Init server with IO, address & port to use
        /*!
         * \param io - IO service
         * \param context - SSL context
         * \param addr - Address to use
         * \param port - port to use
         */
        Server(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const std::string &addr, unsigned int port);

        //! Init server with IO & endpoint
        /*!
         * \param io - IO service
         * \param context - SSL context
         * \param endpoint - endpoint to use
         */
        Server(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const asio::ip::tcp::endpoint &endpoint);

        const std::shared_ptr<Context> &context() const noexcept { return _context; }
    
    protected:
        // forward declarations from the Tcp server for second level class inheritance
        using Tcp::Server::onStart;
        using Tcp::Server::onStop;
        using Tcp::Server::onConnect;
        using Tcp::Server::onDisconnect;
        using Tcp::Server::onErr;

        virtual std::shared_ptr<Tcp::Session> newSession(const std::shared_ptr<Tcp::Server> &server) override;

        //! Handle session handshaked notification
        /*!
         * \param session - Handshaked session
         */
        virtual void onHandshaked(std::shared_ptr<Tcp::Session> &session) {}

    private:
        std::shared_ptr<Context> _context;

    };
}