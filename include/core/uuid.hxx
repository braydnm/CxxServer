#pragma once

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>

namespace CxxServer::Core {
    using Uuid = boost::uuids::uuid;

    inline Uuid GenUuid() {
        static thread_local boost::uuids::random_generator rg;
        return rg();
    }
}