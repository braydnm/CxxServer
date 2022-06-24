# CxxServer

Low latency, high performance, asynchronous socket server & client C++ library with
support TCP, SSL, and future support for HTTP, HTTPS, WebSocket protocols and [10K connections problem](https://en.wikipedia.org/wiki/C10k_problem)
solution.

# Contents
  * [Features](#features)
  * [Requirements](#requirements)
  * [How to build?](#how-to-build)
  * [Performance](#performance)
    * [Benchmark: Round-Trip](#benchmark-round-trip)
      * [TCP echo server](#tcp-echo-server)
      * [SSL echo server](#ssl-echo-server)

# Features
* [Asynchronous communication](https://think-async.com)
* Supported CPU scalability designs: IO service per thread, thread pool
* Supported transport protocols: [TCP](#example-tcp-chat-server), [SSL](#example-ssl-chat-server)
* WIP Web protocols: [HTTP](#example-http-server), [HTTPS](#example-https-server),
  [WebSocket](#example-websocket-chat-server), [WebSocket secure](#example-websocket-secure-chat-server)

# Requirements
* Linux
* [cmake](https://www.cmake.org)
* [gcc](https://gcc.gnu.org)
* [git](https://git-scm.com)

# How to build?

### Linux: install required packages
```shell
sudo apt-get install -y binutils-dev uuid-dev libssl-dev
```

### Setup repository
```shell
git clone --recursive https://github.com/braydnm/CxxServer.git
cd CxxServer
cd certs && ./gen.sh && cd ../
```

### Build Release
``` shell
mkdir build && cd build
cmake ../ -DCMAKE_BUILD_TYPE=Release
make -j $(nproc)
```

# Performance

Benchmark Environment:
```
CPU: Intel(R) Core(TM) i5-8250U
CPU Logical Cores: 8
CPU Physical Cores: 4
Clock Speed: 1.60GHz
Hyperthreading: enabled
Total Memory: 7825
Used Memory: 1254 
Free Memory: 4958

OS: 5.18.3-arch1-1 #1 SMP PREEMPT_DYNAMIC Thu, 09 Jun 2022 16:14:10 +0000 x86_64 GNU/Linux
Build Config: Release
```

## Benchmark: Round-Trip

This scenario sends lots of messages from several clients to a server.
The server responses to each message and resend the similar response to
the client. The benchmark measures total round-trip time to send all
messages and receive all responses, messages & data throughput, count
of errors.

### TCP echo server

* [cxxserver-performance-echo_tcp_server](https://github.com/braydnm/CxxServer/blob/master/performance/echo_tcp_server.cxx)
* [cxxserver-performance-echo_tcp_client](https://github.com/braydnm/CxxServer/blob/master/performance/echo_tcp_client.cxx) --clients 1 --threads 1
```
Server address: 127.0.0.1
Server port: 1111
Number of Threads: 1
Number of Clients: 1
Number of Concurrent Messages: 1000
Message Size (bytes): 32
Seconds for Benchmarking: 10

Starting service... done
Connecting clients... done
All clients connected
Running benchmark... done
Disconnecting clients... done
All threads disconnected
Stopping IO service... done

Errors: 0

Total Time: 10000342464 ns
Total Data: 9191510336 bytes
Total Messages: 287234698
Data throughput: 919119557 bytes/s
Average Message Latency: 34 ns
Message Throughput: 28723469 msgs/s
```

* [cxxserver-performance-echo_tcp_server](https://github.com/braydnm/CxxServer/blob/master/performance/echo_tcp_server.cxx)
* [cxxserver-performance-echo_tcp_client](https://github.com/braydnm/CxxServer/blob/master/performance/echo_tcp_client.cxx) --clients 100 --threads 4
```
Server address: 127.0.0.1
Server port: 1111
Number of Threads: 4
Number of Clients: 100
Number of Concurrent Messages: 1000
Message Size (bytes): 32
Seconds for Benchmarking: 10

Starting service... done
Connecting clients... done
All clients connected
Running benchmark... done
Disconnecting clients... done
All threads disconnected
Stopping IO service... done

Errors: 0

Total Time: 10034466743 ns
Total Data: 6301315552 bytes
Total Messages: 196916111
Data throughput: 627967156 bytes/s
Average Message Latency: 50 ns
Message Throughput: 19691611 msgs/s
```

### SSL echo server
* [cxxserver-performance-echo_ssl_server](https://github.com/braydnm/CxxServer/blob/master/performance/echo_ssl_server.cxx)
* [cxxserver-performance-echo_ssl_client](https://github.com/braydnm/CxxServer/blob/master/performance/echo_ssl_client.cxx) --clients 1 --threads 1
```
Server address: 127.0.0.1
Server port: 1111
Number of Threads: 1
Number of Clients: 1
Number of Concurrent Messages: 1000
Message Size (bytes): 32
Seconds for Benchmarking: 10

Starting service... done
Connecting clients... done
All clients connected
Running benchmark... done
Disconnecting clients... done
All threads disconnected
Stopping IO service... done

Errors: 0

Total Time: 10006817947 ns
Total Data: 3261808672 bytes
Total Messages: 101931521
Data throughput: 325958630 bytes/s
Average Message Latency: 98 ns
Message Throughput: 10193152 msgs/s
```

* [cxxserver-performance-echo_ssl_server](https://github.com/braydnm/CxxServer/blob/master/performance/echo_ssl_server.cxx)
* [cxxserver-performance-echo_ssl_client](https://github.com/braydnm/CxxServer/blob/master/performance/echo_ssl_client.cxx) --clients 100 --threads 4
```
Server address: 127.0.0.1
Server port: 1111
Number of Threads: 4
Number of Clients: 100
Number of Concurrent Messages: 1000
Message Size (bytes): 32
Seconds for Benchmarking: 10

Starting service... done
Connecting clients... done
All clients connected
Running benchmark... done
Disconnecting clients... done
All threads disconnected
Stopping IO service... done

Errors: 0

Total Time: 10061187170 ns
Total Data: 2374995072 bytes
Total Messages: 74218596
Data throughput: 236055152 bytes/s
Average Message Latency: 135 ns
Message Throughput: 7421859 msgs/s
```

### Roadmap

- [x] TCP Support
- [x] SSL Support
- [ ] HTTP(S) Support
- [ ] WebSocket Support