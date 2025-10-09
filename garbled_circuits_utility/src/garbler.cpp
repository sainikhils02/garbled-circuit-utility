#include "common.h"
#include "garbled_circuit.h"
#include "socket_utils.h"
#include "ot_handler.h"
#include <iostream>
#include <fstream>
#include <getopt.h>

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
                        
            // Load circuit
            auto circuit = load_circuit();
            
            // Parse garbler inputs
            auto garbler_inputs = parse_inputs();
            
            // Garble circuit
            Garbler garbler;
            auto garbled_circuit = garbler.garble_circuit(circuit);
            
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
    
    
    bool parse_arguments(int argc, char* argv[]) {
        static struct option long_options[] = {
            {"port", required_argument, 0, 'p'},
            {"circuit", required_argument, 0, 'c'},
            {"input", required_argument, 0, 'i'},
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
        
        // Step 1: Send garbled circuit
        std::cout << "\n[STEP 1] Sending garbled circuit to evaluator..." << std::endl;
        protocol.send_circuit(gc);
        
        // Step 2: Send garbler's input labels
        std::vector<int> garbler_wire_indices;
        for (size_t i = 0; i < garbler_inputs.size(); ++i) {
            garbler_wire_indices.push_back(gc.circuit.input_wires[i]);
        }
        
        if (!garbler_inputs.empty()) {
            std::cout << "[STEP 2] Sending garbler's input labels..." << std::endl;
            auto garbler_labels = garbler.encode_inputs(gc, garbler_inputs, garbler_wire_indices);
            protocol.send_input_labels(garbler_labels);
            std::cout << "           Sent " << garbler_labels.size() << " wire labels for garbler's inputs" << std::endl;
        }
        
        // Step 3: Perform OT for evaluator's inputs
        size_t evaluator_input_count = gc.circuit.num_inputs - garbler_inputs.size();
        if (evaluator_input_count > 0) {
            std::cout << "[STEP 3] Performing OT for evaluator's " << evaluator_input_count << " inputs..." << std::endl;
            perform_ot_for_evaluator(protocol, gc, garbler, evaluator_input_count);
        }
        
        // Step 4: Receive result
        std::cout << "[STEP 4] Waiting for evaluation result..." << std::endl;
        
        auto result_data = protocol.receive_result();
        
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
        auto final_result = garbler.decode_outputs(gc, output_labels);
        
        std::cout << "\n=== PROTOCOL RESULT ===" << std::endl;
        std::cout << "Circuit Output: ";
        for (bool bit : final_result) {
            std::cout << (bit ? '1' : '0');
        }
        std::cout << " (decimal: " << (final_result[0] ? 1 : 0) << ")" << std::endl;
        
        // Show Multi Party Computation summary
        std::cout << "\n=== COMPUTATION SUMMARY ===" << std::endl;
        std::cout << "Function computed: Garbler(" << CircuitUtils::bits_to_int(garbler_inputs) 
                  << ") ⊕ Evaluator(?) = " << (final_result[0] ? 1 : 0) << std::endl;
        
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
