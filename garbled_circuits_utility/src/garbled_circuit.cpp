#include "garbled_circuit.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <set>
#include <sstream>

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
            continue;
        }
        
        parse_circuit_line(line, circuit);
    }
    
    // Determine output wires: gates outputs that are not inputs to other gates
    if (circuit.output_wires.empty()) {
        std::set<int> gate_inputs;
        std::set<int> gate_outputs;
        
        // Collect all input and output wires from gates
        for (const auto& gate : circuit.gates) {
            gate_outputs.insert(gate.output_wire);
            gate_inputs.insert(gate.input_wire1);
            if (gate.type != GateType::NOT) {
                gate_inputs.insert(gate.input_wire2);
            }
        }
        
        // Output wires are gate outputs that are not inputs to other gates
        for (int output_wire : gate_outputs) {
            if (gate_inputs.find(output_wire) == gate_inputs.end()) {
                circuit.output_wires.push_back(output_wire);
            }
        }
    }

    // Validate the parsed circuit
    if (!validate_circuit(circuit)) {
        throw std::runtime_error("Invalid circuit structure");
    }

    return circuit;
}

void GarbledCircuitManager::parse_circuit_line(const std::string& line, Circuit& circuit) {
    std::string work = line;
    auto hashPos = work.find('#');
    if (hashPos != std::string::npos) {
        work = work.substr(0, hashPos);
    }
    auto first_non = work.find_first_not_of(" \t\r\n");
    if (first_non == std::string::npos) {
        return;
    }
    auto last_non = work.find_last_not_of(" \t\r\n");
    work = work.substr(first_non, last_non - first_non + 1);

    std::istringstream iss(work);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(tok);
    }
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
        
        circuit.num_wires = std::max({circuit.num_wires, output_wire, input1_wire});
        if (tokens.size() == 5) {
            circuit.num_wires = std::max(circuit.num_wires, std::stoi(tokens[3]));
        }
    }
}

