#!/bin/bash

# Test script for garbled circuits
echo "Garbled Circuits Test Suite"
echo "=========================="

# Build the project first
if [ ! -f "build/garbler" ]; then
    echo "Building project..."
    mkdir -p build
    cd build
    cmake ..
    make -j4
    cd ..
fi

echo ""
echo "Testing simple circuits..."

# Test AND gate
echo "1. Testing AND gate (1 AND 0 = 0):"
timeout 10 ./build/garbler -c examples/simple_and.txt -i 1 -p 8081 &
GARBLER_PID=$!
sleep 1
timeout 5 ./build/evaluator -i 0 -p 8081
wait $GARBLER_PID

echo ""
echo "2. Testing OR gate (1 OR 0 = 1):"
timeout 10 ./build/garbler -c examples/simple_or.txt -i 1 -p 8082 &
GARBLER_PID=$!
sleep 1
timeout 5 ./build/evaluator -i 0 -p 8082
wait $GARBLER_PID

echo ""
echo "3. Testing XOR gate (1 XOR 0 = 1):"
timeout 10 ./build/garbler -c examples/simple_xor.txt -i 1 -p 8083 &
GARBLER_PID=$!
sleep 1
timeout 5 ./build/evaluator -i 0 -p 8083
wait $GARBLER_PID

echo ""
echo "All tests completed!"
