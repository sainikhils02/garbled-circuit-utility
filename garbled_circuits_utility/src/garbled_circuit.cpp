#include "garbled_circuit.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <set>
#include <sstream>
#include <numeric>
#include <cctype>

namespace FileFormats {
    void save_bristol_circuit(const Circuit& circuit, const std::string& filename);
}

GarbledCircuitManager::GarbledCircuitManager() {
}

Circuit GarbledCircuitManager::load_circuit_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open circuit file: " + filename);
    }
    
    return parse_bristol_stream(file);
}

Circuit GarbledCircuitManager::parse_circuit(const std::string& circuit_description) {
    std::istringstream input_stream(circuit_description);
    return parse_bristol_stream(input_stream);
}

void GarbledCircuitManager::save_circuit_to_file(const Circuit& circuit, const std::string& filename) {
    FileFormats::save_bristol_circuit(circuit, filename);
}

std::string GarbledCircuitManager::circuit_to_string(const Circuit& circuit) {
    std::ostringstream out;
    out << circuit.num_gates << ' ' << circuit.num_wires << '\n';

    auto write_partition = [&out](const std::vector<int>& partition, int fallback) {
        if (!partition.empty()) {
            for (size_t i = 0; i < partition.size(); ++i) {
                if (i != 0) out << ' ';
                out << partition[i];
            }
        } else {
            out << fallback;
        }
        out << '\n';
    };

    write_partition(circuit.input_partition, circuit.num_inputs);
    write_partition(circuit.output_partition, circuit.num_outputs);

    for (const auto& gate : circuit.gates) {
        if (gate.input_wire2 == -1) {
            out << "1 1 " << gate.input_wire1 << ' '
                << gate.output_wire << " INV\n";
        } else {
            std::string gate_type = gate_type_to_string(gate.type);
            if (gate.type == GateType::NOT) {
                gate_type = "INV";
            }
            out << "2 1 " << gate.input_wire1 << ' ' << gate.input_wire2 << ' '
                << gate.output_wire << ' ' << gate_type << '\n';
        }
    }

    return out.str();
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

    if (!circuit.input_partition.empty()) {
        int input_sum = std::accumulate(circuit.input_partition.begin(), circuit.input_partition.end(), 0);
        if (input_sum != circuit.num_inputs) {
            LOG_ERROR("Input partition does not sum to declared input count");
            return false;
        }
    }

    if (!circuit.output_partition.empty()) {
        int output_sum = std::accumulate(circuit.output_partition.begin(), circuit.output_partition.end(), 0);
        if (output_sum != circuit.num_outputs) {
            LOG_ERROR("Output partition does not sum to declared output count");
            return false;
        }
    }
    
    return check_wire_consistency(circuit) && check_gate_validity(circuit);
}

