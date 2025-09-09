# Create garbler.cpp
garbler_cpp = """#include "common.h"
#include "garbled_circuit.h"
#include "socket_utils.h"
#include "ot_handler.h"
#include <iostream>
#include <fstream>
#include <getopt.h>

/**
 * Garbler program - acts as server in the protocol
 * Responsibilities:
 * 1. Load and garble the circuit
 * 2. Listen for evaluator connection
 * 3. Send garbled circuit to evaluator
 * 4. Perform OT for evaluator's inputs
 * 5. Receive and display final result
 */
class GarblerProgram {
public:
    GarblerProgram() : port(DEFAULT_PORT), verbose(false) {}
    
    int run(int argc, char* argv[]) {
        try {
            if (!parse_arguments(argc, argv)) {
                return 1;
            }
            
            if (verbose) {
                std::cout << "Starting garbler with parameters:" << std::endl;
                std::cout << "  Port: " << port << std::endl;
                std::cout << "  Circuit: " << circuit_file << std::endl;
                std::cout << "  Input: " << input_string << std::endl;
            }
            
            // Load circuit
            auto circuit = load_circuit();
            if (verbose) {
                CircuitUtils::print_circuit(circuit);
            }
            
            // Parse garbler inputs
            auto garbler_inputs = parse_inputs();
            
            // Garble circuit
            Garbler garbler;
            auto garbled_circuit = garbler.garble_circuit(circuit);
            
            if (verbose) {
                std::cout << "Circuit garbled successfully!" << std::endl;
            }
            
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
    bool verbose;
    
    bool parse_arguments(int argc, char* argv[]) {
        static struct option long_options[] = {
            {"port", required_argument, 0, 'p'},
            {"circuit", required_argument, 0, 'c'},
            {"input", required_argument, 0, 'i'},
            {"verbose", no_argument, 0, 'v'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        
        int opt;
        int option_index = 0;
        
        while ((opt = getopt_long(argc, argv, "p:c:i:vh", long_options, &option_index)) != -1) {
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
                case 'v':
                    verbose = true;
                    break;
                case 'h':
                    print_help();
                    return false;
                default:
                    print_help();
                    return false;
            }
        }
        
        if (circuit_file.empty()) {
            std::cerr << "Error: Circuit file is required" << std::endl;
            print_help();
            return false;
        }
        
        return true;
    }
    
    void print_help() {
        std::cout << "Garbled Circuits Garbler" << std::endl;
        std::cout << "Usage: garbler [OPTIONS]" << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -p, --port PORT        Port to listen on (default: " << DEFAULT_PORT << ")" << std::endl;
        std::cout << "  -c, --circuit FILE     Circuit description file (required)" << std::endl;
        std::cout << "  -i, --input BITS       Garbler's input bits (e.g., '101')" << std::endl;
        std::cout << "  -v, --verbose          Enable verbose output" << std::endl;
        std::cout << "  -h, --help             Show this help message" << std::endl;
        std::cout << std::endl;
        std::cout << "Example:" << std::endl;
        std::cout << "  garbler -p 8080 -c examples/and_gate.txt -i 1" << std::endl;
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
        
        // Step 1: Exchange hello messages
        protocol.send_hello("Garbler");
        std::string evaluator_name = protocol.receive_hello();
        std::cout << "Connected to: " << evaluator_name << std::endl;
        
        // Step 2: Send garbled circuit
        if (verbose) {
            std::cout << "Sending garbled circuit..." << std::endl;
        }
        protocol.send_circuit(gc);
        
        // Step 3: Send garbler's input labels
        std::vector<int> garbler_wire_indices;
        for (size_t i = 0; i < garbler_inputs.size(); ++i) {
            garbler_wire_indices.push_back(gc.circuit.input_wires[i]);
        }
        
        if (!garbler_inputs.empty()) {
            auto garbler_labels = garbler.encode_inputs(gc, garbler_inputs, garbler_wire_indices);
            protocol.send_input_labels(garbler_labels);
            
            if (verbose) {
                std::cout << "Sent " << garbler_labels.size() << " input labels" << std::endl;
            }
        }
        
        // Step 4: Perform OT for evaluator's inputs
        size_t evaluator_input_count = gc.circuit.num_inputs - garbler_inputs.size();
        if (evaluator_input_count > 0) {
            perform_ot_for_evaluator(protocol, gc, garbler, evaluator_input_count);
        }
        
        // Step 5: Receive result
        if (verbose) {
            std::cout << "Waiting for evaluation result..." << std::endl;
        }
        
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
        
        std::cout << "\\n=== RESULT ===" << std::endl;
        std::cout << "Output: ";
        for (bool bit : final_result) {
            std::cout << (bit ? '1' : '0');
        }
        std::cout << std::endl;
        
        // For debugging, also show as integer if single output
        if (final_result.size() == 1) {
            std::cout << "Output (decimal): " << (final_result[0] ? 1 : 0) << std::endl;
        } else if (final_result.size() <= 32) {
            int value = CircuitUtils::bits_to_int(final_result);
            std::cout << "Output (decimal): " << value << std::endl;
        }
        
        protocol.send_goodbye();
    }
    
    void perform_ot_for_evaluator(ProtocolManager& protocol, 
                                 const GarbledCircuit& gc,
                                 Garbler& garbler,
                                 size_t evaluator_input_count) {
        
        if (verbose) {
            std::cout << "Performing OT for " << evaluator_input_count << " evaluator inputs..." << std::endl;
        }
        
        // Get the wire indices for evaluator's inputs
        std::vector<int> evaluator_wire_indices;
        size_t garbler_input_count = gc.circuit.num_inputs - evaluator_input_count;
        
        for (size_t i = garbler_input_count; i < gc.circuit.input_wires.size(); ++i) {
            evaluator_wire_indices.push_back(gc.circuit.input_wires[i]);
        }
        
        // Get label pairs for OT
        auto label_pairs = garbler.get_ot_input_pairs(gc, evaluator_wire_indices);
        
        // For this simplified implementation, we'll use the simple OT
        // In a real implementation, this would use libOTe
        try {
            SimpleOT::send_batch_ot(label_pairs, *protocol.connection);
            
            if (verbose) {
                std::cout << "OT completed successfully" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "OT failed: " << e.what() << std::endl;
            throw;
        }
    }
};

int main(int argc, char* argv[]) {
    // Initialize OpenSSL
    OpenSSLContext crypto_context;
    
    std::cout << "Garbled Circuits Garbler v1.0" << std::endl;
    std::cout << "==============================" << std::endl;
    
    GarblerProgram program;
    return program.run(argc, argv);
}
"""

