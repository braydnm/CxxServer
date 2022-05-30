#pragma once

#define ASIO_STANDALONE
#define ASIO_SEPARATE_COMPILATION
#define ASIO_NO_WIN32_LEAN_AND_MEAN

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <iostream>

namespace CxxServer::Core {
    enum class InternetProtocol { IPv4, IPv6 };

    template<class OutStream>
    inline OutStream &operator<<(OutStream &o, InternetProtocol p) {
        if (p == InternetProtocol::IPv4)
            o << "IPv4";
        else if (p == InternetProtocol::IPv6)
            o << "IPv6";
        else
            o << "<unknown>";

        return o;
    }
}