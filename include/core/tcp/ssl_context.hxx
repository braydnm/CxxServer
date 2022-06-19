#pragma once

#include "core/io.hxx"
#include "core/properties.hxx"
#include "core/service.hxx"

namespace CxxServer::Core::SSL {
    class Context : public asio::ssl::context, private noncopyable, private nonmovable {
    public:
        using asio::ssl::context::context;
        
        ~Context() = default;
    };
}