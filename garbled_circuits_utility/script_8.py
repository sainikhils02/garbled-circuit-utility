# Create garbled_circuit.cpp - this is a large file, so I'll split it into parts
garbled_circuit_cpp_part1 = """#include "garbled_circuit.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <iomanip>

// GarbledCircuitManager implementation
GarbledCircuitManager::GarbledCircuitManager() {
}

Circuit GarbledCircuitManager::load_circuit_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open circuit file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return parse_circuit(buffer.str());
}

Circuit GarbledCircuitManager::parse_circuit(const std::string& circuit_description) {
    Circuit circuit;
    std::istringstream iss(circuit_description);
    std::string line;
    
    while (std::getline(iss, line)) {
        line = trim_string(line);
        if (line.empty() || line[0] == '#') {
            continue; // Skip empty lines and comments
        }
        
        parse_circuit_line(line, circuit);
    }
    
    // Validate the parsed circuit
    if (!validate_circuit(circuit)) {
        throw std::runtime_error("Invalid circuit structure");
    }
    
    return circuit;
}

void GarbledCircuitManager::parse_circuit_line(const std::string& line, Circuit& circuit) {
    auto tokens = split_string(line, ' ');
    if (tokens.empty()) return;
    
    std::string command = tokens[0];
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);
    
    if (command == "INPUTS") {
        if (tokens.size() != 2) {
            throw std::runtime_error("Invalid INPUTS line format");
        }
        circuit.num_inputs = std::stoi(tokens[1]);
        for (int i = 1; i <= circuit.num_inputs; ++i) {
            circuit.input_wires.push_back(i);
        }
    }
    else if (command == "OUTPUTS") {
        if (tokens.size() != 2) {
            throw std::runtime_error("Invalid OUTPUTS line format");
        }
        circuit.num_outputs = std::stoi(tokens[1]);
        // Output wires will be determined from gates
    }
    else if (command == "GATES") {
        if (tokens.size() != 2) {
            throw std::runtime_error("Invalid GATES line format");
        }
        circuit.num_gates = std::stoi(tokens[1]);
        circuit.gates.reserve(circuit.num_gates);
    }
    else if (command == "GATE") {
        // Format: GATE output_wire input1_wire input2_wire GATE_TYPE
        // or: GATE output_wire input1_wire GATE_TYPE (for unary gates)
        if (tokens.size() < 4) {
            throw std::runtime_error("Invalid GATE line format");
        }
        
        int output_wire = std::stoi(tokens[1]);
        int input1_wire = std::stoi(tokens[2]);
        
        if (tokens.size() == 4) {
            // Unary gate
            GateType type = string_to_gate_type(tokens[3]);
            circuit.gates.emplace_back(output_wire, input1_wire, type);
        } else if (tokens.size() == 5) {
            // Binary gate
            int input2_wire = std::stoi(tokens[3]);
            GateType type = string_to_gate_type(tokens[4]);
            circuit.gates.emplace_back(output_wire, input1_wire, input2_wire, type);
        } else {
            throw std::runtime_error("Invalid GATE line format");
        }
        
        // Update max wire number
        circuit.num_wires = std::max({circuit.num_wires, output_wire, input1_wire});
        if (tokens.size() == 5) {
            circuit.num_wires = std::max(circuit.num_wires, std::stoi(tokens[3]));
        }
    }
}

bool GarbledCircuitManager::validate_circuit(const Circuit& circuit) {
    // Basic validation checks
    if (circuit.num_inputs <= 0 || circuit.num_outputs <= 0 || circuit.num_gates <= 0) {
        return false;
    }
    
    if (circuit.gates.size() != static_cast<size_t>(circuit.num_gates)) {
        return false;
    }
    
    if (circuit.input_wires.size() != static_cast<size_t>(circuit.num_inputs)) {
        return false;
    }
    
    return check_wire_consistency(circuit) && check_gate_validity(circuit);
}

bool GarbledCircuitManager::check_wire_consistency(const Circuit& circuit) {
    std::set<int> defined_wires(circuit.input_wires.begin(), circuit.input_wires.end());
    
    // Check that all gates have properly defined inputs
    for (const auto& gate : circuit.gates) {
        // Check if input wires are defined
        if (defined_wires.find(gate.input_wire1) == defined_wires.end()) {
            LOG_ERROR("Gate uses undefined wire: " << gate.input_wire1);
            return false;
        }
        
        if (gate.input_wire2 != -1 && 
            defined_wires.find(gate.input_wire2) == defined_wires.end()) {
            LOG_ERROR("Gate uses undefined wire: " << gate.input_wire2);
            return false;
        }
        
        // Add output wire to defined wires
        defined_wires.insert(gate.output_wire);
    }
    
    return true;
}

bool GarbledCircuitManager::check_gate_validity(const Circuit& circuit) {
    for (const auto& gate : circuit.gates) {
        // Check gate type validity
        if (gate.type == GateType::INPUT || gate.type == GateType::OUTPUT) {
            LOG_ERROR("Invalid gate type in circuit");
            return false;
        }
        
        // Check unary vs binary gate consistency
        bool is_unary = (gate.type == GateType::NOT);
        if (is_unary && gate.input_wire2 != -1) {
            LOG_ERROR("Unary gate has two inputs");
            return false;
        }
        if (!is_unary && gate.input_wire2 == -1) {
            LOG_ERROR("Binary gate has only one input");
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> GarbledCircuitManager::split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        token = trim_string(token);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

std::string GarbledCircuitManager::trim_string(const std::string& str) {
    size_t start = str.find_first_not_of(" \\t\\n\\r");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \\t\\n\\r");
    return str.substr(start, end - start + 1);
}

// Example circuit creation functions
Circuit GarbledCircuitManager::create_and_gate_circuit() {
    Circuit circuit;
    circuit.num_inputs = 2;
    circuit.num_outputs = 1;
    circuit.num_gates = 1;
    circuit.num_wires = 3;
    
    circuit.input_wires = {1, 2};
    circuit.output_wires = {3};
    
    // AND gate: output wire 3, inputs wires 1 and 2
    circuit.gates.emplace_back(3, 1, 2, GateType::AND);
    
    return circuit;
}

Circuit GarbledCircuitManager::create_or_gate_circuit() {
    Circuit circuit;
    circuit.num_inputs = 2;
    circuit.num_outputs = 1;
    circuit.num_gates = 1;
    circuit.num_wires = 3;
    
    circuit.input_wires = {1, 2};
    circuit.output_wires = {3};
    
    // OR gate: output wire 3, inputs wires 1 and 2
    circuit.gates.emplace_back(3, 1, 2, GateType::OR);
    
    return circuit;
}

Circuit GarbledCircuitManager::create_xor_gate_circuit() {
    Circuit circuit;
    circuit.num_inputs = 2;
    circuit.num_outputs = 1;
    circuit.num_gates = 1;
    circuit.num_wires = 3;
    
    circuit.input_wires = {1, 2};
    circuit.output_wires = {3};
    
    // XOR gate: output wire 3, inputs wires 1 and 2
    circuit.gates.emplace_back(3, 1, 2, GateType::XOR);
    
    return circuit;
}

Circuit GarbledCircuitManager::create_comparison_circuit(int bit_width) {
    // Create a circuit that compares two bit_width-bit numbers
    // Returns 1 if first number >= second number
    
    Circuit circuit;
    circuit.num_inputs = 2 * bit_width;
    circuit.num_outputs = 1;
    circuit.num_gates = 0; // Will be calculated
    
    int wire_counter = 2 * bit_width + 1; // Start after input wires
    
    // Input wires: 1 to bit_width (first number), bit_width+1 to 2*bit_width (second number)
    for (int i = 1; i <= 2 * bit_width; ++i) {
        circuit.input_wires.push_back(i);
    }
    
    // Build comparison logic bit by bit (simplified)
    // This is a basic implementation - in practice you'd want a more sophisticated comparator
    
    int carry_wire = wire_counter++;
    // Initialize carry to 1 (for >= comparison)
    // This would need additional logic gates
    
    for (int i = 0; i < bit_width; ++i) {
        int a_wire = i + 1;
        int b_wire = bit_width + i + 1;
        
        // XOR for difference
        int diff_wire = wire_counter++;
        circuit.gates.emplace_back(diff_wire, a_wire, b_wire, GateType::XOR);
        
        // AND for carry propagation (simplified)
        int new_carry = wire_counter++;
        circuit.gates.emplace_back(new_carry, a_wire, b_wire, GateType::AND);
        
        carry_wire = new_carry;
    }
    
    circuit.output_wires = {carry_wire};
    circuit.num_gates = circuit.gates.size();
    circuit.num_wires = wire_counter - 1;
    
    return circuit;
}
"""

