#pragma once

#if defined(__clang__)
#pragma clang system_header
#elif defined(__GNUC__)
#pragma GCC system_header
#endif

#define ASIO_STANDALONE
#define ASIO_SEPARATE_COMPILATION
#define ASIO_NO_WIN32_LEAN_AND_MEAN

// for the asio library ignore the shadow warning
// which seems to crop up for clang in the asio library
// see issue https://github.com/chriskohlhoff/asio/issues/721
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include <asio.hpp>
#include <asio/ssl.hpp>
#pragma clang diagnostic pop