bool GarbledCircuitManager::validate_circuit(const Circuit& circuit) {
    // validation checks
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
    
    for (const auto& gate : circuit.gates) {
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
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

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
    
    // Fill remaining slots with random encrypted data to maintain consistent size
    auto random_labels = CryptoUtils::generate_random_labels(4);
    garbled_gate.ciphertexts[2] = CryptoUtils::encrypt_label(random_labels[0], random_labels[1], WireLabel{}, gate_id);
    garbled_gate.ciphertexts[3] = CryptoUtils::encrypt_label(random_labels[2], random_labels[3], WireLabel{}, gate_id);
    
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
    
    // for (int j = 0; j < 8; ++j) std::cout << std::hex << (int)in1_label0[j];
    // std::cout << std::endl;
    // for (int j = 0; j < 8; ++j) std::cout << std::hex << (int)in1_label1[j];
    // std::cout << std::endl;
    // for (int j = 0; j < 8; ++j) std::cout << std::hex << (int)in2_label0[j];
    // std::cout << std::endl;
    // for (int j = 0; j < 8; ++j) std::cout << std::hex << (int)in2_label1[j];
    // std::cout << std::endl;
    // for (int j = 0; j < 8; ++j) std::cout << std::hex << (int)out_label0[j];
    // std::cout << std::endl;
    // for (int j = 0; j < 8; ++j) std::cout << std::hex << (int)out_label1[j];
    // std::cout << std::endl;
    
    // Generate all 4 ciphertexts for the truth table
    // Entry (a,b): encrypt output_label_for_gate(a,b) with input_labels(a,b)
    
    // (0,0)
    bool result_00 = gate_function(gate.type, false, false);
    WireLabel result_label_00 = result_00 ? out_label1 : out_label0;
    auto ct0 = CryptoUtils::encrypt_label(result_label_00, in1_label0, in2_label0, gate_id);
    garbled_gate.ciphertexts[0] = ct0;
    
    // (0,1)  
    bool result_01 = gate_function(gate.type, false, true);
    WireLabel result_label_01 = result_01 ? out_label1 : out_label0;
    auto ct1 = CryptoUtils::encrypt_label(result_label_01, in1_label0, in2_label1, gate_id);
    garbled_gate.ciphertexts[1] = ct1;
    
    // (1,0)
    bool result_10 = gate_function(gate.type, true, false);
    WireLabel result_label_10 = result_10 ? out_label1 : out_label0;
    auto ct2 = CryptoUtils::encrypt_label(result_label_10, in1_label1, in2_label0, gate_id);
    garbled_gate.ciphertexts[2] = ct2;
    
    // (1,1)
    bool result_11 = gate_function(gate.type, true, true);
    WireLabel result_label_11 = result_11 ? out_label1 : out_label0;
    auto ct3 = CryptoUtils::encrypt_label(result_label_11, in1_label1, in2_label1, gate_id);
    garbled_gate.ciphertexts[3] = ct3;
    
    // Print ciphertexts before permutation
    // std::cout << "[CIPHER DEBUG] Gate " << gate_id << " - Ciphertexts (before permutation):" << std::endl;
    // std::cout << "               CT[0] (0,0)->" << (result_00 ? "1" : "0") << ": ";
    // for (size_t j = 0; j < garbled_gate.ciphertexts[0].size(); ++j) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)garbled_gate.ciphertexts[0][j];
    // }
    // std::cout << std::dec << std::endl;
    
    // std::cout << "               CT[1] (0,1)->" << (result_01 ? "1" : "0") << ": ";
    // for (size_t j = 0; j < garbled_gate.ciphertexts[1].size(); ++j) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)garbled_gate.ciphertexts[1][j];
    // }
    // std::cout << std::dec << std::endl;
    
    // std::cout << "               CT[2] (1,0)->" << (result_10 ? "1" : "0") << ": ";
    // for (size_t j = 0; j < garbled_gate.ciphertexts[2].size(); ++j) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)garbled_gate.ciphertexts[2][j];
    // }
    // std::cout << std::dec << std::endl;
    
    // std::cout << "               CT[3] (1,1)->" << (result_11 ? "1" : "0") << ": ";
    // for (size_t j = 0; j < garbled_gate.ciphertexts[3].size(); ++j) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)garbled_gate.ciphertexts[3][j];
    // }
    // std::cout << std::dec << std::endl;
    
    // Randomly permute the table to hide the mapping
    permute_garbled_table(garbled_gate);
    
    // Print ciphertexts after permutation
    // std::cout << "[CIPHER DEBUG] Gate " << gate_id << " - Ciphertexts (after permutation, sent to evaluator):" << std::endl;
    // for (int i = 0; i < 4; i++) {
    //     std::cout << "               CT[" << i << "]: ";
    //     for (size_t j = 0; j < garbled_gate.ciphertexts[i].size(); ++j) {
    //         std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)garbled_gate.ciphertexts[i][j];
    //     }
    //     std::cout << std::dec << std::endl;
    // }
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
        
        std::cout << "[OUTPUT DECODE] Wire " << output_wire << " comparison:" << std::endl;
        std::cout << "                Result label: ";
        for (int j = 0; j < 8; ++j) std::cout << std::hex << (int)result_label[j];
        std::cout << std::endl;
        std::cout << "                Zero label:   ";
        for (int j = 0; j < 8; ++j) std::cout << std::hex << (int)it->second[j];
        std::cout << std::endl;
        
        // Compare with the "0" label to determine the bit value
        bool labels_match = CryptoUtils::labels_equal(result_label, it->second);
        bool bit_value = !labels_match;
        
        std::cout << "                Labels match: " << (labels_match ? "YES" : "NO") << std::endl;
        std::cout << "                Decoded bit:  " << bit_value << std::endl;
        
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
        
        std::cout << "[OT DEBUG] Wire " << wire_id << " - sending label pair:" << std::endl;
        std::cout << "           Label_0: ";
        for (int i = 0; i < 8; ++i) std::cout << std::hex << (int)it->second.first[i];
        std::cout << std::endl;
        std::cout << "           Label_1: ";
        for (int i = 0; i < 8; ++i) std::cout << std::hex << (int)it->second.second[i];
        std::cout << std::endl;
        
        pairs.push_back(it->second);
    }
    
    return pairs;
}

Evaluator::Evaluator() {
    reset_stats();
}

