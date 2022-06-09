cmake_minimum_required(VERSION 3.20)

# Global properties
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Project name
project(cxxserver)

message("-- Building with ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION} ${CMAKE_SYSTEM_INFO_FILE}")

# Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  set(DOXYGEN "doxygen")
  if(NOT TARGET ${DOXYGEN})
    add_custom_command(OUTPUT "Doxyfile" COMMAND ${DOXYGEN_EXECUTABLE} "Doxyfile" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/documents")
    add_custom_target(${DOXYGEN} DEPENDS "Doxyfile")
    set_target_properties(${DOXYGEN} PROPERTIES FOLDER "doxygen")
  endif()
endif()

# Set Compiler Flags
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# Set warning & error flags
# Store origin compile flags
set(CMAKE_C_FLAGS_ORIGIN ${CMAKE_C_FLAGS})
set(CMAKE_CXX_FLAGS_ORIGIN ${CMAKE_CXX_FLAGS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
# Common compile flags
set(COMMON_COMPILE_FLAGS "")
# Pedantic compile flags
set(PEDANTIC_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -Wshadow -pedantic")


find_package(OpenSSL REQUIRED)
add_subdirectory(libs)
list(APPEND LINKLIBS ${OPENSSL_LIBRARIES})

# Libraries directories
include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/libs")

# Source code
file(GLOB_RECURSE LIB_HEADER_FILES "include/*.hxx")
file(GLOB_RECURSE LIB_SOURCE_FILES "source/*.cxx")
add_library(cxxserver ${LIB_HEADER_FILES} ${LIB_INLINE_FILES} ${LIB_SOURCE_FILES})
set_target_properties(cxxserver PROPERTIES COMPILE_FLAGS "${PEDANTIC_COMPILE_FLAGS}" FOLDER "libraries")
target_include_directories(cxxserver PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(cxxserver ${LINKLIBS} asio)

list(APPEND LINKLIBS cxxserver)

# Tests
file(GLOB TESTS_HEADER_FILES "tests/*.hxx")
file(GLOB TESTS_SOURCE_FILES "tests/*.cxx")
add_executable(tests ${TESTS_SOURCE_FILES} ${TESTS_HEADER_FILES} ${Catch2})
set_target_properties(tests PROPERTIES COMPILE_FLAGS "${PEDANTIC_COMPILE_FLAGS}" FOLDER "tests")
target_include_directories(tests PRIVATE ${Catch2})
target_link_libraries(tests ${LINKLIBS})
list(APPEND INSTALL_TARGETS tests)
list(APPEND INSTALL_TARGETS_PDB tests)

# CTest
enable_testing()
add_test(tests tests --durations yes --order lex)