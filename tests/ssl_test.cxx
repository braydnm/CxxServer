#include "catch2/catch.hpp"

#include "core/service.hxx"
#include "core/tcp/ssl_client.hxx"
#include "core/tcp/ssl_context.hxx"
#include "core/tcp/ssl_server.hxx"
#include "core/tcp/ssl_session.hxx"

namespace {
    using SslSession = CxxServer::Core::SSL::Session;
    using SslServer = CxxServer::Core::SSL::Server;
    using SslClient = CxxServer::Core::SSL::Client;
    using SslContext = CxxServer::Core::SSL::Context;
    
    using TcpSession = CxxServer::Core::Tcp::Session;
    using TcpServer = CxxServer::Core::Tcp::Server;
    using TcpClient = CxxServer::Core::Tcp::Client;

    class EchoService : public CxxServer::Core::Service {
        public:
            using CxxServer::Core::Service::Service;
            std::atomic<bool> thread_init = false;
            std::atomic<bool> thread_clean = false;
            std::atomic<bool> started = false;
            std::atomic<bool> stopped = false;
            std::atomic<bool> idle = false;
            std::atomic<bool> errors = false;
        protected:
            void onThreadInit() override { thread_init = true; }
            void onThreadCleanup() override { thread_clean = true; }
            void onStarted() override { started = true; }
            void onStopped() override { stopped = true; }
            void onIdle() override { idle = true; }
            void onErr(int error, const std::string &category, const std::string &message) override { errors = true; }
    };

    class EchoSession : public SslSession {
    public:
        using SslSession::Session;
        std::atomic<bool> connected = false;
        std::atomic<bool> handshaked = false;
        std::atomic<bool> disconnected = false;
        std::atomic<bool> errors = false;

    protected:
        void onConnect() override { connected = true; }
        void onHandshaked() override { handshaked = true; }
        void onDisconnect() override { disconnected = true; }
        void onErr(int error, const std::string &category, const std::string &message) override { errors = true; }
        void onReceive(const void *data, size_t size) override { 
            sendAsync(data, size); 
        }
    };

    class EchoServer : public SslServer {
        public:
            using SslServer::Server;
            std::atomic<bool> started = false;
            std::atomic<bool> stopped = false;
            std::atomic<bool> connected = false;
            std::atomic<bool> handshaked = false;
            std::atomic<bool> disconnected = false;
            std::atomic<size_t> connections = 0;
            std::atomic<bool> errors = false;

            static std::shared_ptr<SslContext> CreateContext()
            {
                auto context = std::make_shared<SslContext>(asio::ssl::context::tlsv12);
                context->set_password_callback([](size_t max_length, asio::ssl::context::password_purpose purpose) -> std::string { return "qwerty"; });
                context->use_certificate_chain_file("../certs/server.pem");
                context->use_private_key_file("../certs/server.pem", asio::ssl::context::pem);
                context->use_tmp_dh_file("../certs/dh4096.pem");
                return context;
            }

        protected:
            std::shared_ptr<TcpSession> newSession(const std::shared_ptr<TcpServer> &server) override { return std::make_shared<EchoSession>(server, context()); }

            void onStart() override { started = true; }
            void onStop() override { stopped = true; }
            void onConnect(std::shared_ptr<TcpSession> &s) override { connected = true; ++connections; }
            void onHandshaked(std::shared_ptr<TcpSession> &session) override { handshaked = true; }
            void onDisconnect(std::shared_ptr<TcpSession> &s) override { disconnected = true; --connections; }
            void onErr(int error, const std::string &category, const std::string &message) override { errors = true; }
    };

    class EchoClient : public SslClient {
    public:
        using SslClient::Client;
        std::atomic<bool> connected = false;
        std::atomic<bool> handshaked = false;
        std::atomic<bool> disconnected = false;
        std::atomic<bool> errors = false;

        static std::shared_ptr<SslContext> CreateContext()
        {
            auto context = std::make_shared<SslContext>(asio::ssl::context::tlsv12);
            context->set_default_verify_paths();
            context->set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
            context->load_verify_file("../certs/ca.pem");
            return context;
        }

