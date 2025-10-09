#include "common.h"
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
        
        // Display protocol information  
        std::cout << "\n=== GARBLED CIRCUIT PROTOCOL ===" << std::endl;
        std::cout << "Evaluator Input: ";
        for (bool bit : evaluator_inputs) {
            std::cout << (bit ? '1' : '0');
        }
        std::cout << " (decimal: " << CircuitUtils::bits_to_int(evaluator_inputs) << ")" << std::endl;
        
        // Step 2: Receive garbled circuit
        std::cout << "\n[STEP 1] Receiving garbled circuit from garbler..." << std::endl;
        
        auto garbled_circuit = protocol.receive_circuit();
        
        // Step 3: Receive garbler's input labels (if any)
        std::vector<WireLabel> all_input_labels;
        size_t garbler_input_count = garbled_circuit.circuit.num_inputs - evaluator_inputs.size();
        
        if (garbler_input_count > 0) {
            std::cout << "[STEP 2] Receiving garbler's input labels..." << std::endl;
            
            auto garbler_labels = protocol.receive_input_labels(garbler_input_count);
            all_input_labels.insert(all_input_labels.end(), garbler_labels.begin(), garbler_labels.end());
            std::cout << "           Received " << garbler_labels.size() << " wire labels for garbler's inputs" << std::endl;
        }
        
        // Step 4: Perform OT to get evaluator's input labels
        if (!evaluator_inputs.empty()) {
            std::cout << "[STEP 3] Performing OT to obtain evaluator's input labels..." << std::endl;
            auto evaluator_labels = perform_ot_for_inputs(protocol, evaluator_inputs);
            all_input_labels.insert(all_input_labels.end(), evaluator_labels.begin(), evaluator_labels.end());
            std::cout << "           Obtained " << evaluator_labels.size() << " wire labels via OT" << std::endl;
        }
        
        // Step 5: Evaluate the garbled circuit
        std::cout << "[STEP 4] Evaluating garbled circuit..." << std::endl;
        
        Evaluator evaluator;
        auto output_labels = evaluator.evaluate_circuit(garbled_circuit, all_input_labels);
        
        auto stats = evaluator.get_evaluation_stats();
        std::cout << "           Successfully evaluated " << stats.gates_evaluated << " gates" << std::endl;
        std::cout << "           Evaluation time: " << stats.total_time.count() << " microseconds" << std::endl;
        
        // Step 6: Send result back to garbler
        std::cout << "[STEP 5] Sending evaluation result to garbler..." << std::endl;
        std::vector<uint8_t> result_data;
        for (const auto& label : output_labels) {
            result_data.insert(result_data.end(), label.begin(), label.end());
        }
        
        protocol.send_result(result_data);
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
            std::cerr << "OT failed (SimplestOT): " << e.what() << std::endl;
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
