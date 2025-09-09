#include "common.h"
#include "garbled_circuit.h"
#include <iostream>

/**
 * Simple example demonstrating AND gate circuit
 * This can be run as a standalone test or used to generate circuit files
 */

int main(int argc, char* argv[]) {
    try {
        std::cout << "Simple AND Gate Example" << std::endl;
        std::cout << "=======================" << std::endl;
        
        // Create AND gate circuit
        auto circuit = GarbledCircuitManager::create_and_gate_circuit();
        
        std::cout << "Created AND gate circuit:" << std::endl;
        CircuitUtils::print_circuit(circuit);
        
        // Test the circuit with all possible inputs
        std::cout << "\nTesting all input combinations:" << std::endl;
        
        std::vector<std::vector<bool>> test_cases = {
            {false, false},  // 0 AND 0 = 0
            {false, true},   // 0 AND 1 = 0
            {true, false},   // 1 AND 0 = 0
            {true, true}     // 1 AND 1 = 1
        };
        
        for (const auto& inputs : test_cases) {
            auto outputs = CircuitUtils::evaluate_plaintext(circuit, inputs);
            
            std::cout << "  " << (inputs[0] ? '1' : '0') << " AND " 
                     << (inputs[1] ? '1' : '0') << " = " 
                     << (outputs[0] ? '1' : '0') << std::endl;
        }
        
        // Test garbled circuit implementation
        std::cout << "\nTesting garbled circuit implementation:" << std::endl;
        
        if (CircuitUtils::test_circuit_correctness(circuit, 10)) {
            std::cout << "All tests passed!" << std::endl;
        } else {
            std::cout << "Some tests failed!" << std::endl;
            return 1;
        }
        
        // Save circuit to file for use with garbler/evaluator
        if (argc > 1) {
            std::string filename = argv[1];
            FileFormats::save_simple_circuit(circuit, filename);
            std::cout << "\nCircuit saved to: " << filename << std::endl;
        } else {
            FileFormats::save_simple_circuit(circuit, "and_gate.txt");
            std::cout << "\nCircuit saved to: and_gate.txt" << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