    protected:
        void onConnect() override { connected = true; }
        void onHandshaked() override { handshaked = true; }
        void onDisconnect() override { disconnected = true; }
        void onErr(int error, const std::string &category, const std::string &message) override { errors = true; }
    };

    TEST_CASE("SSL server test", "[CxxServer][SSL]") {
        const std::string address = "127.0.0.1";
        const unsigned int port = 2222;

        // Create and start IO service
        auto service = std::make_shared<EchoService>();
        INFO("Starting service");
        REQUIRE(service->start());
        while (!service->isStarted())
            std::this_thread::yield();

        INFO("Service started");

        auto server_context = EchoServer::CreateContext();

        auto server = std::make_shared<EchoServer>(service, server_context, port);
        REQUIRE(server->start());
        while (!server->isStarted())
            std::this_thread::yield();

        INFO("Server started")

        auto client_context = EchoClient::CreateContext();
        auto client = std::make_shared<EchoClient>(service, client_context, address, port);
        REQUIRE(client->connectAsync());
        INFO("Client connecting")
        while (!client->isReady() || (server->connections != 1))
            std::this_thread::yield();

        INFO("Client connected")
        client->sendAsync("test");

        INFO("Client sent message")

        while (client->numBytesReceived() != 4)
            std::this_thread::yield();

        INFO("Client received message")

        REQUIRE(client->disconnectAsync());
        while (client->isConnected() || (server->connections != 0))
            std::this_thread::yield();

        REQUIRE(server->stop());
        while (server->isStarted())
            std::this_thread::yield();

        REQUIRE(service->stop());
        while (service->isStarted())
            std::this_thread::yield();

        // Check the Asio service state
        REQUIRE(service->thread_init);
        REQUIRE(service->thread_clean);
        REQUIRE(service->started);
        REQUIRE(service->stopped);
        REQUIRE(!service->idle);
        REQUIRE(!service->errors);

        // Check the Echo server state
        REQUIRE(server->started);
        REQUIRE(server->stopped);
        REQUIRE(server->connected);
        REQUIRE(server->handshaked);
        REQUIRE(server->disconnected);
        REQUIRE(server->numBytesSent() == 4);
        REQUIRE(server->numBytesReceived() == 4);
        REQUIRE(!server->errors);

        // Check the Echo client state
        REQUIRE(client->connected);
        REQUIRE(client->handshaked);
        REQUIRE(client->disconnected);
        REQUIRE(client->numBytesSent() == 4);
        REQUIRE(client->numBytesReceived() == 4);
        REQUIRE(!client->errors);
    }

    TEST_CASE("SSL multicast server test", "[CxxServer][SSL]") {
        const std::string address = "127.0.0.1";
        const unsigned int port = 2223;

        auto service = std::make_shared<EchoService>();
        REQUIRE(service->start(true));
        while (!service->isStarted())
            std::this_thread::yield();

        auto server_context = EchoServer::CreateContext();
        auto server = std::make_shared<EchoServer>(service, server_context, address, port);
        REQUIRE(server->start());
        while (!server->isStarted())
            std::this_thread::yield();

        auto client_context = EchoClient::CreateContext();
        std::vector<std::pair<std::shared_ptr<EchoClient>, size_t>> clients;
        for (size_t i = 0; i < 3; ++i) {
            clients.push_back({std::make_shared<EchoClient>(service, client_context, address, port), i});
            REQUIRE(clients.back().first->connectAsync());
            while (!clients.back().first->isReady() || server->connections != clients.size())
                std::this_thread::yield();

            server->multicast("test");

            bool receiving = true;
            while (receiving) {
                receiving = false;
                for (auto &c : clients) {
                    if (c.first->numBytesReceived() == 4 * (i - c.second + 1))
                        continue;

                    receiving = true;
                    break;
                }

                if (receiving)
                    std::this_thread::yield();
            }
        }

        for (size_t i = 0; i < 3; ++i) {
            auto &client = clients[i];

            REQUIRE(client.first->disconnectAsync());
            while (client.first->isReady() || server->connections != clients.size() - i - 1)
                std::this_thread::yield();

            server->multicast("test");

            bool receiving = true;
            while (receiving) {
                receiving = false;
                for (size_t j = i + 1; j < clients.size(); ++j) {
                    auto &c = clients[j];
                    if (c.first->numBytesReceived() == 4 * (4 - c.second + i))
                        continue;

                    receiving = true;
                    break;
                }

                if (receiving)
                    std::this_thread::yield();
            }
        }

        REQUIRE(server->stop());
        while (server->isStarted())
            std::this_thread::yield();

        REQUIRE(service->stop());
        while (service->isStarted())
            std::this_thread::yield();

        REQUIRE(service->thread_init);
        REQUIRE(service->thread_clean);
        REQUIRE(service->started);
        REQUIRE(service->stopped);
        REQUIRE(service->idle);
        REQUIRE(!service->errors);

        // Check the Echo server state
        REQUIRE(server->started);
        REQUIRE(server->stopped);
        REQUIRE(server->connected);
        REQUIRE(server->handshaked);
        REQUIRE(server->disconnected);
        REQUIRE(server->numBytesSent() == 36);
        REQUIRE(server->numBytesReceived() == 0);
        REQUIRE(!server->errors);

        // Check the Echo client state
        for (auto &c : clients) {
            REQUIRE(c.first->numBytesSent() == 0);
            REQUIRE(c.first->numBytesReceived() == 12);
            REQUIRE(!c.first->errors);
        }
    }

