# Library Usage Notes

Below are detailed notes summarizing the key data structures and functions across the header files you’ll need to understand to use the library effectively.

---

## 1. Cryptographic Building Blocks

- **block**
  - A 128-bit (or fixed-size) data unit used for cryptographic operations and random message generation.
- **BitVector**
  - Provides an efficient container for bit-arrays, extensively used to represent choices in OT protocols.
- **PRNG (Pseudo-Random Number Generator)**
  - Generates cryptographic randomness.
  - Typically instantiated with a seed (given as a `block`) for reproducibility between communicating parties.

---

## 2. Base OT (Oblivious Transfer)

- **Header**: `libOTe/Base/BaseOT.h`

- Key Points:
  - Implements base OT protocols, often via classes such as `SimplestOT`.
  - One party (sender) prepares two messages per operation, while the receiver selects one message using a BitVector mask.
  - Base OTs provide the necessary groundwork (e.g., keys or correlated randomness) for OT Extension protocols.

---

## 3. Two-Choose-One OT Extension (IKNP Protocol)

- **Headers**:
  - `libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h`
  - `libOTe/TwoChooseOne/Iknp/IknpOtExtReceiver.h`

- **Sender Class Functions**:
  - `setBaseOts(...)`: Accepts the results from the base OT phase (vector of `block` values and a BitVector) for initialization.
  - `send(...)`: Takes a vector of message pairs (each pair is an array of two `block`s) and extends the base OT to a larger number of OTs.

- **Receiver Class Functions**:
  - `setBaseOts(...)`: Configures the receiver with the base OT data (typically a vector of `block`s produced by the sender).
  - `receive(...)`: Accepts a BitVector of choices and produces the corresponding OT messages using the synchronized PRNG.

- **Workflow**:
  1. Execute a base OT operation using the foundational protocol (e.g., `SimplestOT`).
  2. Use the base OT outputs to call the `setBaseOts` method.
  3. Expand to a larger number of OTs via the extension methods (`send` for sender, `receive` for receiver).

---

## 4. Extension Modules and Additional Protocol Variants

- **Other Modules Include**:
  - **NChooseOne**: For oblivious transfer where the receiver selects one out of N options.
  - **Dpf (Distributed Point Functions)**: Supports function secret sharing and distributed randomness.
  - **Vole (Vector OLE)**: Implements protocols for Oblivious Linear Evaluation used in secure computations.

- **Common Elements**:
  - All modules heavily rely on the core cryptographic primitives (`block`, `BitVector`, `PRNG`).

---

## 5. Integration and Workflow

- **Overall Workflow**:
  1. **Base OT Phase:** Execute base OT (using classes like `SimplestOT`).
  2. **Synchronization:** Ensure both parties (sender and receiver) use synchronized randomness (PRNG seeded appropriately).
  3. **OT Extension Initialization:** Call `setBaseOts` on the sender and receiver objects.
  4. **OT Expansion:** Use the extension functions (`send` for sender; `receive` for receiver) to generate the desired number of OTs.

- **Asynchronous Operations:**
  - Many functions return tasks that require synchronization (using helpers like `coproto::sync_wait`).

---

## Summary

To use the library effectively:

- Understand and utilize the basic data types: `block`, `BitVector`, and `PRNG`.
- Familiarize yourself with the base OT functions provided in `libOTe/Base/BaseOT.h` as they lay the foundation for secure communications.
- Study the IKNP OT extension functions in `libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h` and `IknpOtExtReceiver.h` to efficiently scale the OTs.
- Explore additional modules like NChooseOne, Dpf, and Vole based on your application's security requirements.

For more detailed information, review the inline documentation within the header files of each module.