std::vector<WireLabel> Evaluator::evaluate_circuit(const GarbledCircuit& gc,
                                                  const std::vector<WireLabel>& input_labels) {
    LOG_INFO("Evaluating garbled circuit with " << gc.circuit.gates.size() << " gates");
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    wire_values.clear();
    
    // Set input wire values
    if (input_labels.size() != gc.circuit.input_wires.size()) {
        throw EvaluatorException("Input label count mismatch");
    }
    
    std::cout << "[EVAL DEBUG] Evaluator received " << input_labels.size() << " input labels:" << std::endl;
    for (size_t i = 0; i < input_labels.size(); ++i) {
        wire_values[gc.circuit.input_wires[i]] = input_labels[i];
        std::cout << "             Wire " << gc.circuit.input_wires[i] << " label: ";
        for (int j = 0; j < 8; ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)input_labels[i][j];
        }
        std::cout << std::dec << std::endl;
    }
    
    // Evaluate gates 
    for (size_t i = 0; i < gc.circuit.gates.size(); ++i) {
        const auto& gate = gc.circuit.gates[i];
        const auto& garbled_gate = gc.garbled_gates[i];
        
        WireLabel result_label;
        
        if (gate.input_wire2 == -1) {
            // Unary gate
            auto input_it = wire_values.find(gate.input_wire1);
            if (input_it == wire_values.end()) {
                throw EvaluatorException("Input wire not found: " + std::to_string(gate.input_wire1));
            }
            
            result_label = evaluate_unary_gate(garbled_gate, input_it->second, i);
        } else {
            // Binary gate
            auto input1_it = wire_values.find(gate.input_wire1);
            auto input2_it = wire_values.find(gate.input_wire2);
            
            if (input1_it == wire_values.end() || input2_it == wire_values.end()) {
                throw EvaluatorException("Input wires not found for gate");
            }
            
            result_label = evaluate_gate(garbled_gate, input1_it->second, input2_it->second, i);
        }
        
        wire_values[gate.output_wire] = result_label;
        eval_stats.gates_evaluated++;
    }
    
    std::vector<WireLabel> output_labels;
    output_labels.reserve(gc.circuit.output_wires.size());
    
    for (int output_wire : gc.circuit.output_wires) {
        auto it = wire_values.find(output_wire);
        if (it == wire_values.end()) {
            throw EvaluatorException("Output wire not found: " + std::to_string(output_wire));
        }
        output_labels.push_back(it->second);
    }
    
    std::cout << "[EVAL DEBUG] Final output labels (to be sent to garbler):" << std::endl;
    for (size_t i = 0; i < output_labels.size(); ++i) {
        std::cout << "             Wire " << gc.circuit.output_wires[i] << " label: ";
        for (int j = 0; j < 8; ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)output_labels[i][j];
        }
        std::cout << std::dec << std::endl;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    eval_stats.total_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    LOG_INFO("Circuit evaluation completed in " << eval_stats.total_time.count() << " microseconds");
    
    return output_labels;
}

WireLabel Evaluator::evaluate_gate(const GarbledGate& garbled_gate,
                                  const WireLabel& input1_label,
                                  const WireLabel& input2_label,
                                  int gate_id) {
    eval_stats.decryption_attempts++;
    
    // Print the input labels being used
    std::cout << "[EVAL DEBUG] Gate " << gate_id << " - Input labels received:" << std::endl;
    std::cout << "             Input1 label: ";
    for (int j = 0; j < 8; ++j) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)input1_label[j];
    }
    std::cout << std::dec << std::endl;
    
    std::cout << "             Input2 label: ";
    for (int j = 0; j < 8; ++j) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)input2_label[j];
    }
    std::cout << std::dec << std::endl;
    
    // Try to decrypt each ciphertext in the garbled table
    // std::cout << "[EVAL DEBUG] Gate " << gate_id << " - Attempting to decrypt ciphertexts:" << std::endl;
    int count = 0;
    for (size_t i = 0; i < 4; ++i) {
        try {
            // std::cout << "             Trying CT[" << i << "]: ";
            // for (size_t j = 0; j < garbled_gate.ciphertexts[i].size(); ++j) {
            //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)garbled_gate.ciphertexts[i][j];
            // }
            // std::cout << std::dec << std::endl;
            
            WireLabel result = CryptoUtils::decrypt_label(
                garbled_gate.ciphertexts[i], input1_label, input2_label, gate_id);
            
            //std::cout << "             ✓ SUCCESS! Decrypted output label: ";
            eval_stats.cipher_decryptions++;
            std::cout<<"             "<<++count<<" ciphers are decrypted"<<std::endl;
            // for (int j = 0; j < 8; ++j) {
            //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)result[j];
            // }
            // std::cout << std::dec << std::endl;
            
            eval_stats.successful_decryptions++;
            return result;
        } catch (const CryptoException& e) {
            // Decryption failed, try next cipher
            // std::cout << "             ✗ Failed: " << e.what() << std::endl;
            eval_stats.cipher_decryptions++;
            count++;
            continue;
        }
    }
    
    throw EvaluatorException("Failed to decrypt any ciphertext in garbled gate " + std::to_string(gate_id));
}

