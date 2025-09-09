# Create FindLibOTe.cmake
find_libote_cmake = """# FindLibOTe.cmake - CMake module to find libOTe library
#
# This module defines:
#  libOTe_FOUND - True if libOTe is found
#  libOTe_INCLUDE_DIRS - Include directories for libOTe
#  libOTe_LIBRARIES - Libraries to link against
#  libOTe_DEFINITIONS - Compiler definitions required by libOTe

# Look for the header files
find_path(libOTe_INCLUDE_DIR
    NAMES libOTe/libOTe.h libOTe/Tools/Tools.h
    PATHS
        ${libOTe_ROOT}
        ${libOTe_ROOT}/include
        $ENV{libOTe_ROOT}
        $ENV{libOTe_ROOT}/include
        /usr/local/include
        /usr/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libOTe
        ${CMAKE_CURRENT_SOURCE_DIR}/../libOTe
    DOC "libOTe include directory"
)

# Look for the library
find_library(libOTe_LIBRARY
    NAMES libOTe ote
    PATHS
        ${libOTe_ROOT}
        ${libOTe_ROOT}/lib
        ${libOTe_ROOT}/build
        $ENV{libOTe_ROOT}
        $ENV{libOTe_ROOT}/lib  
        $ENV{libOTe_ROOT}/build
        /usr/local/lib
        /usr/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/libOTe/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/../libOTe/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/libOTe/build
        ${CMAKE_CURRENT_SOURCE_DIR}/../libOTe/build
    DOC "libOTe library"
)

# Look for additional libraries that libOTe might depend on
find_library(libOTe_cryptoTools_LIBRARY
    NAMES cryptoTools
    PATHS
        ${libOTe_ROOT}/lib
        ${libOTe_ROOT}/build
        $ENV{libOTe_ROOT}/lib
        $ENV{libOTe_ROOT}/build
        /usr/local/lib
        /usr/lib
    DOC "cryptoTools library (libOTe dependency)"
)

# Handle standard arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libOTe
    REQUIRED_VARS libOTe_LIBRARY libOTe_INCLUDE_DIR
    FOUND_VAR libOTe_FOUND
)

if(libOTe_FOUND)
    set(libOTe_INCLUDE_DIRS ${libOTe_INCLUDE_DIR})
    set(libOTe_LIBRARIES ${libOTe_LIBRARY})
    
    # Add cryptoTools if found
    if(libOTe_cryptoTools_LIBRARY)
        list(APPEND libOTe_LIBRARIES ${libOTe_cryptoTools_LIBRARY})
    endif()
    
    # Set compiler definitions
    set(libOTe_DEFINITIONS "")
    
    # Create imported target
    if(NOT TARGET libOTe::libOTe)
        add_library(libOTe::libOTe UNKNOWN IMPORTED)
        set_target_properties(libOTe::libOTe PROPERTIES
            IMPORTED_LOCATION ${libOTe_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${libOTe_INCLUDE_DIR}
        )
        
        # Link cryptoTools if available
        if(libOTe_cryptoTools_LIBRARY)
            set_target_properties(libOTe::libOTe PROPERTIES
                IMPORTED_LINK_INTERFACE_LIBRARIES ${libOTe_cryptoTools_LIBRARY}
            )
        endif()
    endif()
    
    message(STATUS "Found libOTe:")
    message(STATUS "  Include directory: ${libOTe_INCLUDE_DIR}")
    message(STATUS "  Library: ${libOTe_LIBRARY}")
    if(libOTe_cryptoTools_LIBRARY)
        message(STATUS "  CryptoTools library: ${libOTe_cryptoTools_LIBRARY}")
    endif()
else()
    message(STATUS "libOTe not found. The project will use fallback OT implementations.")
    message(STATUS "To use libOTe, please:")
    message(STATUS "  1. Install libOTe: git clone https://github.com/osu-crypto/libOTe.git")
    message(STATUS "  2. Build libOTe: cd libOTe && python3 build.py --setup")
    message(STATUS "  3. Set libOTe_ROOT to the libOTe directory")
    message(STATUS "     e.g., cmake .. -DlibOTe_ROOT=/path/to/libOTe")
endif()

# Mark variables as advanced so they don't show up in basic cmake GUIs
mark_as_advanced(
    libOTe_INCLUDE_DIR
    libOTe_LIBRARY
    libOTe_cryptoTools_LIBRARY
)
"""

with open("garbled_circuits_utility/cmake/FindLibOTe.cmake", "w") as f:
    f.write(find_libote_cmake)

# Create .gitignore
gitignore_content = """# Build directories
build/
cmake-build-*/
out/

# Compiled binaries
*.exe
*.out
*.app
garbler
evaluator
simple_and
millionaires

# Object files
*.o
*.obj
*.lo
*.slo

# Libraries
*.lib
*.a
*.la
*.so
*.so.*
*.dylib
*.dll

# CMake generated files
CMakeCache.txt
CMakeFiles/
CMakeScripts/
cmake_install.cmake
install_manifest.txt
compile_commands.json
CTestTestfile.cmake
_deps/

# IDE files
.vscode/
.idea/
*.vcxproj*
*.sln
*.suo
*.user

# Temporary files
*~
*.swp
*.swo
.DS_Store
Thumbs.db

# Generated circuit files
*.txt
!examples/*.txt

# Log files
*.log

# Core dumps
core
core.*

# libOTe (if cloned as submodule)
libOTe/

# Dependencies
thirdparty/
third_party/
deps/

# Package managers
vcpkg/
conan/

# Backup files
*.bak
*.backup
*.orig

# Test outputs
test_results/
coverage/

# Documentation builds
docs/build/
docs/_build/

# Python bytecode (if using Python build scripts)
__pycache__/
*.pyc
*.pyo
*.pyd
.Python

# Virtual environments
venv/
env/
ENV/

# Local configuration
.env
.env.local
local_config.h

# Profiler output
*.prof
gmon.out
callgrind.out.*

# Valgrind output
valgrind-*.log

# Address sanitizer output
*.asan

# Generated protocol buffers or other generated code
*_pb2.py
*_pb2_grpc.py
*.pb.cc
*.pb.h

# OS specific
.fuse_hidden*
.nfs*
"""

with open("garbled_circuits_utility/.gitignore", "w") as f:
    f.write(gitignore_content)

print("FindLibOTe.cmake and .gitignore created successfully!")
print(f"FindLibOTe.cmake length: {len(find_libote_cmake)} characters")
print(f".gitignore length: {len(gitignore_content)} characters")