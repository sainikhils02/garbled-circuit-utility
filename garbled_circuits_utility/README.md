# Garbled Circuits Utility

A simple C++ implementation of Yao's Garbled Circuits protocol for secure two‑party computation. Two parties jointly compute a function while keeping their inputs private. This repo includes a text‑based circuit format and a working OT integration using libOTe’s SimplestOT over coproto (Boost.Asio).

## Features

- Basic implementation of Yao's Garbled Circuits
- Socket-based communication between garbler and evaluator
- Oblivious Transfer via libOTe SimplestOT using coproto (Boost.Asio)
- Supported gates: AND, OR, XOR, NAND, NOT 
- Example circuits (AND gate, 4‑bit Millionaire’s simplified comparator)

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
│   └── main.cpp            # (if present) main entry point
├── include/                # Header files
│   └── common.h           # Common definitions
├── examples/               # Example circuits (text format)
│   ├── simple_and.txt      # 2‑input AND
│   ├── millionaires_4bit.txt# 4‑bit (A>=B) comparator
│   └── ...
├── cmake/                 # CMake modules
│   └── FindLibOTe.cmake   # LibOTe finder
├── CMakeLists.txt         # Build configuration
├── build.sh               # Build script
├── README.md              # This file
└── .gitignore            # Git ignore rules
```

## Dependencies

- C++17 compiler (GCC 9+/Clang 11+ recommended)
- CMake 3.15+
- OpenSSL (dev headers)
- Boost (dev headers; 1.75+)
- Python 3 (for libOTe build script)
- libOTe (SimplestOT)
- coproto with Boost enabled (for Asio sockets)

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

### 2. Install libOTe and coproto

You can keep the following sibling layout (as used by this repo):

```
<workspace>/
   garbled_circuits_utility/
   libOTe/
   coproto/
```

Clone the dependencies:

```bash
git clone https://github.com/osu-crypto/libOTe.git
git clone https://github.com/ladnir/coproto.git
```

Build coproto with Boost.Asio enabled:

```bash
cd coproto
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCOPROTO_ENABLE_BOOST=ON
cmake --build build -j
cd ..
```

Build libOTe (ensures coproto/Boost is available):

```bash
cd libOTe
python3 build.py --setup
# If needed, force Boost/coproto (varies by environment)
python3 build.py -DCOPROTO_ENABLE_BOOST=ON
cd ..
```

Locate libOTeConfig.cmake for CMake. Common locations after the script runs:

- libOTe/build/out/install/lib/cmake/libOTe/libOTeConfig.cmake
- or inside the build tree under a lib/cmake folder

### 3. Build the project

```bash
# Clone this project
cd garbled_circuits_utility

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release -DlibOTe_DIR=/absolute/path/to/libOTeConfigDir

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

Environment variable (optional but recommended):

- `GC_OT_ENDPOINT` — host:port used by SimplestOT’s secondary Asio channel.
   - Default: `127.0.0.1:9100`
   - The garbler (OT sender) listens on this endpoint; the evaluator connects to it.

Example:

```bash
export GC_OT_ENDPOINT=127.0.0.1:9100
```

#### 1. Start the Garbler (server)
```bash
./build/garbler --port 8080 --circuit examples/simple_and.txt --input 1
```

#### 2. Start the Evaluator (client)
```bash
./build/evaluator --host localhost --port 8080 --input 0
```

### Command line options

Garbler:
- `--port <port>`: Port to listen on (default: 8080)
- `--circuit <file>`: Circuit description file (text format)
- `--input <bits>`: Garbler’s input bits (e.g., `1011`)

Evaluator:
- `--host <hostname>`: Garbler hostname (default: `localhost`)
- `--port <port>`: Port to connect to (default: 8080)
- `--input <bits>`: Evaluator’s input bits

### Circuit format (Bristol fashion)

Circuits use the standard Bristol Fashion text format:

```
# Comments start with '#'

<gate_count> <wire_count>
<inputs_party0> [inputs_party1 ...]
<outputs_party0> [outputs_party1 ...]

<in_arity> <out_arity> <in wires...> <out wires...> <GATE>
```

- Wires are zero-indexed and must be within `[0, wire_count)`. The input lines assign wires to each party in order (party 0 first).
- Supported gate tokens: `AND`, `OR`, `XOR`, `NAND`, `NOR`, and `INV` (logical NOT). Only unary and binary gates are currently accepted.
- The final `<sum(outputs)>` wires (highest indices) are interpreted as circuit outputs.

Example – one AND gate where each party supplies one bit:

```
1 3
1 1
1
2 1 0 1 2 AND
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

### Example 2: Millionaire's Problem (4‑bit simplified)

```bash
# Terminal 1 (Alice with wealth = 50)
./build/garbler --port 8080 --circuit examples/millionaires_4bit.txt --input 0101

# Terminal 2 (Bob with wealth = 75)
./build/evaluator --host localhost --port 8080 --input 0011

# Output: 0 (Alice is not richer than Bob)
```

## Implementation details

### Security Model
- Semi‑honest adversaries 
- No optimizations like Free XOR/point‑and‑permute 

### Protocol Flow
1. Circuit generation: garbler loads a text circuit
2. Garbling: each gate produces 4 ciphertexts (NOT uses 2 real + 2 dummy), with integrity padding
3. OT phase: evaluator obtains input labels via libOTe SimplestOT over coproto Asio (secondary socket)
4. Evaluation: evaluator tries decryptions and forwards output labels
5. Output: garbler decodes final bits

### Cryptographic Primitives
- PRF: SHA‑256 based on both input labels and gate id
- Encryption: AES‑128‑ECB without PKCS padding; appends 16‑byte zero padding for integrity check
- OT: libOTe SimplestOT; labels masked via SHA‑256 KDF of OT blocks

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
Minimal smoke test (two terminals):

```bash
# Terminal 1
export GC_OT_ENDPOINT=127.0.0.1:9100
./build/garbler --port 8080 --circuit examples/simple_and.txt --input 1

# Terminal 2
export GC_OT_ENDPOINT=127.0.0.1:9100
./build/evaluator --host localhost --port 8080 --input 0
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

### Tips & troubleshooting

Common issues:

1) libOTe not found
- Make sure `-DlibOTe_DIR` points to the directory that contains `libOTeConfig.cmake`.

2) coproto/Boost missing
- Build coproto with `-DCOPROTO_ENABLE_BOOST=ON`.
- Ensure Boost dev packages are installed.

3) OT connection errors (hangs or refused)
- Set the same `GC_OT_ENDPOINT` on both sides (default 127.0.0.1:9100).
- Ensure the port is free and both processes can connect.


