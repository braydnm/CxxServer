#include "core/tcp/ssl_server.hxx"
#include <stdexcept>

namespace CxxServer::Core::SSL {
    
    Server::Server(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, unsigned int port, InternetProtocol proto) :
        Tcp::Server::Server(service, port, proto),
        _context(context) 
    {
        assert((context != nullptr) && "SSL context is invalid!");
        if (context == nullptr)
            throw std::invalid_argument("SSL context is invalid!");
    }

    Server::Server(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const std::string &addr, unsigned int port) :
        Tcp::Server::Server(service, addr, port),
        _context(context)
    {
        assert((context != nullptr) && "SSL context is invalid!");
        if (context == nullptr)
            throw std::invalid_argument("SSL context is invalid!");
    }

    Server::Server(const std::shared_ptr<Service> &service, const std::shared_ptr<Context> &context, const asio::ip::tcp::endpoint &endpoint) :
        Tcp::Server::Server(service, endpoint),
        _context(context)
    {
        assert((context != nullptr) && "SSL context is invalid!");
        if (context == nullptr)
            throw std::invalid_argument("SSL context is invalid!");
    }

    std::shared_ptr<Tcp::Session> Server::newSession(const std::shared_ptr<Tcp::Server> &server) {
        return std::make_shared<Session>(server, _context);
    }

}