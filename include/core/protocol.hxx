#pragma once

#include "core/io.hxx"

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