bool GarbledCircuitManager::check_wire_consistency(const Circuit& circuit) {
    for (int wire : circuit.input_wires) {
        if (wire < 0 || wire >= circuit.num_wires) {
            LOG_ERROR("Input wire index out of range: " << wire);
            return false;
        }
    }

    std::set<int> defined_wires(circuit.input_wires.begin(), circuit.input_wires.end());
    
    for (const auto& gate : circuit.gates) {
        if (gate.input_wire1 < 0 || gate.input_wire1 >= circuit.num_wires) {
            LOG_ERROR("Gate uses wire outside declared range: " << gate.input_wire1);
            return false;
        }
        if (defined_wires.find(gate.input_wire1) == defined_wires.end()) {
            LOG_ERROR("Gate uses undefined wire: " << gate.input_wire1);
            return false;
        }
        
        if (gate.input_wire2 != -1) {
            if (gate.input_wire2 < 0 || gate.input_wire2 >= circuit.num_wires) {
                LOG_ERROR("Gate uses wire outside declared range: " << gate.input_wire2);
                return false;
            }
            if (defined_wires.find(gate.input_wire2) == defined_wires.end()) {
                LOG_ERROR("Gate uses undefined wire: " << gate.input_wire2);
                return false;
            }
        }
        
        // Add output wire to defined wires
        if (gate.output_wire < 0 || gate.output_wire >= circuit.num_wires) {
            LOG_ERROR("Gate output wire outside declared range: " << gate.output_wire);
            return false;
        }
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

std::string GarbledCircuitManager::trim_string(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

Circuit GarbledCircuitManager::parse_bristol_stream(std::istream& input) {
    auto next_content_line = [&](std::istream& in) -> std::string {
        std::string line;
        while (std::getline(in, line)) {
            std::string trimmed = trim_string(line);
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }
            auto comment_pos = trimmed.find('#');
            if (comment_pos != std::string::npos) {
                trimmed = trim_string(trimmed.substr(0, comment_pos));
                if (trimmed.empty()) {
                    continue;
                }
            }
            return trimmed;
        }
        return {};
    };

    Circuit circuit;

    std::string header_line = next_content_line(input);
    if (header_line.empty()) {
        throw std::runtime_error("Bristol circuit is missing header line");
    }

    {
        std::istringstream header_stream(header_line);
        if (!(header_stream >> circuit.num_gates >> circuit.num_wires)) {
            throw std::runtime_error("Invalid Bristol header line: expected 'gates wires'");
        }
        if (circuit.num_gates <= 0 || circuit.num_wires <= 0) {
            throw std::runtime_error("Bristol header must specify positive gate and wire counts");
        }
    }

    std::string inputs_line = next_content_line(input);
    if (inputs_line.empty()) {
        throw std::runtime_error("Bristol circuit missing inputs line");
    }

    std::vector<int> input_counts;
    {
        std::istringstream inputs_stream(inputs_line);
        int count = 0;
        while (inputs_stream >> count) {
            if (count < 0) {
                throw std::runtime_error("Bristol inputs line contains negative count");
            }
            input_counts.push_back(count);
        }
    }

    if (input_counts.empty()) {
        throw std::runtime_error("Bristol inputs line must contain at least one integer");
    }

    int total_inputs = std::accumulate(input_counts.begin(), input_counts.end(), 0);
    if (total_inputs <= 0) {
        throw std::runtime_error("Bristol circuit must declare at least one input");
    }
    if (total_inputs > circuit.num_wires) {
        throw std::runtime_error("Total inputs exceed declared wire count in Bristol circuit");
    }

    circuit.num_inputs = total_inputs;
    circuit.input_wires.clear();
    circuit.input_wires.reserve(total_inputs);
    circuit.input_partition = input_counts;

    int next_wire = 0;
    for (int count : input_counts) {
        for (int i = 0; i < count; ++i) {
            circuit.input_wires.push_back(next_wire++);
        }
    }

    std::string outputs_line = next_content_line(input);
    if (outputs_line.empty()) {
        throw std::runtime_error("Bristol circuit missing outputs line");
    }

    std::vector<int> output_counts;
    {
        std::istringstream outputs_stream(outputs_line);
        int count = 0;
        while (outputs_stream >> count) {
            if (count < 0) {
                throw std::runtime_error("Bristol outputs line contains negative count");
            }
            output_counts.push_back(count);
        }
    }

    if (output_counts.empty()) {
        throw std::runtime_error("Bristol outputs line must contain at least one integer");
    }

    int total_outputs = std::accumulate(output_counts.begin(), output_counts.end(), 0);
    if (total_outputs <= 0) {
        throw std::runtime_error("Bristol circuit must declare at least one output");
    }
    if (total_outputs > circuit.num_wires) {
        throw std::runtime_error("Total outputs exceed declared wire count in Bristol circuit");
    }

    circuit.num_outputs = total_outputs;
    circuit.output_wires.clear();
    circuit.output_wires.reserve(total_outputs);
    circuit.output_partition = output_counts;
    for (int wire = circuit.num_wires - total_outputs; wire < circuit.num_wires; ++wire) {
        circuit.output_wires.push_back(wire);
    }

    circuit.gates.clear();
    circuit.gates.reserve(circuit.num_gates);

    auto normalize_gate_token = [](std::string token) {
        std::transform(token.begin(), token.end(), token.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        if (token == "INV") {
            return std::string("NOT");
        }
        return token;
    };

    for (int gate_index = 0; gate_index < circuit.num_gates; ++gate_index) {
        std::string gate_line = next_content_line(input);
        if (gate_line.empty()) {
            throw std::runtime_error("Unexpected end of file while reading Bristol gates");
        }

        std::istringstream gate_stream(gate_line);
        int input_count = 0;
        int output_count = 0;
        if (!(gate_stream >> input_count >> output_count)) {
            throw std::runtime_error("Malformed Bristol gate line: missing arity");
        }
        if (input_count <= 0 || output_count <= 0) {
            throw std::runtime_error("Bristol gate must have positive input/output counts");
        }

        std::vector<int> gate_inputs(input_count);
        for (int i = 0; i < input_count; ++i) {
            if (!(gate_stream >> gate_inputs[i])) {
                throw std::runtime_error("Malformed Bristol gate line: missing input wire index");
            }
        }

        std::vector<int> gate_outputs(output_count);
        for (int i = 0; i < output_count; ++i) {
            if (!(gate_stream >> gate_outputs[i])) {
                throw std::runtime_error("Malformed Bristol gate line: missing output wire index");
            }
        }

        std::string gate_type_token;
        if (!(gate_stream >> gate_type_token)) {
            throw std::runtime_error("Malformed Bristol gate line: missing gate type");
        }

        std::string gate_type_str = normalize_gate_token(gate_type_token);
        GateType gate_type = string_to_gate_type(gate_type_str);

        if (gate_outputs.size() != 1) {
            throw std::runtime_error("Only single-output gates are supported in this implementation");
        }

        int out_wire = gate_outputs[0];
        if (out_wire < 0 || out_wire >= circuit.num_wires) {
            throw std::runtime_error("Gate output wire index out of range in Bristol circuit");
        }

        if (input_count == 1) {
            int in_wire = gate_inputs[0];
            if (in_wire < 0 || in_wire >= circuit.num_wires) {
                throw std::runtime_error("Gate input wire index out of range in Bristol circuit");
            }
            circuit.gates.emplace_back(out_wire, in_wire, gate_type);
        } else if (input_count == 2) {
            int in_wire1 = gate_inputs[0];
            int in_wire2 = gate_inputs[1];
            if (in_wire1 < 0 || in_wire1 >= circuit.num_wires ||
                in_wire2 < 0 || in_wire2 >= circuit.num_wires) {
                throw std::runtime_error("Gate input wire index out of range in Bristol circuit");
            }
            circuit.gates.emplace_back(out_wire, in_wire1, in_wire2, gate_type);
        } else {
            throw std::runtime_error("Only unary or binary gates are supported in this implementation");
        }
    }

    circuit.num_gates = static_cast<int>(circuit.gates.size());

    if (!validate_circuit(circuit)) {
        throw std::runtime_error("Parsed Bristol circuit failed validation");
    }

    return circuit;
}

Circuit GarbledCircuitManager::create_and_gate_circuit() {
    Circuit circuit;
    circuit.num_inputs = 2;
    circuit.num_outputs = 1;
    circuit.num_gates = 1;
    circuit.num_wires = 3;
    
    circuit.input_wires = {0, 1};
    circuit.output_wires = {2};
    circuit.input_partition = {1, 1};
    circuit.output_partition = {1};
    
    // AND gate: output wire 2, input wires 0 and 1
    circuit.gates.emplace_back(2, 0, 1, GateType::AND);
    circuit.num_gates = static_cast<int>(circuit.gates.size());
    
    return circuit;
}

Circuit GarbledCircuitManager::create_or_gate_circuit() {
    Circuit circuit;
    circuit.num_inputs = 2;
    circuit.num_outputs = 1;
    circuit.num_gates = 1;
    circuit.num_wires = 3;
    
    circuit.input_wires = {0, 1};
    circuit.output_wires = {2};
    circuit.input_partition = {1, 1};
    circuit.output_partition = {1};
    
    // OR gate: output wire 2, input wires 0 and 1
    circuit.gates.emplace_back(2, 0, 1, GateType::OR);
    circuit.num_gates = static_cast<int>(circuit.gates.size());
    
    return circuit;
}

Circuit GarbledCircuitManager::create_xor_gate_circuit() {
    Circuit circuit;
    circuit.num_inputs = 2;
    circuit.num_outputs = 1;
    circuit.num_gates = 1;
    circuit.num_wires = 3;
    
    circuit.input_wires = {0, 1};
    circuit.output_wires = {2};
    circuit.input_partition = {1, 1};
    circuit.output_partition = {1};
    
    // XOR gate: output wire 2, input wires 0 and 1
    circuit.gates.emplace_back(2, 0, 1, GateType::XOR);
    circuit.num_gates = static_cast<int>(circuit.gates.size());
    
    return circuit;
}

Circuit GarbledCircuitManager::create_comparison_circuit(int bit_width) {
    // Returns 1 if first number >= second number
    
    Circuit circuit;
    circuit.num_inputs = 2 * bit_width;
    circuit.num_outputs = 1;
    circuit.num_gates = 0; // Will be calculated
    circuit.input_partition = {bit_width, bit_width};
    circuit.output_partition = {1};
    
    int wire_counter = 2 * bit_width; // Start after input wires
    
    // Input wires: 0..bit_width-1 (first number), bit_width..(2*bit_width-1) (second number)
    for (int i = 0; i < 2 * bit_width; ++i) {
        circuit.input_wires.push_back(i);
    }
    
    // Build comparison logic bit by bit (simplified placeholder comparator)
    int carry_wire = -1;
    for (int i = 0; i < bit_width; ++i) {
        int a_wire = i;
        int b_wire = bit_width + i;
        
        // XOR for difference
        int diff_wire = wire_counter++;
        circuit.gates.emplace_back(diff_wire, a_wire, b_wire, GateType::XOR);
        
        // AND for carry propagation (simplified)
        int new_carry = wire_counter++;
        circuit.gates.emplace_back(new_carry, a_wire, b_wire, GateType::AND);
        
        carry_wire = new_carry;
    }
    
    if (carry_wire == -1) {
        carry_wire = 0;
    }
    
    circuit.output_wires = {carry_wire};
    circuit.num_gates = static_cast<int>(circuit.gates.size());
    circuit.num_wires = wire_counter;
    
    return circuit;
}

Garbler::Garbler(bool use_pandp) : use_pandp_(use_pandp) {}

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
        WireLabel l0 = CryptoUtils::generate_random_label();
        WireLabel l1 = CryptoUtils::generate_random_label();
        if (use_pandp_) {
            // Set permutation/color bit as LSB of last byte: 0 for label0, 1 for label1
            l0[WIRE_LABEL_SIZE - 1] &= 0xFE;
            l1[WIRE_LABEL_SIZE - 1] |= 0x01;
        }
        wire_labels[wire] = {l0, l1};
    }
    
    // Generate labels for internal and output wires
    for (const auto& gate : gc.circuit.gates) {
        if (wire_labels.find(gate.output_wire) == wire_labels.end()) {
            WireLabel l0 = CryptoUtils::generate_random_label();
            WireLabel l1 = CryptoUtils::generate_random_label();
            if (use_pandp_) {
                l0[WIRE_LABEL_SIZE - 1] &= 0xFE;
                l1[WIRE_LABEL_SIZE - 1] |= 0x01;
            }
            wire_labels[gate.output_wire] = {l0, l1};
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
    
    if (!use_pandp_) {
        permute_garbled_table(garbled_gate);
    }
    
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
    
    if (use_pandp_) {
        // Canonical ordering by permutation bits (a = perm(in1), b = perm(in2)) => index = a*2 + b
        std::array<std::vector<uint8_t>,4> ordered{};
        // Compute perm bits
        uint8_t a0 = perm_bit(in1_label0);
        uint8_t a1 = perm_bit(in1_label1);
        uint8_t b0 = perm_bit(in2_label0);
        uint8_t b1 = perm_bit(in2_label1);
        // Map (actual input bit, perm bit) to index
        ordered[a0 * 2 + b0] = garbled_gate.ciphertexts[0]; // (0,0)
        ordered[a0 * 2 + b1] = garbled_gate.ciphertexts[1]; // (0,1)
        ordered[a1 * 2 + b0] = garbled_gate.ciphertexts[2]; // (1,0)
        ordered[a1 * 2 + b1] = garbled_gate.ciphertexts[3]; // (1,1)
        garbled_gate.ciphertexts = ordered;
    } else {
        // Randomly permute the table to hide the mapping
        permute_garbled_table(garbled_gate);
    }
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
        bool labels_match = CryptoUtils::labels_equal(result_label, it->second);
        bool bit_value = !labels_match;
        
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

// Defaulted via Evaluator(bool use_pandp = false)
Evaluator::Evaluator(bool use_pandp) : use_pandp_(use_pandp) { reset_stats(); }

std::vector<WireLabel> Evaluator::evaluate_circuit(const GarbledCircuit& gc,
                                                  const std::vector<WireLabel>& input_labels) {
    LOG_INFO("Evaluating garbled circuit with " << gc.circuit.gates.size() << " gates");
    
    
    wire_values.clear();
    
    // Set input wire values
    if (input_labels.size() != gc.circuit.input_wires.size()) {
        throw EvaluatorException("Input label count mismatch");
    }
    
    for (size_t i = 0; i < input_labels.size(); ++i) {
        wire_values[gc.circuit.input_wires[i]] = input_labels[i];
    }
    
    // Evaluate gates
    auto start_time = std::chrono::high_resolution_clock::now();
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
    auto end_time = std::chrono::high_resolution_clock::now();
    
    std::vector<WireLabel> output_labels;
    output_labels.reserve(gc.circuit.output_wires.size());
    
    for (int output_wire : gc.circuit.output_wires) {
        auto it = wire_values.find(output_wire);
        if (it == wire_values.end()) {
            throw EvaluatorException("Output wire not found: " + std::to_string(output_wire));
        }
        output_labels.push_back(it->second);
    }
    
    // std::cout << "[EVAL DEBUG] Final output labels (to be sent to garbler):" << std::endl;
    // for (size_t i = 0; i < output_labels.size(); ++i) {
    //     std::cout << "             Wire " << gc.circuit.output_wires[i] << " label: ";
    //     for (int j = 0; j < 8; ++j) {
    //         std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)output_labels[i][j];
    //     }
    //     std::cout << std::dec << std::endl;
    // }
    
    
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
    // std::cout << "[EVAL DEBUG] Gate " << gate_id << " - Input labels received:" << std::endl;
    // std::cout << "             Input1 label: ";
    // for (int j = 0; j < 8; ++j) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)input1_label[j];
    // }
    // std::cout << std::dec << std::endl;
    
    // std::cout << "             Input2 label: ";
    // for (int j = 0; j < 8; ++j) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)input2_label[j];
    // }
    // std::cout << std::dec << std::endl;

    if (use_pandp_) {
        // Directly select row using permutation bits
        uint8_t a = perm_bit(input1_label);
        uint8_t b = perm_bit(input2_label);
        size_t idx = static_cast<size_t>(a * 2 + b);
        try {
            WireLabel result = CryptoUtils::decrypt_label(
                garbled_gate.ciphertexts[idx], input1_label, input2_label, gate_id);
                eval_stats.cipher_decryptions++;
            eval_stats.successful_decryptions++;
            return result;
        } catch (const CryptoException& e) {
            eval_stats.cipher_decryptions++;
            throw EvaluatorException(std::string("Point-and-permute decryption failed: ") + e.what());
        }
    } else {
        // Try to decrypt each ciphertext in the garbled table
        int count = 0;
        for (size_t i = 0; i < 4; ++i) {
            try {
                WireLabel result = CryptoUtils::decrypt_label(
                    garbled_gate.ciphertexts[i], input1_label, input2_label, gate_id);
                eval_stats.cipher_decryptions++;
                eval_stats.successful_decryptions++;
                // std::cout<<"             "<<++count<<" ciphers are decrypted"<<std::endl;
                return result;
            } catch (const CryptoException&) {
                eval_stats.cipher_decryptions++;
                count++;
                continue;
            }
        }
        throw EvaluatorException("Failed to decrypt any ciphertext in garbled gate " + std::to_string(gate_id));
    }
    throw EvaluatorException("Failed to decrypt any ciphertext in garbled gate " + std::to_string(gate_id));
}

WireLabel Evaluator::evaluate_unary_gate(const GarbledGate& garbled_gate,
                                        const WireLabel& input_label,
                                        int gate_id) {
    eval_stats.decryption_attempts++;
    if (use_pandp_) {
        // Point-and-permute for unary gates: index by the input label's perm bit
        uint8_t a = perm_bit(input_label);        // 0 for label0, 1 for label1
        size_t idx = static_cast<size_t>(a);
        try {
            WireLabel result = CryptoUtils::decrypt_label(
                garbled_gate.ciphertexts[idx], input_label, WireLabel{}, gate_id);
            eval_stats.cipher_decryptions++;
            eval_stats.successful_decryptions++;
            return result;
        } catch (const CryptoException& e) {
            eval_stats.cipher_decryptions++;
            throw EvaluatorException(std::string("Point-and-permute (unary) decryption failed: ") + e.what());
        }
    }else{
        int count = 0;
        for (size_t i = 0; i < 4; ++i) {
            try {
                WireLabel result = CryptoUtils::decrypt_label(
                    garbled_gate.ciphertexts[i], input_label, WireLabel{}, gate_id);
                eval_stats.cipher_decryptions++;
                eval_stats.successful_decryptions++;
                std::cout<<"             "<<++count<<" ciphers are decrypted"<<std::endl;
                return result;
            } catch (const CryptoException&) {
                eval_stats.cipher_decryptions++;
                continue; // try next ciphertext
            }
        }
        throw EvaluatorException("Failed to decrypt unary gate " + std::to_string(gate_id));
    }
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
    
    Circuit load_bristol_circuit(const std::string& filename) {
        GarbledCircuitManager manager;
        return manager.load_circuit_from_file(filename);
    }

    Circuit load_simple_circuit(const std::string& filename) {
        // Legacy helper: delegate to Bristol parser
        return load_bristol_circuit(filename);
    }

    void save_bristol_circuit(const Circuit& circuit, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + filename);
        }

        file << circuit.num_gates << " " << circuit.num_wires << std::endl;

        if (!circuit.input_partition.empty()) {
            for (size_t i = 0; i < circuit.input_partition.size(); ++i) {
                if (i != 0) file << ' ';
                file << circuit.input_partition[i];
            }
        } else {
            file << circuit.num_inputs;
        }
        file << std::endl;

        if (!circuit.output_partition.empty()) {
            for (size_t i = 0; i < circuit.output_partition.size(); ++i) {
                if (i != 0) file << ' ';
                file << circuit.output_partition[i];
            }
        } else {
            file << circuit.num_outputs;
        }
        file << std::endl;

        for (const auto& gate : circuit.gates) {
            if (gate.input_wire2 == -1) {
                file << "1 1 " << gate.input_wire1 << " " << gate.output_wire << " INV" << std::endl;
            } else {
                std::string gate_type = gate_type_to_string(gate.type);
                if (gate.type == GateType::NOT) {
                    gate_type = "INV";
                }
                file << "2 1 " << gate.input_wire1 << " " << gate.input_wire2 << " "
                     << gate.output_wire << " " << gate_type << std::endl;
            }
        }
    }

    void save_simple_circuit(const Circuit& circuit, const std::string& filename) {
        // Legacy helper: write Bristol fashion
        save_bristol_circuit(circuit, filename);
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