with open("garbled_circuits_utility/src/garbler.cpp", "w") as f:
    f.write(garbler_cpp)

# Create evaluator.cpp
evaluator_cpp = """#include "common.h"
#include "garbled_circuit.h"
#include "socket_utils.h"
#include "ot_handler.h"
#include <iostream>
#include <getopt.h>

/**
 * Evaluator program - acts as client in the protocol
 * Responsibilities:
 * 1. Connect to garbler
 * 2. Receive garbled circuit
 * 3. Perform OT to get input labels
 * 4. Evaluate the garbled circuit
 * 5. Send result back to garbler
 */
class EvaluatorProgram {
public:
    EvaluatorProgram() : hostname("localhost"), port(DEFAULT_PORT), verbose(false) {}
    
    int run(int argc, char* argv[]) {
        try {
            if (!parse_arguments(argc, argv)) {
                return 1;
            }
            
            if (verbose) {
                std::cout << "Starting evaluator with parameters:" << std::endl;
                std::cout << "  Host: " << hostname << std::endl;
                std::cout << "  Port: " << port << std::endl;
                std::cout << "  Input: " << input_string << std::endl;
            }
            
            // Parse evaluator inputs
            auto evaluator_inputs = parse_inputs();
            
            // Connect to garbler
            auto connection = std::make_unique<SocketConnection>(hostname, port);
            auto protocol = ProtocolManager(std::move(connection));
            
            // Execute protocol
            execute_protocol(protocol, evaluator_inputs);
            
            std::cout << "Protocol completed successfully!" << std::endl;
            return 0;
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }

private:
    std::string hostname;
    std::string input_string;
    int port;
    bool verbose;
    
    bool parse_arguments(int argc, char* argv[]) {
        static struct option long_options[] = {
            {"host", required_argument, 0, 'H'},
            {"port", required_argument, 0, 'p'},
            {"input", required_argument, 0, 'i'},
            {"verbose", no_argument, 0, 'v'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        
        int opt;
        int option_index = 0;
        
        while ((opt = getopt_long(argc, argv, "H:p:i:vh", long_options, &option_index)) != -1) {
            switch (opt) {
                case 'H':
                    hostname = optarg;
                    break;
                case 'p':
                    port = std::stoi(optarg);
                    break;
                case 'i':
                    input_string = optarg;
                    break;
                case 'v':
                    verbose = true;
                    break;
                case 'h':
                    print_help();
                    return false;
                default:
                    print_help();
                    return false;
            }
        }
        
        return true;
    }
    
    void print_help() {
        std::cout << "Garbled Circuits Evaluator" << std::endl;
        std::cout << "Usage: evaluator [OPTIONS]" << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -H, --host HOST        Garbler hostname (default: localhost)" << std::endl;
        std::cout << "  -p, --port PORT        Port to connect to (default: " << DEFAULT_PORT << ")" << std::endl;
        std::cout << "  -i, --input BITS       Evaluator's input bits (e.g., '101')" << std::endl;
        std::cout << "  -v, --verbose          Enable verbose output" << std::endl;
        std::cout << "  -h, --help             Show this help message" << std::endl;
        std::cout << std::endl;
        std::cout << "Example:" << std::endl;
        std::cout << "  evaluator -H localhost -p 8080 -i 0" << std::endl;
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
                         const std::vector<bool>& evaluator_inputs) {
        
        // Step 1: Exchange hello messages
        protocol.send_hello("Evaluator");
        std::string garbler_name = protocol.receive_hello();
        std::cout << "Connected to: " << garbler_name << std::endl;
        
        // Step 2: Receive garbled circuit
        if (verbose) {
            std::cout << "Receiving garbled circuit..." << std::endl;
        }
        
        auto garbled_circuit = protocol.receive_circuit();
        
        if (verbose) {
            std::cout << "Received garbled circuit with " << garbled_circuit.circuit.num_gates << " gates" << std::endl;
            CircuitUtils::print_circuit(garbled_circuit.circuit);
        }
        
        // Step 3: Receive garbler's input labels (if any)
        std::vector<WireLabel> all_input_labels;
        size_t garbler_input_count = garbled_circuit.circuit.num_inputs - evaluator_inputs.size();
        
        if (garbler_input_count > 0) {
            if (verbose) {
                std::cout << "Receiving " << garbler_input_count << " garbler input labels..." << std::endl;
            }
            
            auto garbler_labels = protocol.receive_input_labels(garbler_input_count);
            all_input_labels.insert(all_input_labels.end(), garbler_labels.begin(), garbler_labels.end());
            
            if (verbose) {
                std::cout << "Received " << garbler_labels.size() << " input labels from garbler" << std::endl;
            }
        }
        
        // Step 4: Perform OT to get evaluator's input labels
        if (!evaluator_inputs.empty()) {
            auto evaluator_labels = perform_ot_for_inputs(protocol, evaluator_inputs);
            all_input_labels.insert(all_input_labels.end(), evaluator_labels.begin(), evaluator_labels.end());
            
            if (verbose) {
                std::cout << "Obtained " << evaluator_labels.size() << " input labels via OT" << std::endl;
            }
        }
        
        // Step 5: Evaluate the garbled circuit
        if (verbose) {
            std::cout << "Evaluating garbled circuit..." << std::endl;
        }
        
        Evaluator evaluator;
        auto output_labels = evaluator.evaluate_circuit(garbled_circuit, all_input_labels);
        
        if (verbose) {
            auto stats = evaluator.get_evaluation_stats();
            std::cout << "Evaluation completed:" << std::endl;
            std::cout << "  Gates evaluated: " << stats.gates_evaluated << std::endl;
            std::cout << "  Decryption attempts: " << stats.decryption_attempts << std::endl;
            std::cout << "  Successful decryptions: " << stats.successful_decryptions << std::endl;
            std::cout << "  Time: " << stats.total_time.count() << " microseconds" << std::endl;
        }
        
        // Step 6: Send result back to garbler
        std::vector<uint8_t> result_data;
        for (const auto& label : output_labels) {
            result_data.insert(result_data.end(), label.begin(), label.end());
        }
        
        protocol.send_result(result_data);
        
        if (verbose) {
            std::cout << "Sent evaluation result to garbler" << std::endl;
        }
        
        // Wait for goodbye
        auto msg = protocol.receive_any_message();
        if (msg.type == MessageType::GOODBYE) {
            if (verbose) {
                std::cout << "Received goodbye from garbler" << std::endl;
            }
        }
    }
    
    std::vector<WireLabel> perform_ot_for_inputs(ProtocolManager& protocol,
                                                const std::vector<bool>& evaluator_inputs) {
        
        if (verbose) {
            std::cout << "Performing OT for " << evaluator_inputs.size() << " inputs..." << std::endl;
        }
        
        try {
            // For this simplified implementation, we'll use the simple OT
            // In a real implementation, this would use libOTe
            auto labels = SimpleOT::receive_batch_ot(evaluator_inputs, *protocol.connection);
            
            if (verbose) {
                std::cout << "OT completed successfully" << std::endl;
            }
            
            return labels;
        } catch (const std::exception& e) {
            std::cerr << "OT failed: " << e.what() << std::endl;
            throw;
        }
    }
};

int main(int argc, char* argv[]) {
    // Initialize OpenSSL
    OpenSSLContext crypto_context;
    
    std::cout << "Garbled Circuits Evaluator v1.0" << std::endl;
    std::cout << "================================" << std::endl;
    
    EvaluatorProgram program;
    return program.run(argc, argv);
}
"""

with open("garbled_circuits_utility/src/evaluator.cpp", "w") as f:
    f.write(evaluator_cpp)

print("garbler.cpp and evaluator.cpp created successfully!")
print(f"Garbler length: {len(garbler_cpp)} characters")
print(f"Evaluator length: {len(evaluator_cpp)} characters")