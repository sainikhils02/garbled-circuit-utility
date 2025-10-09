cmake_minimum_required(VERSION 3.15)
project(OTExample)

# Change to C++20 to support std::span
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler-specific flags for better compatibility
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

# Find libOTe package
find_package(libOTe 2.2.0 CONFIG REQUIRED)

# Create executable
add_executable(iknp_ot_example iknp_ot_example.cpp)

# Link against libOTe
target_link_libraries(iknp_ot_example oc::libOTe)

# SimplestOT base OT standalone example
add_executable(simplest_ot_example simplest_ot_example.cpp)
target_link_libraries(simplest_ot_example oc::libOTe)

# Print debug information about libOTe
message(STATUS "Found libOTe at: ${libOTe_DIR}")
message(STATUS "libOTe version: ${libOTe_VERSION}")