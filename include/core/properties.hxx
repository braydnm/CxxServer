#pragma once

namespace CxxServer::Core {
class noncopyable {
public:
    noncopyable() = default;
    noncopyable(noncopyable &) = delete;
    noncopyable &operator=(noncopyable &)  = delete;
};

class nonmovable {
public:
    nonmovable() = default;
    nonmovable(nonmovable &&) = delete;
    nonmovable &operator=(nonmovable &&) = delete;
};
}