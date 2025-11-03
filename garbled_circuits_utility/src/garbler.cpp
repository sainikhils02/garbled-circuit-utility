#include "common.h"
#include "garbled_circuit.h"
#include "socket_utils.h"
#include "ot_handler.h"
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <chrono>

/**
 * Responsibilities:
 * 1. Load and garble the circuit
 * 2. Listen for evaluator connection
 * 3. Send garbled circuit to evaluator
 * 4. Perform OT for evaluator's inputs
 * 5. Receive and display final result
 */
class GarblerProgram {
public:
    GarblerProgram() : port(DEFAULT_PORT) {}
    
    int run(int argc, char* argv[]) {
        try {
            if (!parse_arguments(argc, argv)) {
                return 1;
            }
                        
            auto t0 = std::chrono::high_resolution_clock::now();
            // Load circuit
            auto circuit = load_circuit();
            auto t1 = std::chrono::high_resolution_clock::now();
            auto load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            std::cout << "[TIME] Loaded circuit in " << load_ms << " ms" << std::endl;
            
            // Parse garbler inputs
            auto garbler_inputs = parse_inputs();
            
            // Garble circuit
            auto tg0 = std::chrono::high_resolution_clock::now();
            Garbler garbler(use_pandp);
            auto garbled_circuit = garbler.garble_circuit(circuit);
            auto tg1 = std::chrono::high_resolution_clock::now();
            auto garble_ms = std::chrono::duration_cast<std::chrono::milliseconds>(tg1 - tg0).count();
            std::cout << "[TIME] Garbled circuit in " << garble_ms << " ms" << std::endl;
            
            // Set up network connection
            auto connection = std::make_unique<SocketConnection>(port);
            connection->wait_for_client();
            
            auto protocol = ProtocolManager(std::move(connection));
            
            // Protocol execution
            execute_protocol(protocol, garbled_circuit, garbler, garbler_inputs);
            
            std::cout << "Protocol completed successfully!" << std::endl;
            return 0;
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }

private:
    std::string circuit_file;
    std::string input_string;
    int port;
    bool use_pandp = false;
    
    
    bool parse_arguments(int argc, char* argv[]) {
        static struct option long_options[] = {
            {"port", required_argument, 0, 'p'},
            {"circuit", required_argument, 0, 'c'},
            {"input", required_argument, 0, 'i'},
            {"pandp", no_argument, 0, 0},
            {0, 0, 0, 0}
        };
        
        int opt;
        int option_index = 0;
        
        while ((opt = getopt_long(argc, argv, "p:c:i:", long_options, &option_index)) != -1) {
            switch (opt) {
                case 'p':
                    port = std::stoi(optarg);
                    break;
                case 'c':
                    circuit_file = optarg;
                    break;
                case 'i':
                    input_string = optarg;
                    break;
                case 0:
                    if (std::string(long_options[option_index].name) == "pandp") {
                        use_pandp = true;
                    }
                    break;
                default:
                    return false;
            }
        }
        
        if (circuit_file.empty()) {
            std::cerr << "Error: Circuit file is required (use -c or --circuit)" << std::endl;
            return false;
        }
        
        return true;
    }
    
    
    Circuit load_circuit() {
        GarbledCircuitManager manager;
        return manager.load_circuit_from_file(circuit_file);
    }
    
    std::vector<bool> parse_inputs() {
        std::vector<bool> inputs;
        
        if (input_string.empty()) {
            return inputs;
        }
        
        for (char c : input_string) {
            if (c == '0') {
                inputs.push_back(false);
            } else if (c == '1') {
                inputs.push_back(true);
            } else if (c != ' ' && c != ',') {
                throw std::invalid_argument("Invalid input bit: " + std::string(1, c));
            }
        }
        
        return inputs;
    }
    
    void execute_protocol(ProtocolManager& protocol, 
                         const GarbledCircuit& gc,
                         Garbler& garbler,
                         const std::vector<bool>& garbler_inputs) {
        
        // Step 0: Exchange hello messages
        protocol.send_hello("Garbler");
        std::string evaluator_name = protocol.receive_hello();
        std::cout << "Connected to: " << evaluator_name << std::endl;
        
        // Display protocol information
        std::cout << "\n=== GARBLED CIRCUIT PROTOCOL ===" << std::endl;
        std::cout << "Garbler Input:  ";
        for (bool bit : garbler_inputs) {
            std::cout << (bit ? '1' : '0');
        }
        std::cout << " (decimal: " << CircuitUtils::bits_to_int(garbler_inputs) << ")" << std::endl;
        if (use_pandp) std::cout << "Point-and-Permute: ENABLED" << std::endl;

        size_t expected_garbler_inputs = garbler_inputs.size();
        size_t expected_evaluator_inputs = 0;
        if (!gc.circuit.input_partition.empty()) {
            expected_garbler_inputs = static_cast<size_t>(gc.circuit.input_partition.front());
            expected_evaluator_inputs = 0;
            for (size_t idx = 1; idx < gc.circuit.input_partition.size(); ++idx) {
                expected_evaluator_inputs += static_cast<size_t>(gc.circuit.input_partition[idx]);
            }
        } else {
            if (gc.circuit.num_inputs < static_cast<int>(expected_garbler_inputs)) {
                throw std::invalid_argument("Circuit declares fewer input wires than bits provided by garbler");
            }
            expected_evaluator_inputs = static_cast<size_t>(gc.circuit.num_inputs) - expected_garbler_inputs;
        }

        if (static_cast<size_t>(gc.circuit.num_inputs) != expected_garbler_inputs + expected_evaluator_inputs) {
            throw std::invalid_argument("Circuit input partition does not match declared input count");
        }

        if (garbler_inputs.size() != expected_garbler_inputs) {
            throw std::invalid_argument("Garbler provided " + std::to_string(garbler_inputs.size()) +
                                         " bits but circuit expects " + std::to_string(expected_garbler_inputs));
        }
        
        // Step 1: Send garbled circuit
    std::cout << "\n[STEP 1] Sending garbled circuit to evaluator..." << std::endl;
    auto s0 = std::chrono::high_resolution_clock::now();
    protocol.send_circuit(gc);
    auto s1 = std::chrono::high_resolution_clock::now();
    std::cout << "           Done in "
          << std::chrono::duration_cast<std::chrono::milliseconds>(s1 - s0).count()
          << " ms" << std::endl;
        
        // Step 2: Send garbler's input labels
        std::vector<int> garbler_wire_indices;
        garbler_wire_indices.reserve(expected_garbler_inputs);
        for (size_t i = 0; i < expected_garbler_inputs; ++i) {
            garbler_wire_indices.push_back(gc.circuit.input_wires[i]);
        }
        
        if (!garbler_inputs.empty()) {
            std::cout << "[STEP 2] Sending garbler's input labels..." << std::endl;
            auto s2 = std::chrono::high_resolution_clock::now();
            auto garbler_labels = garbler.encode_inputs(gc, garbler_inputs, garbler_wire_indices);
            protocol.send_input_labels(garbler_labels);
            auto s3 = std::chrono::high_resolution_clock::now();
            std::cout << "           Sent in "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(s3 - s2).count()
                      << " ms" << std::endl;
            std::cout << "           Sent " << garbler_labels.size() << " wire labels for garbler's inputs" << std::endl;
        }
        
        // Step 3: Perform OT for evaluator's inputs
    size_t evaluator_input_count = expected_evaluator_inputs;
        if (evaluator_input_count > 0) {
            std::cout << "[STEP 3] Performing OT for evaluator's " << evaluator_input_count << " inputs..." << std::endl;
            auto ot0 = std::chrono::high_resolution_clock::now();
            perform_ot_for_evaluator(protocol, gc, garbler, evaluator_input_count);
            auto ot1 = std::chrono::high_resolution_clock::now();
            std::cout << "           OT completed in "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(ot1 - ot0).count()
                      << " ms" << std::endl;
        }
        
        // Step 4: Receive result
        std::cout << "[STEP 4] Waiting for evaluation result..." << std::endl;
        
    auto r0 = std::chrono::high_resolution_clock::now();
    auto result_data = protocol.receive_result();
    auto r1 = std::chrono::high_resolution_clock::now();
    std::cout << "           Received in "
          << std::chrono::duration_cast<std::chrono::milliseconds>(r1 - r0).count()
          << " ms" << std::endl;
        
        // The result should contain the output wire labels
        std::vector<WireLabel> output_labels;
        for (size_t i = 0; i < result_data.size(); i += WIRE_LABEL_SIZE) {
            if (i + WIRE_LABEL_SIZE <= result_data.size()) {
                WireLabel label;
                std::copy(result_data.begin() + i, result_data.begin() + i + WIRE_LABEL_SIZE, label.begin());
                output_labels.push_back(label);
            }
        }
        
        // Decode the final result
    auto d0 = std::chrono::high_resolution_clock::now();
    auto final_result = garbler.decode_outputs(gc, output_labels);
    auto d1 = std::chrono::high_resolution_clock::now();
    std::cout << "[TIME] Decoded outputs in "
          << std::chrono::duration_cast<std::chrono::milliseconds>(d1 - d0).count()
          << " ms" << std::endl;
        
        std::cout << "\n=== PROTOCOL RESULT ===" << std::endl;
        // Show bits MSB->LSB for readability and compute decimal.
        int decimal_value = 0;
        for (size_t i = 0; i < final_result.size(); ++i) {
            if (final_result[i]) decimal_value |= (1 << static_cast<int>(i));
        }
        std::cout << "Circuit Output: ";
        for (size_t i = final_result.size(); i > 0; --i) {
            std::cout << (final_result[i - 1] ? '1' : '0');
        }
        std::cout << " (decimal: " << decimal_value << ")" << std::endl;
        
        // Show Multi Party Computation summary
    std::cout << "\n=== COMPUTATION SUMMARY ===" << std::endl;
    std::cout << "Function computed: Garbler(" << CircuitUtils::bits_to_int(garbler_inputs)
          << ") ⊕ Evaluator(?) = " << decimal_value << std::endl;
        
        protocol.send_goodbye();
    }
    
    void perform_ot_for_evaluator(ProtocolManager& protocol, 
                                 const GarbledCircuit& gc,
                                 Garbler& garbler,
                                 size_t evaluator_input_count) {
        
        // Get the wire indices for evaluator's inputs
        std::vector<int> evaluator_wire_indices;
        size_t garbler_input_count = gc.circuit.num_inputs - evaluator_input_count;
        
        for (size_t i = garbler_input_count; i < gc.circuit.input_wires.size(); ++i) {
            evaluator_wire_indices.push_back(gc.circuit.input_wires[i]);
        }
        
        // Get label pairs for OT
        auto label_pairs = garbler.get_ot_input_pairs(gc, evaluator_wire_indices);
        
        try {
            OTHandler ot;
            ot.init_sender(*protocol.connection);
            if(!ot.send_ot(label_pairs, *protocol.connection)){
                throw std::runtime_error("SimplestOT send_ot reported failure");
            }
            std::cout << "           OT invoked for " << evaluator_input_count << " wires" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "OT failed: " << e.what() << std::endl;
            throw;
        }
    }
};

int main(int argc, char* argv[]) {
    // Initialize OpenSSL
    OpenSSLContext crypto_context;
    
    std::cout << "Garbled Circuits Garbler" << std::endl;
    std::cout << "==============================" << std::endl;
    
    GarblerProgram program;
    return program.run(argc, argv);
}
