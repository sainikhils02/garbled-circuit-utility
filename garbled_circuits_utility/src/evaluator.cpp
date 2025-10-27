#include "common.h"
#include "garbled_circuit.h"
#include "socket_utils.h"
#include "ot_handler.h"
#include <iostream>
#include <getopt.h>
#include <chrono>

/**
 * Responsibilities:
 * 1. Connect to garbler
 * 2. Receive garbled circuit
 * 3. Perform OT to get input labels
 * 4. Evaluate the garbled circuit
 * 5. Send result back to garbler
 */
class EvaluatorProgram {
public:
    EvaluatorProgram() : hostname("localhost"), port(DEFAULT_PORT) {}
    
    int run(int argc, char* argv[]) {
        try {
            if (!parse_arguments(argc, argv)) {
                return 1;
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
    bool use_pandp = false;
    
    
    bool parse_arguments(int argc, char* argv[]) {
        static struct option long_options[] = {
            {"host", required_argument, 0, 'H'},
            {"port", required_argument, 0, 'p'},
            {"input", required_argument, 0, 'i'},
            {"pandp", no_argument, 0, 0},
            {0, 0, 0, 0}
        };
        
        int opt;
        int option_index = 0;
        
        while ((opt = getopt_long(argc, argv, "H:p:i:", long_options, &option_index)) != -1) {
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
                case 0:
                    if (std::string(long_options[option_index].name) == "pandp") {
                        use_pandp = true;
                    }
                    break;
                default:
                    return false;
            }
        }
        
        return true;
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
        
        // Step 0: Exchange hello messages
        protocol.send_hello("Evaluator");
        std::string garbler_name = protocol.receive_hello();
        std::cout << "Connected to: " << garbler_name << std::endl;
        
        // Display protocol information  
        std::cout << "\n=== GARBLED CIRCUIT PROTOCOL ===" << std::endl;
        std::cout << "Evaluator Input: ";
        for (bool bit : evaluator_inputs) {
            std::cout << (bit ? '1' : '0');
        }
        std::cout << " (decimal: " << CircuitUtils::bits_to_int(evaluator_inputs) << ")" << std::endl;
        
        // Step 1: Receive garbled circuit
        std::cout << "\n[STEP 1] Receiving garbled circuit from garbler..." << std::endl;
        auto rc0 = std::chrono::high_resolution_clock::now();
        auto garbled_circuit = protocol.receive_circuit();
        auto rc1 = std::chrono::high_resolution_clock::now();
        std::cout << "           Received circuit in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(rc1 - rc0).count()
                  << " ms" << std::endl;

        size_t expected_garbler_inputs = 0;
        size_t expected_evaluator_inputs = evaluator_inputs.size();
        if (!garbled_circuit.circuit.input_partition.empty()) {
            expected_garbler_inputs = static_cast<size_t>(garbled_circuit.circuit.input_partition.front());
            expected_evaluator_inputs = 0;
            for (size_t idx = 1; idx < garbled_circuit.circuit.input_partition.size(); ++idx) {
                expected_evaluator_inputs += static_cast<size_t>(garbled_circuit.circuit.input_partition[idx]);
            }
        } else {
            if (garbled_circuit.circuit.num_inputs < static_cast<int>(evaluator_inputs.size())) {
                throw std::invalid_argument("Circuit declares fewer input wires than bits provided by evaluator");
            }
            expected_garbler_inputs = static_cast<size_t>(garbled_circuit.circuit.num_inputs) - evaluator_inputs.size();
        }

        if (static_cast<size_t>(garbled_circuit.circuit.num_inputs) != expected_garbler_inputs + expected_evaluator_inputs) {
            throw std::invalid_argument("Circuit input partition does not match declared input count");
        }

        if (evaluator_inputs.size() != expected_evaluator_inputs) {
            throw std::invalid_argument("Evaluator provided " + std::to_string(evaluator_inputs.size()) +
                                         " bits but circuit expects " + std::to_string(expected_evaluator_inputs));
        }
        
        // Step 2: Receive garbler's input labels
    std::vector<WireLabel> all_input_labels;
    all_input_labels.reserve(garbled_circuit.circuit.input_wires.size());
        size_t garbler_input_count = expected_garbler_inputs;
        
        if (garbler_input_count > 0) {
            std::cout << "[STEP 2] Receiving garbler's input labels..." << std::endl;
            
            auto garbler_labels = protocol.receive_input_labels(garbler_input_count);
            all_input_labels.insert(all_input_labels.end(), garbler_labels.begin(), garbler_labels.end());
            std::cout << "           Received " << garbler_labels.size() << " wire labels for garbler's inputs" << std::endl;
        }
        
        // Step 3: Perform OT to get evaluator's input labels
        if (!evaluator_inputs.empty()) {
            std::cout << "[STEP 3] Performing OT to obtain evaluator's input labels..." << std::endl;
            auto ot0 = std::chrono::high_resolution_clock::now();
            auto evaluator_labels = perform_ot_for_inputs(protocol, evaluator_inputs);
            auto ot1 = std::chrono::high_resolution_clock::now();
            std::cout << "           OT completed in "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(ot1 - ot0).count()
                      << " ms" << std::endl;
            all_input_labels.insert(all_input_labels.end(), evaluator_labels.begin(), evaluator_labels.end());
            std::cout << "           Obtained " << evaluator_labels.size() << " wire labels via OT" << std::endl;
        }
        
    // Step 4: Evaluate the garbled circuit
    std::cout << "[STEP 4] Evaluating garbled circuit..." << std::endl;
    if (use_pandp) std::cout << "           Point-and-Permute: ENABLED" << std::endl;
        
    Evaluator evaluator(use_pandp);
    auto ev0 = std::chrono::high_resolution_clock::now();
    auto output_labels = evaluator.evaluate_circuit(garbled_circuit, all_input_labels);
    auto ev1 = std::chrono::high_resolution_clock::now();
    std::cout << "           Evaluation completed in "
          << std::chrono::duration_cast<std::chrono::milliseconds>(ev1 - ev0).count()
          << " ms" << std::endl;
        
        auto stats = evaluator.get_evaluation_stats();
        std::cout << "           Successfully evaluated " << stats.gates_evaluated << " gates" << std::endl;
        std::cout << "           Decrypted " << stats.cipher_decryptions << " ciphers" << std::endl;
        std::cout << "           Evaluation time: " << stats.total_time.count() << " microseconds" << std::endl;
        
        // Step 5: Send result back to garbler
        std::cout << "[STEP 5] Sending evaluation result to garbler..." << std::endl;
        std::vector<uint8_t> result_data;
        for (const auto& label : output_labels) {
            result_data.insert(result_data.end(), label.begin(), label.end());
        }
        
    auto sr0 = std::chrono::high_resolution_clock::now();
    protocol.send_result(result_data);
    auto sr1 = std::chrono::high_resolution_clock::now();
    std::cout << "           Sent result in "
          << std::chrono::duration_cast<std::chrono::milliseconds>(sr1 - sr0).count()
          << " ms" << std::endl;
        std::cout << "           Result transmission completed" << std::endl;
        
        std::cout << "\n=== PROTOCOL COMPLETED ===" << std::endl;
        
        // Wait for goodbye
        auto msg = protocol.receive_any_message();
        if (msg.type == MessageType::GOODBYE) {
            std::cout << "Protocol terminated successfully" << std::endl;
        }
    }
    
    std::vector<WireLabel> perform_ot_for_inputs(ProtocolManager& protocol,
                                                const std::vector<bool>& evaluator_inputs) {
        
        try {
            OTHandler ot;
            ot.init_receiver(*protocol.connection);
            auto labels = ot.receive_ot(evaluator_inputs, *protocol.connection);
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
    
    std::cout << "Garbled Circuits Evaluator" << std::endl;
    std::cout << "================================" << std::endl;
    
    EvaluatorProgram program;
    return program.run(argc, argv);
}
