#include "core/service.hxx"
#include "core/tcp/ssl_session.hxx"
#include "core/tcp/ssl_context.hxx"
#include "core/tcp/ssl_session.hxx"

#include <cstdlib>
#include <cxxopts.hpp>

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

class EchoSession : public CxxServer::Core::SSL::Session {
public:
    using CxxServer::Core::SSL::Session::Session;
protected:
    void onReceive(const void *buffer, size_t size) override { sendAsync(buffer, size); }
    void onErr(int error, const std::string &category, const std::string &message) override {
        std::cerr<<"[x] "<<message<<"("<<category<<"): "<<error<<std::endl;
    }
};

class EchoServer : public CxxServer::Core::SSL::Server {
public:
    using CxxServer::Core::SSL::Server::Server;

protected:
    std::shared_ptr<CxxServer::Core::Tcp::Session> newSession(const std::shared_ptr<CxxServer::Core::Tcp::Server> &server) override {
        return std::make_shared<EchoSession>(server, context());
    }

    void onErr(int error, const std::string &category, const std::string &message) override {
        std::cerr<<"[x] "<<message<<"("<<category<<"): "<<error<<std::endl;
    }
};

int main(int argc, char **argv) {

    long num_threads_default = sysconf(_SC_NPROCESSORS_ONLN);

    cxxopts::Options options("Echo SSL Server", "Echo server for round trip performance benchmarking");

    options.add_options()
        ("p,port", "Port to bind to", cxxopts::value<unsigned int>()->default_value("1111"))
        ("t,threads", "Number of work threads", cxxopts::value<unsigned int>()->default_value(std::to_string(num_threads_default)));

    auto parsed = options.parse(argc, argv);

    if (parsed.count("help")) {
        std::cout<<options.help()<<std::endl;
        exit(0);
    }

    unsigned int port = parsed["port"].as<unsigned int>();
    unsigned int num_threads = parsed["threads"].as<unsigned int>();

    std::cout<<"Port: "<<port<<std::endl;
    std::cout<<"Num threads: "<<num_threads<<std::endl;

    std::cout<<std::endl;

    std::cout<<"Starting IO service... ";
    auto service = std::make_shared<CxxServer::Core::Service>(num_threads);
    service->start();
    std::cout<<"done"<<std::endl;

    auto context = std::make_shared<CxxServer::Core::SSL::Context>(asio::ssl::context::tlsv12);
    context->set_password_callback([](size_t max_length, asio::ssl::context::password_purpose purpose) -> std::string { return "qwerty"; });
    context->use_certificate_chain_file("../certs/server.pem");
    context->use_private_key_file("../certs/server.pem", asio::ssl::context::pem);
    context->use_tmp_dh_file("../certs/dh4096.pem");

    std::cout<<"Starting server... ";
    auto server = std::make_shared<EchoServer>(service, context, port);
    server->reusePort() = true;
    server->reuseAddress() = true;
    server->start();
    std::cout<<"done"<<std::endl;

    std::cout<<"Press enter to stop, or \"!\" to restart the server"<<std::endl;
    std::string line;
    while(std::getline(std::cin, line)) {
        if (line.empty())
            break;

        if (line != "!")
            continue;

        std::cout<<"Restarting server... ";
        server->restart();
        std::cout<<"done"<<std::endl;
    }

    std::cout<<"Stopping server... ";
    server->stop();
    std::cout<<"done"<<std::endl;

    std::cout<<"Stopping service... ";
    service->stop();
    std::cout<<"done"<<std::endl;

    return 0;
}