WireLabel Evaluator::evaluate_unary_gate(const GarbledGate& garbled_gate,
                                        const WireLabel& input_label,
                                        int gate_id) {
    eval_stats.decryption_attempts++;

    for (size_t i = 0; i < 4; ++i) {
        try {
            WireLabel result = CryptoUtils::decrypt_label(
                garbled_gate.ciphertexts[i], input_label, WireLabel{}, gate_id);
            eval_stats.successful_decryptions++;
            return result;
        } catch (const CryptoException&) {
            continue; // try next ciphertext
        }
    }
    throw EvaluatorException("Failed to decrypt unary gate " + std::to_string(gate_id));
}

WireLabel Evaluator::try_decrypt_gate(const GarbledGate& garbled_gate,
                                     const WireLabel& input1,
                                     const WireLabel& input2,
                                     int gate_id) {
    for (size_t i = 0; i < 4; ++i) {
        try {
            return CryptoUtils::decrypt_label(garbled_gate.ciphertexts[i], input1, input2, gate_id);
        } catch (...) {
            continue;
        }
    }
    throw EvaluatorException("All decryptions failed");
}

WireLabel Evaluator::try_decrypt_unary_gate(const GarbledGate& garbled_gate,
                                           const WireLabel& input,
                                           int gate_id) {
    for (size_t i = 0; i < 4; ++i) {
        try {
            return CryptoUtils::decrypt_label(garbled_gate.ciphertexts[i], input, WireLabel{}, gate_id);
        } catch (...) {
            continue;
        }
    }
    throw EvaluatorException("All unary decryptions failed");
}

bool Evaluator::validate_inputs(const GarbledCircuit& gc, const std::vector<WireLabel>& inputs) {
    return inputs.size() == gc.circuit.input_wires.size();
}

// CircuitUtils implementation
std::vector<bool> CircuitUtils::evaluate_plaintext(const Circuit& circuit, 
                                                  const std::vector<bool>& inputs) {
    if (inputs.size() != static_cast<size_t>(circuit.num_inputs)) {
        throw std::invalid_argument("Input size mismatch");
    }
    
    std::map<int, bool> wire_values;
    
    // Set input values
    for (size_t i = 0; i < inputs.size(); ++i) {
        wire_values[circuit.input_wires[i]] = inputs[i];
    }
    
    // Evaluate gates 
    for (const auto& gate : circuit.gates) {
        bool result;
        
        if (gate.input_wire2 == -1) {
            // Unary gate
            bool input1 = wire_values[gate.input_wire1];
            result = gate_function(gate.type, input1);
        } else {
            // Binary gate
            bool input1 = wire_values[gate.input_wire1];
            bool input2 = wire_values[gate.input_wire2];
            result = gate_function(gate.type, input1, input2);
        }
        
        wire_values[gate.output_wire] = result;
    }
    
    // Collect outputs
    std::vector<bool> outputs;
    outputs.reserve(circuit.output_wires.size());
    
    for (int output_wire : circuit.output_wires) {
        outputs.push_back(wire_values[output_wire]);
    }
    
    return outputs;
}

bool CircuitUtils::verify_evaluation(const Circuit& circuit,
                                   const std::vector<bool>& inputs,
                                   const std::vector<bool>& garbled_output) {
    auto plaintext_output = evaluate_plaintext(circuit, inputs);
    
    if (plaintext_output.size() != garbled_output.size()) {
        return false;
    }
    
    for (size_t i = 0; i < plaintext_output.size(); ++i) {
        if (plaintext_output[i] != garbled_output[i]) {
            return false;
        }
    }
    
    return true;
}

