#!/bin/bash

# For macOS, set CC and CXX to gcc-12 and g++-12 respectively.
# Also remember to update the default Conan profile.

mkdir build && cd build
conan install .. -pr:b=default --build=missing -s build_type=Debug
cmake .. -GNinja -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12 
-DCMAKE_BUILD_TYPE=Debug
ninja
