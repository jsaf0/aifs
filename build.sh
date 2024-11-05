#!/bin/bash

set -e

if conan --version | grep -v "Conan version 2."; then
	echo "Needs Conan 2.x"
	exit 1
fi

if [ -d ./build ]; then
	rm -rf build
fi

conan install . --output-folder=build -pr:b=default --build=missing -s build_type=Debug

cmake --preset conan-debug
cmake --build --preset conan-debug
