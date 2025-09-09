# Create the remaining parts of garbled_circuit.cpp
garbled_circuit_cpp_part3 = """
// Evaluator implementation
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
    
    for (size_t i = 0; i < input_labels.size(); ++i) {
        wire_values[gc.circuit.input_wires[i]] = input_labels[i];
    }
    
    // Evaluate gates in order
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
    
    // Collect output labels
    std::vector<WireLabel> output_labels;
    output_labels.reserve(gc.circuit.output_wires.size());
    
    for (int output_wire : gc.circuit.output_wires) {
        auto it = wire_values.find(output_wire);
        if (it == wire_values.end()) {
            throw EvaluatorException("Output wire not found: " + std::to_string(output_wire));
        }
        output_labels.push_back(it->second);
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
    
    // Try to decrypt each ciphertext in the garbled table
    for (size_t i = 0; i < 4; ++i) {
        try {
            WireLabel result = CryptoUtils::decrypt_label(
                garbled_gate.ciphertexts[i], input1_label, input2_label, gate_id);
            
            // Verify decryption by checking if it has valid structure
            auto plaintext = CryptoUtils::encrypt_label(result, input1_label, input2_label, gate_id);
            if (CryptoUtils::is_valid_decryption(plaintext)) {
                eval_stats.successful_decryptions++;
                return result;
            }
        } catch (const CryptoException& e) {
            // Decryption failed, try next ciphertext
            continue;
        }
    }
    
    throw EvaluatorException("Failed to decrypt any ciphertext in garbled gate " + std::to_string(gate_id));
}

WireLabel Evaluator::evaluate_unary_gate(const GarbledGate& garbled_gate,
                                        const WireLabel& input_label,
                                        int gate_id) {
    eval_stats.decryption_attempts++;
    
    // For unary gates (like NOT), only first two ciphertexts are valid
    for (size_t i = 0; i < 2; ++i) {
        try {
            WireLabel result = CryptoUtils::decrypt_label(
                garbled_gate.ciphertexts[i], input_label, WireLabel{}, gate_id);
            
            eval_stats.successful_decryptions++;
            return result;
        } catch (const CryptoException& e) {
            // Decryption failed, try next ciphertext
            continue;
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
    for (size_t i = 0; i < 2; ++i) {
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
    
    // Evaluate gates in order
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
    
    for (int i = 0; i < bit_width; ++i) {
        bits.push_back((value >> i) & 1);
    }
    
    return bits;
}

int CircuitUtils::bits_to_int(const std::vector<bool>& bits) {
    int value = 0;
    for (size_t i = 0; i < bits.size(); ++i) {
        if (bits[i]) {
            value |= (1 << i);
        }
    }
    return value;
}

bool CircuitUtils::test_circuit_correctness(const Circuit& circuit, size_t num_tests) {
    LOG_INFO("Testing circuit correctness with " << num_tests << " random inputs");
    
    GarbledCircuitManager manager;
    Garbler garbler;
    Evaluator evaluator;
    
    // Garble the circuit once
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
    
    std::cout << "\\nInput wires: ";
    for (int wire : circuit.input_wires) {
        std::cout << wire << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Output wires: ";
    for (int wire : circuit.output_wires) {
        std::cout << wire << " ";
    }
    std::cout << std::endl;
    
    std::cout << "\\nGates:" << std::endl;
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

// File format implementations (simplified)
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
    
} // namespace FileFormats
"""

# Append to the existing file
with open("garbled_circuits_utility/src/garbled_circuit.cpp", "a") as f:
    f.write(garbled_circuit_cpp_part3)

print("garbled_circuit.cpp completed successfully!")
print(f"Part 3 length: {len(garbled_circuit_cpp_part3)} characters")