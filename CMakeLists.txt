
cmake_minimum_required(VERSION 3.22)
project(aifs)

find_package(fmt)
find_package(spdlog)
find_package(http_parser)

add_library(aifs STATIC src/event_loop.cpp src/http/response.cpp)
target_include_directories(aifs PUBLIC include)
target_link_libraries(aifs PUBLIC fmt::fmt spdlog::spdlog http_parser::http_parser)
target_compile_features(aifs PUBLIC cxx_std_20)

add_executable(http_server_example examples/http_server.cpp)
target_link_libraries(http_server_example PRIVATE aifs)
