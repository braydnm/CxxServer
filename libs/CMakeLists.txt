include("asio.cmake")
if (NOT TARGET uuid)
    add_subdirectory("uuid")
endif()