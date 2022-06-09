#pragma once

#if defined(__clang__)
#pragma clang system_header
#elif defined(__GNUC__)
#pragma GCC system_header
#endif

#define ASIO_STANDALONE
#define ASIO_SEPARATE_COMPILATION
#define ASIO_NO_WIN32_LEAN_AND_MEAN

#include <asio.hpp>