# Continue with part 2
garbled_circuit_cpp_part2 = """
// Garbler implementation
Garbler::Garbler() {
}

GarbledCircuit Garbler::garble_circuit(const Circuit& circuit) {
    LOG_INFO("Garbling circuit with " << circuit.num_gates << " gates");
    
    GarbledCircuit gc(circuit);
    
    // Generate random labels for all wires
    generate_wire_labels(gc);
    
    // Garble each gate
    for (size_t i = 0; i < circuit.gates.size(); ++i) {
        gc.garbled_gates[i] = garble_gate(circuit.gates[i], i);
    }
    
    // Set up output mapping for decoding
    for (int output_wire : circuit.output_wires) {
        if (wire_labels.find(output_wire) != wire_labels.end()) {
            // Store the "0" label for output decoding
            gc.output_mapping[output_wire] = wire_labels[output_wire].first;
        }
    }
    
    LOG_INFO("Circuit garbling completed");
    return gc;
}

void Garbler::generate_wire_labels(GarbledCircuit& gc) {
    wire_labels.clear();
    
    // Generate labels for input wires
    for (int wire : gc.circuit.input_wires) {
        wire_labels[wire] = {
            CryptoUtils::generate_random_label(),
            CryptoUtils::generate_random_label()
        };
    }
    
    // Generate labels for internal and output wires
    for (const auto& gate : gc.circuit.gates) {
        if (wire_labels.find(gate.output_wire) == wire_labels.end()) {
            wire_labels[gate.output_wire] = {
                CryptoUtils::generate_random_label(),
                CryptoUtils::generate_random_label()
            };
        }
    }
    
    // Copy to garbled circuit
    gc.input_labels = wire_labels;
    
    LOG_INFO("Generated labels for " << wire_labels.size() << " wires");
}

GarbledGate Garbler::garble_gate(const Gate& gate, int gate_id) {
    switch (gate.type) {
        case GateType::AND:
            return garble_and_gate(gate, gate_id);
        case GateType::OR:
            return garble_or_gate(gate, gate_id);
        case GateType::XOR:
            return garble_xor_gate(gate, gate_id);
        case GateType::NAND:
            return garble_nand_gate(gate, gate_id);
        case GateType::NOT:
            return garble_not_gate(gate, gate_id);
        default:
            throw GarblerException("Unsupported gate type: " + gate_type_to_string(gate.type));
    }
}

GarbledGate Garbler::garble_and_gate(const Gate& gate, int gate_id) {
    GarbledGate garbled_gate;
    
    auto& out_labels = wire_labels[gate.output_wire];
    auto& in1_labels = wire_labels[gate.input_wire1];
    auto& in2_labels = wire_labels[gate.input_wire2];
    
    generate_garbled_table(garbled_gate, gate, gate_id,
                          out_labels.first, out_labels.second,
                          in1_labels.first, in1_labels.second,
                          in2_labels.first, in2_labels.second);
    
    return garbled_gate;
}

GarbledGate Garbler::garble_or_gate(const Gate& gate, int gate_id) {
    GarbledGate garbled_gate;
    
    auto& out_labels = wire_labels[gate.output_wire];
    auto& in1_labels = wire_labels[gate.input_wire1];
    auto& in2_labels = wire_labels[gate.input_wire2];
    
    generate_garbled_table(garbled_gate, gate, gate_id,
                          out_labels.first, out_labels.second,
                          in1_labels.first, in1_labels.second,
                          in2_labels.first, in2_labels.second);
    
    return garbled_gate;
}

GarbledGate Garbler::garble_xor_gate(const Gate& gate, int gate_id) {
    GarbledGate garbled_gate;
    
    auto& out_labels = wire_labels[gate.output_wire];
    auto& in1_labels = wire_labels[gate.input_wire1];
    auto& in2_labels = wire_labels[gate.input_wire2];
    
    generate_garbled_table(garbled_gate, gate, gate_id,
                          out_labels.first, out_labels.second,
                          in1_labels.first, in1_labels.second,
                          in2_labels.first, in2_labels.second);
    
    return garbled_gate;
}

GarbledGate Garbler::garble_nand_gate(const Gate& gate, int gate_id) {
    GarbledGate garbled_gate;
    
    auto& out_labels = wire_labels[gate.output_wire];
    auto& in1_labels = wire_labels[gate.input_wire1];
    auto& in2_labels = wire_labels[gate.input_wire2];
    
    generate_garbled_table(garbled_gate, gate, gate_id,
                          out_labels.first, out_labels.second,
                          in1_labels.first, in1_labels.second,
                          in2_labels.first, in2_labels.second);
    
    return garbled_gate;
}

GarbledGate Garbler::garble_not_gate(const Gate& gate, int gate_id) {
    GarbledGate garbled_gate;
    
    auto& out_labels = wire_labels[gate.output_wire];
    auto& in1_labels = wire_labels[gate.input_wire1];
    
    // For NOT gate, we only need 2 ciphertexts instead of 4
    // Encrypt: NOT(0) = 1, NOT(1) = 0
    
    garbled_gate.ciphertexts[0] = CryptoUtils::encrypt_label(
        out_labels.second, // NOT(0) = 1
        in1_labels.first,  // input label for 0
        WireLabel{},       // no second input
        gate_id
    );
    
    garbled_gate.ciphertexts[1] = CryptoUtils::encrypt_label(
        out_labels.first,  // NOT(1) = 0
        in1_labels.second, // input label for 1
        WireLabel{},       // no second input
        gate_id
    );
    
    // Fill remaining slots with random data
    garbled_gate.ciphertexts[2] = CryptoUtils::generate_random_labels(1)[0];
    garbled_gate.ciphertexts[3] = CryptoUtils::generate_random_labels(1)[0];
    
    permute_garbled_table(garbled_gate);
    
    return garbled_gate;
}

void Garbler::generate_garbled_table(GarbledGate& garbled_gate,
                                   const Gate& gate, 
                                   int gate_id,
                                   const WireLabel& out_label0,
                                   const WireLabel& out_label1,
                                   const WireLabel& in1_label0,
                                   const WireLabel& in1_label1,
                                   const WireLabel& in2_label0,
                                   const WireLabel& in2_label1) {
    
    // Generate all 4 ciphertexts for the truth table
    // Entry (a,b): encrypt output_label_for_gate(a,b) with input_labels(a,b)
    
    // (0,0)
    bool result_00 = gate_function(gate.type, false, false);
    WireLabel result_label_00 = result_00 ? out_label1 : out_label0;
    garbled_gate.ciphertexts[0] = CryptoUtils::encrypt_label(
        result_label_00, in1_label0, in2_label0, gate_id);
    
    // (0,1)  
    bool result_01 = gate_function(gate.type, false, true);
    WireLabel result_label_01 = result_01 ? out_label1 : out_label0;
    garbled_gate.ciphertexts[1] = CryptoUtils::encrypt_label(
        result_label_01, in1_label0, in2_label1, gate_id);
    
    // (1,0)
    bool result_10 = gate_function(gate.type, true, false);
    WireLabel result_label_10 = result_10 ? out_label1 : out_label0;
    garbled_gate.ciphertexts[2] = CryptoUtils::encrypt_label(
        result_label_10, in1_label1, in2_label0, gate_id);
    
    // (1,1)
    bool result_11 = gate_function(gate.type, true, true);
    WireLabel result_label_11 = result_11 ? out_label1 : out_label0;
    garbled_gate.ciphertexts[3] = CryptoUtils::encrypt_label(
        result_label_11, in1_label1, in2_label1, gate_id);
    
    // Randomly permute the table to hide the mapping
    permute_garbled_table(garbled_gate);
}

void Garbler::permute_garbled_table(GarbledGate& garbled_gate) {
    // Randomly permute the 4 ciphertexts
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(garbled_gate.ciphertexts.begin(), garbled_gate.ciphertexts.end(), g);
}

std::vector<WireLabel> Garbler::encode_inputs(const GarbledCircuit& gc, 
                                            const std::vector<bool>& inputs,
                                            const std::vector<int>& wire_indices) {
    if (inputs.size() != wire_indices.size()) {
        throw GarblerException("Input size mismatch");
    }
    
    std::vector<WireLabel> encoded_labels;
    encoded_labels.reserve(inputs.size());
    
    for (size_t i = 0; i < inputs.size(); ++i) {
        int wire_id = wire_indices[i];
        bool input_bit = inputs[i];
        
        auto it = gc.input_labels.find(wire_id);
        if (it == gc.input_labels.end()) {
            throw GarblerException("Wire not found: " + std::to_string(wire_id));
        }
        
        // Choose label based on input bit: 0 -> first label, 1 -> second label
        WireLabel label = input_bit ? it->second.second : it->second.first;
        encoded_labels.push_back(label);
    }
    
    return encoded_labels;
}

std::vector<bool> Garbler::decode_outputs(const GarbledCircuit& gc, 
                                        const std::vector<WireLabel>& output_labels) {
    std::vector<bool> results;
    results.reserve(output_labels.size());
    
    for (size_t i = 0; i < output_labels.size() && i < gc.circuit.output_wires.size(); ++i) {
        int output_wire = gc.circuit.output_wires[i];
        const WireLabel& result_label = output_labels[i];
        
        auto it = gc.output_mapping.find(output_wire);
        if (it == gc.output_mapping.end()) {
            throw GarblerException("Output wire mapping not found");
        }
        
        // Compare with the "0" label to determine the bit value
        bool bit_value = !CryptoUtils::labels_equal(result_label, it->second);
        results.push_back(bit_value);
    }
    
    return results;
}

std::vector<std::pair<WireLabel, WireLabel>> Garbler::get_ot_input_pairs(
    const GarbledCircuit& gc, const std::vector<int>& wire_indices) {
    
    std::vector<std::pair<WireLabel, WireLabel>> pairs;
    pairs.reserve(wire_indices.size());
    
    for (int wire_id : wire_indices) {
        auto it = gc.input_labels.find(wire_id);
        if (it == gc.input_labels.end()) {
            throw GarblerException("Wire not found for OT: " + std::to_string(wire_id));
        }
        
        pairs.push_back(it->second);
    }
    
    return pairs;
}
"""

# Combine both parts and write to file
garbled_circuit_cpp = garbled_circuit_cpp_part1 + garbled_circuit_cpp_part2

with open("garbled_circuits_utility/src/garbled_circuit.cpp", "w") as f:
    f.write(garbled_circuit_cpp)

print("garbled_circuit.cpp (part 1 & 2) created successfully!")
print(f"Total implementation length: {len(garbled_circuit_cpp)} characters")