std::vector<bool> CircuitUtils::generate_random_inputs(size_t num_inputs) {
    std::vector<bool> inputs;
    inputs.reserve(num_inputs);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::bernoulli_distribution dist(0.5);
    
    for (size_t i = 0; i < num_inputs; ++i) {
        inputs.push_back(dist(gen));
    }
    
    return inputs;
}

std::vector<bool> CircuitUtils::int_to_bits(int value, int bit_width) {
    std::vector<bool> bits;
    bits.reserve(bit_width);

    for (int i = bit_width - 1; i >= 0; --i) {
        bits.push_back(((value >> i) & 1) != 0);
    }

    return bits;
}

int CircuitUtils::bits_to_int(const std::vector<bool>& bits) {
    int value = 0;
    for (bool bit : bits) {
        value = (value << 1) | static_cast<int>(bit);
    }
    return value;
}

bool CircuitUtils::test_circuit_correctness(const Circuit& circuit, size_t num_tests) {
    LOG_INFO("Testing circuit correctness with " << num_tests << " random inputs");
    
    GarbledCircuitManager manager;
    Garbler garbler;
    Evaluator evaluator;
    
    // Garble the circuit 
    auto garbled_circuit = garbler.garble_circuit(circuit);
    
    for (size_t test = 0; test < num_tests; ++test) {
        // Generate random inputs
        auto inputs = generate_random_inputs(circuit.num_inputs);
        
        // Evaluate plaintext
        auto plaintext_result = evaluate_plaintext(circuit, inputs);
        
        // Evaluate garbled circuit
        auto input_labels = garbler.encode_inputs(garbled_circuit, inputs, circuit.input_wires);
        auto output_labels = evaluator.evaluate_circuit(garbled_circuit, input_labels);
        auto garbled_result = garbler.decode_outputs(garbled_circuit, output_labels);
        
        // Compare results
        if (!verify_evaluation(circuit, inputs, garbled_result)) {
            LOG_ERROR("Test " << test << " failed!");
            print_inputs_outputs(inputs, plaintext_result);
            print_inputs_outputs(inputs, garbled_result);
            return false;
        }
    }
    
    LOG_INFO("All " << num_tests << " tests passed!");
    return true;
}

