# Garbled Circuits Utility

A simple C++ implementation of Yao's Garbled Circuits protocol for secure two-party computation. This project allows two parties to jointly compute a function while keeping their inputs private.

## Features

- Basic implementation of Yao's Garbled Circuits
- Socket-based communication between garbler and evaluator
- Integration with libOTe library for oblivious transfers
- Support for NAND-based boolean circuits
- Simple examples (AND gate, Millionaire's problem)

## Project Structure

```
garbled_circuits_utility/
├── src/                     # Source files
│   ├── garbler.cpp         # Garbler implementation
│   ├── evaluator.cpp       # Evaluator implementation
│   ├── garbled_circuit.cpp # Core garbling logic
│   ├── garbled_circuit.h   # Circuit definitions
│   ├── ot_handler.cpp      # Oblivious transfer wrapper
│   ├── ot_handler.h        # OT interface
│   ├── crypto_utils.cpp    # Cryptographic utilities
│   ├── crypto_utils.h      # Crypto headers
│   ├── socket_utils.cpp    # Network communication
│   ├── socket_utils.h      # Socket headers
│   └── main.cpp           # Main entry point
├── include/                # Header files
│   └── common.h           # Common definitions
├── examples/              # Example programs
│   ├── simple_and.cpp     # Simple AND gate example
│   └── millionaires.cpp   # Millionaire's problem
├── cmake/                 # CMake modules
│   └── FindLibOTe.cmake   # LibOTe finder
├── CMakeLists.txt         # Build configuration
├── build.sh               # Build script
├── README.md              # This file
└── .gitignore            # Git ignore rules
```

## Dependencies

- **C++ Compiler**: GCC 7+ or Clang 11+ with C++17 support
- **CMake**: Version 3.15 or higher
- **libOTe**: For oblivious transfer operations
- **OpenSSL**: For cryptographic functions
- **Boost**: Required by libOTe (version 1.75+)

## Installation

### 1. Install Dependencies

#### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install build-essential cmake git libssl-dev libboost-dev
```

#### macOS:
```bash
brew install cmake openssl boost
```

### 2. Install libOTe

```bash
# Clone and build libOTe
git clone https://github.com/osu-crypto/libOTe.git
cd libOTe
python3 build.py --setup
cd ..
```

### 3. Build the Project

```bash
# Clone this project
cd garbled_circuits_utility

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release -DlibOTe_DIR=/path/to/libOTe

# Build
make -j4

# Or use the build script
cd ..
chmod +x build.sh
./build.sh
```

## Usage

### Basic Usage

The utility provides two executables: one for the garbler and one for the evaluator.

#### 1. Start the Garbler (Server)
```bash
./build/garbler --port 8080 --circuit examples/simple_and.txt --input 1
```

#### 2. Start the Evaluator (Client)
```bash
./build/evaluator --host localhost --port 8080 --input 0
```

### Command Line Options

#### Garbler Options:
- `--port <port>`: Port to listen on (default: 8080)
- `--circuit <file>`: Circuit description file
- `--input <bits>`: Garbler's input bits
- `--help`: Show help message

#### Evaluator Options:
- `--host <hostname>`: Garbler's hostname (default: localhost)
- `--port <port>`: Port to connect to (default: 8080)
- `--input <bits>`: Evaluator's input bits
- `--help`: Show help message

### Circuit Format

Circuits are defined in a simple text format:

```
# Simple AND gate circuit
INPUTS 2      # Number of input wires
OUTPUTS 1     # Number of output wires
GATES 1       # Number of gates

# Gate format: output_wire input1_wire input2_wire GATE_TYPE
GATE 3 1 2 AND
```

## Examples

### Example 1: Simple AND Gate

```bash
# Terminal 1 (Garbler)
./build/garbler --port 8080 --circuit examples/simple_and.txt --input 1

# Terminal 2 (Evaluator)  
./build/evaluator --host localhost --port 8080 --input 0

# Output: 0 (1 AND 0 = 0)
```

### Example 2: Millionaire's Problem

```bash
# Terminal 1 (Alice with wealth = 50)
./build/garbler --port 8080 --circuit examples/millionaires.txt --input 50

# Terminal 2 (Bob with wealth = 75)
./build/evaluator --host localhost --port 8080 --input 75

# Output: 0 (Alice is not richer than Bob)
```

## Implementation Details

### Security Model
- **Semi-honest adversaries**: Parties follow the protocol but may try to learn additional information
- **No optimizations**: This is a basic implementation without Free XOR, point-and-permute, or other optimizations
- **Educational purpose**: Suitable for learning and understanding the protocol

### Protocol Flow
1. **Circuit Generation**: Garbler creates the boolean circuit
2. **Garbling**: Each gate is encrypted using random labels
3. **OT Phase**: Evaluator obtains garbled inputs via oblivious transfer
4. **Evaluation**: Evaluator computes the garbled circuit
5. **Output**: Result is revealed to both parties

### Cryptographic Primitives
- **PRF**: SHA-256 based pseudorandom function
- **Encryption**: AES-based symmetric encryption
- **OT**: libOTe implementation of oblivious transfer

## Building from Source

### Debug Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
```

### Release Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

### Testing
```bash
# Run built-in tests
cd build
make test

# Or run specific test
./test_garbled_circuit
```

## Troubleshooting

### Common Issues

1. **libOTe not found**
   - Ensure libOTe is properly installed
   - Set the correct path in CMake: `-DlibOTe_DIR=/path/to/libOTe`

2. **Boost version too old**
   - Install Boost 1.75 or newer
   - Use the libOTe build script to install locally

3. **Connection refused**
   - Ensure garbler is running before starting evaluator
   - Check firewall settings for the specified port

4. **Compilation errors**
   - Verify C++17 compiler support
   - Check all dependencies are installed

### Debug Mode

Enable debug output:
```bash
export GC_DEBUG=1
./build/garbler --port 8080 --circuit examples/simple_and.txt --input 1 --verbose
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## License

This project is for educational purposes. See the individual dependencies for their respective licenses.

## References

- Yao, A. C. (1986). How to generate and exchange secrets
- Bellare, M., Hoang, V. T., & Rogaway, P. (2012). Foundations of garbled circuits
- libOTe: https://github.com/osu-crypto/libOTe

## Support

For questions and issues, please open a GitHub issue or contact the maintainer.
