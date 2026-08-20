# Tiny downstream consumer used by packaging smoke tests.
# Configures against an installed standalone prefix:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
cmake_minimum_required(VERSION 3.14)
project(Cog0FindPackageConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Cog0 REQUIRED CONFIG)
find_package(Threads REQUIRED)
# OpenSSL is pulled transitively when Cog0 was built with TLS; find it explicitly
# so generators that do not re-export find_dependency still link cleanly.
find_package(OpenSSL QUIET)

add_executable(cog0_consumer main.cpp)
target_link_libraries(cog0_consumer PRIVATE Cog0::cog0lib Threads::Threads)
if(OpenSSL_FOUND OR OPENSSL_FOUND)
    target_link_libraries(cog0_consumer PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

enable_testing()
add_test(NAME cog0_consumer_runs COMMAND cog0_consumer)