void CircuitUtils::print_circuit(const Circuit& circuit) {
    std::cout << "Circuit Information:" << std::endl;
    std::cout << "  Inputs: " << circuit.num_inputs << std::endl;
    std::cout << "  Outputs: " << circuit.num_outputs << std::endl;
    std::cout << "  Gates: " << circuit.num_gates << std::endl;
    std::cout << "  Wires: " << circuit.num_wires << std::endl;
    
    std::cout << "\nInput wires: ";
    for (int wire : circuit.input_wires) {
        std::cout << wire << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Output wires: ";
    for (int wire : circuit.output_wires) {
        std::cout << wire << " ";
    }
    std::cout << std::endl;
    
    std::cout << "\nGates:" << std::endl;
    for (size_t i = 0; i < circuit.gates.size(); ++i) {
        print_gate(circuit.gates[i], i);
    }
}

void CircuitUtils::print_gate(const Gate& gate, int index) {
    std::cout << "  Gate " << index << ": ";
    std::cout << "out=" << gate.output_wire << " ";
    std::cout << "in1=" << gate.input_wire1 << " ";
    if (gate.input_wire2 != -1) {
        std::cout << "in2=" << gate.input_wire2 << " ";
    }
    std::cout << "type=" << gate_type_to_string(gate.type) << std::endl;
}

void CircuitUtils::print_inputs_outputs(const std::vector<bool>& inputs,
                                       const std::vector<bool>& outputs) {
    std::cout << "Inputs: ";
    for (bool bit : inputs) {
        std::cout << (bit ? '1' : '0');
    }
    std::cout << " -> Outputs: ";
    for (bool bit : outputs) {
        std::cout << (bit ? '1' : '0');
    }
    std::cout << std::endl;
}

namespace FileFormats {
    
    Circuit load_simple_circuit(const std::string& filename) {
        GarbledCircuitManager manager;
        return manager.load_circuit_from_file(filename);
    }
    
    void save_simple_circuit(const Circuit& circuit, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + filename);
        }
        
        file << "# Simple circuit format" << std::endl;
        file << "INPUTS " << circuit.num_inputs << std::endl;
        file << "OUTPUTS " << circuit.num_outputs << std::endl;
        file << "GATES " << circuit.num_gates << std::endl;
        file << std::endl;
        
        for (const auto& gate : circuit.gates) {
            file << "GATE " << gate.output_wire << " " << gate.input_wire1;
            if (gate.input_wire2 != -1) {
                file << " " << gate.input_wire2;
            }
            file << " " << gate_type_to_string(gate.type) << std::endl;
        }
        
        file.close();
    }
    
    Circuit load_bristol_circuit(const std::string& filename) {
        // Bristol format is more complex - this is a simplified implementation
        // Real Bristol format has specific syntax requirements
        return load_simple_circuit(filename);
    }
    
    void save_bristol_circuit(const Circuit& circuit, const std::string& filename) {
        // Bristol format implementation would go here
        save_simple_circuit(circuit, filename);
    }
    
    Circuit load_binary_circuit(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open binary circuit file: " + filename);
        }
        
        Circuit circuit;
        
        // Read header
        file.read(reinterpret_cast<char*>(&circuit.num_inputs), sizeof(circuit.num_inputs));
        file.read(reinterpret_cast<char*>(&circuit.num_outputs), sizeof(circuit.num_outputs));
        file.read(reinterpret_cast<char*>(&circuit.num_gates), sizeof(circuit.num_gates));
        file.read(reinterpret_cast<char*>(&circuit.num_wires), sizeof(circuit.num_wires));
        
        // Read input wires
        circuit.input_wires.resize(circuit.num_inputs);
        for (int& wire : circuit.input_wires) {
            file.read(reinterpret_cast<char*>(&wire), sizeof(wire));
        }
        
        // Read output wires
        circuit.output_wires.resize(circuit.num_outputs);
        for (int& wire : circuit.output_wires) {
            file.read(reinterpret_cast<char*>(&wire), sizeof(wire));
        }
        
        // Read gates
        circuit.gates.reserve(circuit.num_gates);
        for (int i = 0; i < circuit.num_gates; ++i) {
            int output_wire, input_wire1, input_wire2;
            GateType type;
            
            file.read(reinterpret_cast<char*>(&output_wire), sizeof(output_wire));
            file.read(reinterpret_cast<char*>(&input_wire1), sizeof(input_wire1));
            file.read(reinterpret_cast<char*>(&input_wire2), sizeof(input_wire2));
            file.read(reinterpret_cast<char*>(&type), sizeof(type));
            
            if (input_wire2 == -1) {
                circuit.gates.emplace_back(output_wire, input_wire1, type);
            } else {
                circuit.gates.emplace_back(output_wire, input_wire1, input_wire2, type);
            }
        }
        
        file.close();
        return circuit;
    }
    
    void save_binary_circuit(const Circuit& circuit, const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for binary writing: " + filename);
        }
        
        // Write header
        file.write(reinterpret_cast<const char*>(&circuit.num_inputs), sizeof(circuit.num_inputs));
        file.write(reinterpret_cast<const char*>(&circuit.num_outputs), sizeof(circuit.num_outputs));
        file.write(reinterpret_cast<const char*>(&circuit.num_gates), sizeof(circuit.num_gates));
        file.write(reinterpret_cast<const char*>(&circuit.num_wires), sizeof(circuit.num_wires));
        
        // Write input wires
        for (int wire : circuit.input_wires) {
            file.write(reinterpret_cast<const char*>(&wire), sizeof(wire));
        }
        
        // Write output wires
        for (int wire : circuit.output_wires) {
            file.write(reinterpret_cast<const char*>(&wire), sizeof(wire));
        }
        
        // Write gates
        for (const auto& gate : circuit.gates) {
            file.write(reinterpret_cast<const char*>(&gate.output_wire), sizeof(gate.output_wire));
            file.write(reinterpret_cast<const char*>(&gate.input_wire1), sizeof(gate.input_wire1));
            file.write(reinterpret_cast<const char*>(&gate.input_wire2), sizeof(gate.input_wire2));
            file.write(reinterpret_cast<const char*>(&gate.type), sizeof(gate.type));
        }
        
        file.close();
    }
    
}
