<img src="https://r2cdn.perplexity.ai/pplx-full-logo-primary-dark%402x.png" style="height:64px;margin-right:32px"/>

# Create md file for each header file explaining the important data structures and functions in them

Here is a Markdown documentation for each provided header file, summarizing the **important data structures** and **functions** to help you learn and use libOTe efficiently.

***

## block.h

The `block.h` file defines the core `block` structure for representing 128-bit data units used in cryptographic operations, optimized for SIMD/SSE/ARM instructions.

### Namespace

- **osuCrypto**


### Important Data Structures

- **block**
    - 128-bit data type.
    - Aligned to 16 bytes.
    - Storage depends on architecture: SSE (`__m128i`), ARM (`uint8x16_t`), or generic (`uint64_t`).[^1]
    - Used extensively in cryptographic primitives and OT protocols.


### Key Constructors

- `block()` - Default constructor.
- `block(uint64_t x1, uint64_t x0)` - Initializes from two 64-bit words.
- `block(uint32_t x3, x2, x1, x0)` - Initializes from four 32-bit words.
- `block(char e15, ..., char e0)` - Initializes from 16 bytes.
- `explicit block(uint64_t x)` - Initializes with one 64-bit word.


### Member Functions

- `unsigned char* data()` / `const unsigned char* data() const`
    - Returns pointer to underlying bytes.
- `get(), get(index), set(index, value)`
    - Obtain or set components of the block (templated for trivial types).
- `static block allSame<T>(val)`
    - Fill block with repeated values (specialized for different types).
- Bitwise and arithmetic operators:
    - `operator^, operator&, operator|, operator<<, operator>>`
    - Compound assignment (`^=, &=, |=, +=, -=`)
    - Comparison (`==, !=, <, >, <=, >=`)
    - Arithmetic (`add_epi64, sub_epi64`, etc.)
- GF(2^128) arithmetic:
    - `gf128Mul, gf128Reduce, gf128Pow`
- Conditional move and swap:
    - `cmov, cmovBytes, cswap, cswapBytes`
- Zero/One/AllOnes constants:
    - `extern const block ZeroBlock, OneBlock, AllOneBlock`


### Hash Specialization

- Specializes `std::hash<osuCrypto::block>` for use as a hash key.

***

## BaseOT.h

This file provides a mechanism for selecting the default base OT implementation at compile-time, abstracting several possible protocols.

### Namespace

- **osuCrypto**


### Important Type Aliases

- **DefaultBaseOT**
    - Alias to the compiled-in base OT protocol, chosen depending on compile flags:
        - `AsmSimplestOT`, `McRosRoyTwist`, `MasnyRindal`, `McRosRoy`, `MasnyRindalKyber`, `SimplestOT`, `INSECURE_MOCK_OT`
    - `LIBOTE_HAS_BASE_OT` macro indicates availability.


### Usage

- Include and use `DefaultBaseOT` for the recommended protocol depending on platform/dependencies.
- No direct data structure or method definitions—the content is selection logic via macros.

***

## IknpOtExtSender.h

Implements the sender half of the IKNP (1-out-of-2 semi-honest OT extension) protocol.

### Namespace

- **osuCrypto**


### Important Classes

- **IknpOtExtSender** (inherits KosOtExtSender)
    - Represents the sender for the IKNP OT extension protocol.
    - Manages base OTs, extension operation, security setting (semi-honest/malicious).


### Core Functions

- Constructors:
    - `IknpOtExtSender()`
        - Default, initializes in semi-honest mode.
    - `IknpOtExtSender(span baseRecvOts, const BitVector& choices)`
        - Setup with receiver base OTs and choice bits.
- Move/copy assignment/delete semantics as needed.
- `~IknpOtExtSender()`
    - Virtual destructor.
- **splitBase**
    - Returns an independent instance (split from current base OTs) for concurrent use.


### Usage Notes

- Used for performing sender-side OT extension.
- To use: set base OTs, then perform extension and send to receiver.

***

## IknpOtExtReceiver.h

Implements the receiver half of the IKNP (1-out-of-2 semi-honest OT extension) protocol.

### Namespace

- **osuCrypto**


### Important Classes

- **IknpOtExtReceiver** (inherits KosOtExtReceiver)
    - Represents the receiver for the IKNP OT extension protocol.
    - Manages base OT senders, extension, security setting.


### Core Functions

- Constructors:
    - `IknpOtExtReceiver()`
        - Default, initializes in semi-honest mode.
    - `IknpOtExtReceiver(span baseSendOts)`
        - Setup with sender base OTs.
- Move/copy assignment/delete semantics as needed.
- `~IknpOtExtReceiver()`
    - Virtual destructor.
- **splitBase**
    - Returns an independent instance (split from current base OTs) for concurrent use.


### Usage Notes

- Used for performing receiver-side OT extension.
- To use: set base OTs, then perform extension and receive messages.

***

**These documentation files will help you navigate and use the most central components of libOTe as defined by their header files. For protocol-specific usage, refer to protocol-specific sender/receiver classes and manage base OTs and extension as shown.**
<span style="display:none">[^2][^3][^4]</span>

<div style="text-align: center">⁂</div>

[^1]: BaseOT.h

[^2]: block.h

[^3]: IknpOtExtReceiver.h

[^4]: IknpOtExtSender.h

