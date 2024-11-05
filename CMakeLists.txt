
cmake_minimum_required(VERSION 3.22)
project(aifs)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_BINARY_DIR}")

find_package(fmt 9.1.0 REQUIRED)
find_package(spdlog 1.11.0 REQUIRED)
find_package(http_parser 2.9.4 REQUIRED)

add_library(aifs STATIC src/event_loop.cpp include/aifs/tcp_socket.h include/aifs/tcp_acceptor.h include/aifs/socket_ops.h include/aifs/operation.h include/aifs/descriptor.h)
target_include_directories(aifs PUBLIC include)
target_link_libraries(aifs PUBLIC fmt::fmt spdlog::spdlog http_parser::http_parser)
target_compile_features(aifs PUBLIC cxx_std_20)

add_executable(example1 examples/example1.cpp)
target_link_libraries(example1 PRIVATE aifs)
