#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <map>
#include <random>
#include <cassert>

// Security parameter (key length in bits)
constexpr size_t SECURITY_PARAM = 128;
constexpr size_t WIRE_LABEL_SIZE = SECURITY_PARAM / 8; // 16 bytes

// Network constants
constexpr int DEFAULT_PORT = 8080;
constexpr int MAX_MESSAGE_SIZE = 65536;
constexpr int SOCKET_TIMEOUT = 30; // seconds

// Gate types
enum class GateType {
    AND = 0,
    OR = 1,
    XOR = 2,
    NAND = 3,
    NOR = 4,
    NOT = 5,
    INPUT = 6,
    OUTPUT = 7
};

// Wire label type - 128-bit label
using WireLabel = std::array<uint8_t, WIRE_LABEL_SIZE>;

// Gate structure
struct Gate {
    int output_wire;
    int input_wire1;
    int input_wire2;  // -1 for unary gates like NOT
    GateType type;
    
    Gate(int out, int in1, int in2, GateType t) 
        : output_wire(out), input_wire1(in1), input_wire2(in2), type(t) {}
    
    Gate(int out, int in1, GateType t) 
        : output_wire(out), input_wire1(in1), input_wire2(-1), type(t) {}
};

// Circuit structure
struct Circuit {
    int num_inputs;
    int num_outputs; 
    int num_gates;
    int num_wires;
    std::vector<Gate> gates;
    std::vector<int> input_wires;
    std::vector<int> output_wires;
    std::vector<int> input_partition;
    std::vector<int> output_partition;
    
    Circuit() : num_inputs(0), num_outputs(0), num_gates(0), num_wires(0) {}
};

// Garbled gate structure (4 ciphertexts)
struct GarbledGate {
    std::array<std::vector<uint8_t>, 4> ciphertexts;
    
    GarbledGate() {
        for (auto& ct : ciphertexts) {
            ct.resize(WIRE_LABEL_SIZE + 16); // Label + padding for verification
        }
    }
};

// Garbled circuit structure  
struct GarbledCircuit {
    Circuit circuit;
    std::vector<GarbledGate> garbled_gates;
    std::map<int, std::pair<WireLabel, WireLabel>> input_labels; // wire_id -> (label0, label1)
    std::map<int, WireLabel> output_mapping; // For output decoding
    
    GarbledCircuit() = default;
    GarbledCircuit(const Circuit& c) : circuit(c) {
        garbled_gates.resize(c.gates.size());
    }
};

// Network message types
enum class MessageType : uint8_t {
    HELLO = 0,
    CIRCUIT = 1,
    INPUT_LABELS = 2,
    OT_REQUEST = 3,
    OT_RESPONSE = 4,
    RESULT = 5,
    ERROR = 6,
    GOODBYE = 7
};

// Network message structure
struct Message {
    MessageType type;
    uint32_t size;
    std::vector<uint8_t> data;
    
    Message() : type(MessageType::HELLO), size(0) {}
    Message(MessageType t, const std::vector<uint8_t>& d) : type(t), size(d.size()), data(d) {}
};

// Utility functions
inline std::string gate_type_to_string(GateType type) {
    switch (type) {
        case GateType::AND: return "AND";
        case GateType::OR: return "OR"; 
        case GateType::XOR: return "XOR";
        case GateType::NAND: return "NAND";
        case GateType::NOR: return "NOR";
        case GateType::NOT: return "NOT";
        case GateType::INPUT: return "INPUT";
        case GateType::OUTPUT: return "OUTPUT";
        default: return "UNKNOWN";
    }
}

inline GateType string_to_gate_type(const std::string& str) {
    if (str == "AND") return GateType::AND;
    if (str == "OR") return GateType::OR;
    if (str == "XOR") return GateType::XOR; 
    if (str == "NAND") return GateType::NAND;
    if (str == "NOR") return GateType::NOR;
    if (str == "INV") return GateType::NOT;
    if (str == "NOT") return GateType::NOT;
    if (str == "INPUT") return GateType::INPUT;
    if (str == "OUTPUT") return GateType::OUTPUT;
    throw std::runtime_error("Unknown gate type: " + str);
}

inline bool gate_function(GateType type, bool a, bool b = false) {
    switch (type) {
        case GateType::AND: return a && b;
        case GateType::OR: return a || b;
        case GateType::XOR: return a ^ b;
        case GateType::NAND: return !(a && b);
        case GateType::NOR: return !(a || b);
        case GateType::NOT: return !a;
        default: 
            throw std::runtime_error("Invalid gate type for evaluation");
    }
}

// Debug and logging macros
#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << "[DEBUG] " << x << std::endl
#else  
#define DEBUG_PRINT(x)
#endif

#define LOG_INFO(x) std::cout << "[INFO] " << x << std::endl
#define LOG_ERROR(x) std::cerr << "[ERROR] " << x << std::endl
#define LOG_WARNING(x) std::cerr << "[WARNING] " << x << std::endl

// Exception classes
class GarblerException : public std::exception {
private:
    std::string message;
public:
    GarblerException(const std::string& msg) : message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
};

class EvaluatorException : public std::exception {
private:
    std::string message;
public:
    EvaluatorException(const std::string& msg) : message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
};

class NetworkException : public std::exception {
private:
    std::string message;
public:
    NetworkException(const std::string& msg) : message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
};

class CryptoException : public std::exception {
private:
    std::string message;
public:
    CryptoException(const std::string& msg) : message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
};
