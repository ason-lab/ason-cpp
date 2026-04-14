#!/bin/bash


# Build the Asun C++ library
cmake --build asun-cpp/build -j4

mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)