#!/bin/bash

# Build script for Garbled Circuits Utility
set -e

echo "============================================"
echo "Building Garbled Circuits Utility"
echo "============================================"

# Create build directory
if [ -d "build" ]; then
    echo "Removing existing build directory..."
    rm -rf build
fi

mkdir build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building project..."
make -j$(nproc)

echo "============================================"
echo "Build completed successfully!"
echo "============================================"

echo "Executables built:"
echo "  - garbler"
echo "  - evaluator" 
echo "  - simple_and (example)"
echo "  - millionaires (example)"

echo ""
echo "To run:"
echo "  Terminal 1: ./garbler --port 8080 --circuit ../examples/simple_and.txt --input 1"
echo "  Terminal 2: ./evaluator --host localhost --port 8080 --input 0"
