# Create comprehensive CMakeLists.txt
cmake_content = """cmake_minimum_required(VERSION 3.15)
project(GarbledCircuitsUtility VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Add cmake module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Find libOTe
find_package(libOTe REQUIRED)

# Find Boost (required by libOTe)
find_package(Boost 1.75 REQUIRED COMPONENTS system filesystem thread)

# Source files for the library
set(GARBLED_CIRCUIT_SOURCES
    src/garbled_circuit.cpp
    src/crypto_utils.cpp
    src/socket_utils.cpp
    src/ot_handler.cpp
)

# Headers
set(GARBLED_CIRCUIT_HEADERS
    src/garbled_circuit.h
    src/crypto_utils.h
    src/socket_utils.h
    src/ot_handler.h
    include/common.h
)

# Create shared library
add_library(garbled_circuits SHARED ${GARBLED_CIRCUIT_SOURCES} ${GARBLED_CIRCUIT_HEADERS})

# Link libraries to the main library
target_link_libraries(garbled_circuits
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ${libOTe_LIBRARIES}
    ${Boost_LIBRARIES}
)

# Include directories for library
target_include_directories(garbled_circuits PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${OpenSSL_INCLUDE_DIRS}
    ${libOTe_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Garbler executable
add_executable(garbler src/garbler.cpp src/main.cpp)
target_link_libraries(garbler garbled_circuits)
target_compile_definitions(garbler PRIVATE GARBLER_MODE=1)

# Evaluator executable
add_executable(evaluator src/evaluator.cpp src/main.cpp)
target_link_libraries(evaluator garbled_circuits)
target_compile_definitions(evaluator PRIVATE EVALUATOR_MODE=1)

# Example executables
add_executable(simple_and examples/simple_and.cpp)
target_link_libraries(simple_and garbled_circuits)

add_executable(millionaires examples/millionaires.cpp)
target_link_libraries(millionaires garbled_circuits)

# Installation
install(TARGETS garbler evaluator garbled_circuits
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${GARBLED_CIRCUIT_HEADERS}
    DESTINATION include/garbled_circuits
)

# Testing (optional)
enable_testing()

# Simple test to verify build
add_test(NAME BuildTest COMMAND garbler --help)

# Print configuration summary
message(STATUS "==========================================")
message(STATUS "  ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "==========================================")
message(STATUS "  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "  CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "  OpenSSL_VERSION: ${OpenSSL_VERSION}")
message(STATUS "  Boost_VERSION: ${Boost_VERSION}")
if(libOTe_FOUND)
message(STATUS "  libOTe: Found")
else()
message(STATUS "  libOTe: NOT FOUND")
endif()
message(STATUS "==========================================")
"""

# Create CMakeLists.txt
with open("garbled_circuits_utility/CMakeLists.txt", "w") as f:
    f.write(cmake_content)

# Create build script
build_script = """#!/bin/bash

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
"""

with open("garbled_circuits_utility/build.sh", "w") as f:
    f.write(build_script)

print("CMakeLists.txt and build.sh created successfully!")