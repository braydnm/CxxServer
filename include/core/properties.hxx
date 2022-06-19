#pragma once

class noncopyable {
public:
    noncopyable(noncopyable &) = delete;
    noncopyable &operator=(noncopyable &)  = delete;
};

class nonmovable {
public:
    nonmovable(nonmovable &&) = delete;
    nonmovable &operator=(nonmovable &&) = delete;
};