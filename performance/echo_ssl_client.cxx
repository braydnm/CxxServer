#include <atomic>
#include <chrono>
#include <core/tcp/ssl_client.hxx>
#include <core/tcp/ssl_context.hxx>
#include <core/service.hxx>
#include <cxxopts.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

std::vector<uint8_t> to_send;

std::atomic<uint64_t> start = 0;
std::atomic<uint64_t> end = 0;

std::atomic<uint64_t> bytes_sent = 0;
std::atomic<uint64_t> num_errors = 0;

class EchoClient : public CxxServer::Core::SSL::Client {

public:
    EchoClient(const std::shared_ptr<CxxServer::Core::Service> &service, const std::shared_ptr<CxxServer::Core::SSL::Context> context, const std::string &addr, unsigned int port, unsigned int numMsgs)
        : Client(service, context, addr, port), _messages(numMsgs) {}

    void sendMessage() { sendAsync(to_send.data(), to_send.size()); }

protected:
    void onHandshaked() override {
        for (size_t i = 0; i < _messages; ++i)
            sendMessage();
    }

    void onSend(size_t size, size_t remaining) override { _sent += size; }

    void onReceive(const void *buffer, size_t size) override { 
        _received += size; 

        while (_received >= to_send.size())
        {
            sendMessage();
            _received -= to_send.size();
        }

        // TODO: Better storage, this is gonna fail around 2400 but that's probably not going to be my issue
        end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        bytes_sent += size;
    }

    void onErr(int error, const std::string &category, const std::string &message) override {
        std::cerr<<"[x] "<<message<<"("<<category<<"): "<<error<<std::endl;
        ++num_errors;
    }

private:
    size_t _sent = 0;
    size_t _received = 0;
    size_t _messages = 0;
};

int main(int argc, char **argv) {
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    cxxopts::Options options("Echo SSL client", "Echo client for round trip benchmarking");

    options.add_options()
        ("a,address", "Address of server, default to 127.0.0.1", cxxopts::value<std::string>()->default_value("127.0.0.1"))
        ("p,port", "Port of server to connect to, defaults to 1111", cxxopts::value<unsigned int>()->default_value("1111"))
        ("t,threads", "Number of working threads, defaults to number of physical cores", cxxopts::value<unsigned int>()->default_value(std::to_string(num_cores)))
        ("c,clients", "Number of working clients, defaults to 100", cxxopts::value<unsigned int>()->default_value("100"))
        ("m,messages", "Number of messages to send at the same time, defaults to 1000", cxxopts::value<unsigned int>()->default_value("1000"))
        ("s,size", "Single message size, defaults to 32 bytes", cxxopts::value<unsigned int>()->default_value("32"))
        ("z,seconds", "Number of seconds to run the benchmark, defaults to 10 seconds", cxxopts::value<unsigned int>()->default_value("10"));

    auto parser = options.parse(argc, argv);

    if (parser.count("help")) {
        std::cout<<options.help()<<std::endl;
        exit(0);
    }

    std::string addr = parser["address"].as<std::string>();
    unsigned int port = parser["port"].as<unsigned int>();
    unsigned int threads = parser["threads"].as<unsigned int>();
    unsigned int num_clients = parser["clients"].as<unsigned int>();
    unsigned int messages = parser["messages"].as<unsigned int>();
    unsigned int msg_size = parser["size"].as<unsigned int>();
    unsigned int seconds = parser["seconds"].as<unsigned int>();

    std::cout<<"Server address: "<<addr<<std::endl;
    std::cout<<"Server port: "<<port<<std::endl;
    std::cout<<"Number of Threads: "<<threads<<std::endl;
    std::cout<<"Number of Clients: "<<num_clients<<std::endl;
    std::cout<<"Number of Concurrent Messages: "<<messages<<std::endl;
    std::cout<<"Message Size (bytes): "<<msg_size<<std::endl;
    std::cout<<"Seconds for Benchmarking: "<<seconds<<std::endl;

    std::cout<<std::endl;

    to_send.resize(msg_size, 0);
    
    auto service = std::make_shared<CxxServer::Core::Service>();

    std::cout<<"Starting service... ";
    service->start();
    std::cout<<"done"<<std::endl;

    auto context = std::make_shared<CxxServer::Core::SSL::Context>(asio::ssl::context::tlsv12);
    context->set_default_verify_paths();
    context->set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
    context->load_verify_file("../certs/ca.pem");

    std::vector<std::shared_ptr<EchoClient>> clients;
    for (unsigned int i = 0; i < num_clients; ++i) {
        auto client = std::make_shared<EchoClient>(service, context, addr, port, messages);
        clients.push_back(client);
    }

    start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::cout<<"Connecting clients... ";
    for (auto &c : clients)
        c->connectAsync();
    std::cout<<"done"<<std::endl;

    for (const auto &c : clients)
        while (!c->isConnected())
            std::this_thread::yield();

    std::cout<<"All clients connected"<<std::endl;

    std::cout<<"Running benchmark... ";
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    std::cout<<"done"<<std::endl;

    std::cout<<"Disconnecting clients... ";
    for (auto &c : clients)
        c->disconnectAsync();
    std::cout<<"done"<<std::endl;

    for (const auto & c : clients)
        while (c->isConnected())
            std::this_thread::yield();

    std::cout<<"All threads disconnected"<<std::endl;

    std::cout << "Stopping IO service... ";
    service->stop();
    std::cout << "done" << std::endl;

    std::cout << std::endl;

    std::cout << "Errors: " << num_errors << std::endl;

    std::cout << std::endl;

    unsigned int total_messages = bytes_sent / msg_size;

    std::cout<<"Total Time: "<<(end - start)<<" ns"<<std::endl;
    std::cout<<"Total Data: "<<bytes_sent<<" bytes"<<std::endl;
    std::cout<<"Total Messages: "<<total_messages<<std::endl;
    std::cout<<"Data throughput: "<<((bytes_sent * 1000000000) / (end - start)) << " bytes/s"<<std::endl;

    if (total_messages > 0) {
        std::cout<<"Average Message Latency: "<<((end - start) / total_messages)<<" ns"<<std::endl;
        std::cout<<"Message Throughput: "<<total_messages / ((end - start) / 1000000000) << " msgs/s"<<std::endl;
    }

    return 0;
}