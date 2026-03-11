#!/bin/bash


# Build the Ason C++ library
cmake --build ason-cpp/build -j4

mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)