    TEST_CASE("SSL random stress test", "[CxxServer][SSL]") {
        const std::string address = "127.0.0.1";
        const unsigned int port = 2224;

        auto client_context = EchoClient::CreateContext();
        std::vector<std::shared_ptr<EchoClient>> clients;
        clients.reserve(100);

        auto service = std::make_shared<EchoService>();
        REQUIRE(service->start());
        while (!service->isStarted())
            std::this_thread::yield();

        auto server_context = EchoServer::CreateContext();
        auto server = std::make_shared<EchoServer>(service, server_context, address, port);
        REQUIRE(server->start());
        while (!server->isStarted())
            std::this_thread::yield();

        auto start = std::chrono::high_resolution_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < 10) {
            if ((rand() % 1000) == 0) {
                server->disconnectAll();
            }
            else if ((rand() % 100) == 0 && clients.size() < 100) {
                clients.push_back(std::make_shared<EchoClient>(service, client_context, address, port));
                clients.back()->connectAsync();
                while (!clients.back()->isReady())
                    std::this_thread::yield();
            }
            else if ((rand() % 100) == 0 && !clients.empty()) {
                size_t idx = rand() % clients.size();
                auto client = clients[idx];

                if (client->isReady()) {
                    client->disconnectAsync();
                    while (client->isConnected())
                        std::this_thread::yield();
                }
                else if (!client->isConnected()) {
                    client->connectAsync();
                    while (!client->isReady())
                        std::this_thread::yield();
                }
            }
            else if ((rand() % 100) == 0 && !clients.empty()) {
                size_t idx = rand() % clients.size();
                if (clients[idx]->isReady()) {
                    clients[idx]->reconnectAsync();
                    while (!clients[idx]->isReady())
                        std::this_thread::yield();
                }
            }
            else if ((rand() % 10) == 0) {
                server->multicast("test");
            }
            else if (!clients.empty()) {
                size_t idx = rand() % clients.size();
                if (clients[idx]->isReady())
                    clients[idx]->sendAsync("test");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        for (auto & c : clients) {
            c->disconnectAsync();
            while (c->isReady())
                std::this_thread::yield();
        }

        REQUIRE(server->stop());
        while (server->isStarted())
            std::this_thread::yield();

        REQUIRE(service->stop());
        while (service->isStarted())
            std::this_thread::yield();

        REQUIRE(server->started);
        REQUIRE(server->stopped);
        REQUIRE(server->connected);
        REQUIRE(server->handshaked);
        REQUIRE(server->disconnected);
        REQUIRE(server->numBytesSent() > 0);
        REQUIRE(server->numBytesReceived() > 0);
        REQUIRE(!server->errors);
    }
}