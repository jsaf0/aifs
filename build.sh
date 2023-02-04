#!/bin/bash

mkdir build && cd build
conan install .. -pr:b=default -s build_type=Debug
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug
ninja
