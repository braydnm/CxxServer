#pragma once

#include <iostream>
#include <string>

namespace CxxServer::Core {

#define die(...) CxxServer::Core::fatal(__FILE__, __LINE__, __VA_ARGS__)

inline void fatal(const char *filename, const int line, const std::string& message) noexcept
{
    std::cerr << "Fatal error: " << message << std::endl;
    std::cerr << "Source location: " << filename << ":" << line << std::endl;
    std::abort();
}

}