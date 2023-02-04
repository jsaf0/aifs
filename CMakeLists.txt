
cmake_minimum_required(VERSION 3.22)
project(aifs)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_BINARY_DIR}")

find_package(fmt REQUIRED)

add_library(aifs STATIC src/event_loop.cpp)
target_include_directories(aifs PUBLIC include)
target_link_libraries(aifs PUBLIC fmt::fmt)
target_compile_features(aifs PUBLIC cxx_std_20)

add_executable(example1 examples/example1.cpp)
target_link_libraries(example1 PRIVATE aifs)
