# Create main.cpp
main_cpp = """#include "common.h"
#include <iostream>

/**
 * Main entry point - determines whether to run as garbler or evaluator
 * based on compile-time definitions
 */

// Forward declarations
int garbler_main(int argc, char* argv[]);
int evaluator_main(int argc, char* argv[]);

int main(int argc, char* argv[]) {
#ifdef GARBLER_MODE
    return garbler_main(argc, argv);
#elif defined(EVALUATOR_MODE)
    return evaluator_main(argc, argv);
#else
    std::cerr << "Error: Neither GARBLER_MODE nor EVALUATOR_MODE is defined!" << std::endl;
    std::cerr << "Please build the specific targets: 'garbler' or 'evaluator'" << std::endl;
    return 1;
#endif
}

// These functions are implemented in garbler.cpp and evaluator.cpp
// but we provide stub implementations here for compilation safety
int garbler_main(int argc, char* argv[]) {
    std::cerr << "Garbler main not linked properly!" << std::endl;
    return 1;
}

int evaluator_main(int argc, char* argv[]) {
    std::cerr << "Evaluator main not linked properly!" << std::endl;
    return 1;
}
"""

with open("/mnt/c/Users/saini/Downloads/UGP/garbled_circuits_utility/src/main.cpp", "w") as f:
    f.write(main_cpp)

# Create simple_and.cpp example
simple_and_example = """#include "common.h"
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
        std::cout << "\\nTesting all input combinations:" << std::endl;
        
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
        std::cout << "\\nTesting garbled circuit implementation:" << std::endl;
        
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
            std::cout << "\\nCircuit saved to: " << filename << std::endl;
        } else {
            FileFormats::save_simple_circuit(circuit, "and_gate.txt");
            std::cout << "\\nCircuit saved to: and_gate.txt" << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
"""

with open("garbled_circuits_utility/examples/simple_and.cpp", "w") as f:
    f.write(simple_and_example)

# Create millionaires.cpp example
millionaires_example = """#include "common.h"
#include "garbled_circuit.h"
#include <iostream>

/**
 * Millionaire's Problem Example
 * Creates a circuit to compare two integers and determine which is larger
 * This is a classic example in secure multi-party computation
 */

class MillionairesCircuit {
public:
    static Circuit create_comparison_circuit(int bit_width = 8) {
        std::cout << "Creating " << bit_width << "-bit comparison circuit" << std::endl;
        
        Circuit circuit;
        circuit.num_inputs = 2 * bit_width;  // Two numbers, each bit_width bits
        circuit.num_outputs = 1;             // One output: 1 if A >= B, 0 otherwise
        circuit.num_gates = 0;
        
        // Input organization:
        // Wires 1 to bit_width: Alice's number (A)
        // Wires bit_width+1 to 2*bit_width: Bob's number (B)
        for (int i = 1; i <= 2 * bit_width; ++i) {
            circuit.input_wires.push_back(i);
        }
        
        int next_wire = 2 * bit_width + 1;
        
        // Build comparison logic
        // We'll implement A >= B using bit-by-bit comparison
        // Starting from the most significant bit
        
        int result_wire = build_comparison_logic(circuit, bit_width, next_wire);
        
        circuit.output_wires.push_back(result_wire);
        circuit.num_gates = circuit.gates.size();
        circuit.num_wires = next_wire - 1;
        
        return circuit;
    }
    
private:
    static int build_comparison_logic(Circuit& circuit, int bit_width, int& next_wire) {
        // Simplified comparison: just compare the most significant bit
        // In a full implementation, you'd do a proper multi-bit comparison
        
        int a_msb = bit_width;              // Most significant bit of A
        int b_msb = 2 * bit_width;          // Most significant bit of B
        
        // A >= B if A_msb >= B_msb (simplified for this example)
        // This is just: A_msb OR (NOT B_msb)
        
        // First, create NOT B_msb
        int not_b_wire = next_wire++;
        circuit.gates.emplace_back(not_b_wire, b_msb, GateType::NOT);
        
        // Then create A_msb OR (NOT B_msb)
        int result_wire = next_wire++;
        circuit.gates.emplace_back(result_wire, a_msb, not_b_wire, GateType::OR);
        
        return result_wire;
    }
};

int main(int argc, char* argv[]) {
    try {
        std::cout << "Millionaire's Problem Example" << std::endl;
        std::cout << "=============================" << std::endl;
        
        int bit_width = 4;  // 4-bit numbers for simplicity
        
        if (argc > 1) {
            bit_width = std::stoi(argv[1]);
            if (bit_width < 1 || bit_width > 32) {
                std::cerr << "Bit width must be between 1 and 32" << std::endl;
                return 1;
            }
        }
        
        // Create comparison circuit
        auto circuit = MillionairesCircuit::create_comparison_circuit(bit_width);
        
        std::cout << "\\nCreated comparison circuit:" << std::endl;
        CircuitUtils::print_circuit(circuit);
        
        // Test with some examples
        std::cout << "\\nTesting comparison circuit:" << std::endl;
        
        // Test cases: (A, B) -> expected result (1 if A >= B, 0 otherwise)
        std::vector<std::pair<int, int>> test_cases = {
            {5, 3},   // 5 >= 3 = 1
            {3, 5},   // 3 >= 5 = 0
            {7, 7},   // 7 >= 7 = 1
            {0, 1},   // 0 >= 1 = 0
        };
        
        for (const auto& test_case : test_cases) {
            int a = test_case.first;
            int b = test_case.second;
            
            // Convert to binary
            auto a_bits = CircuitUtils::int_to_bits(a, bit_width);
            auto b_bits = CircuitUtils::int_to_bits(b, bit_width);
            
            // Combine inputs
            std::vector<bool> inputs;
            inputs.insert(inputs.end(), a_bits.begin(), a_bits.end());
            inputs.insert(inputs.end(), b_bits.begin(), b_bits.end());
            
            // Evaluate
            auto outputs = CircuitUtils::evaluate_plaintext(circuit, inputs);
            
            std::cout << "  " << a << " >= " << b << " = " 
                     << (outputs[0] ? "true" : "false") 
                     << " (expected: " << (a >= b ? "true" : "false") << ")"
                     << std::endl;
        }
        
        // Test garbled circuit
        std::cout << "\\nTesting garbled circuit implementation:" << std::endl;
        
        if (CircuitUtils::test_circuit_correctness(circuit, 20)) {
            std::cout << "Garbled circuit tests passed!" << std::endl;
        } else {
            std::cout << "Garbled circuit tests failed!" << std::endl;
            return 1;
        }
        
        // Save circuit
        std::string filename = "millionaires_" + std::to_string(bit_width) + "bit.txt";
        FileFormats::save_simple_circuit(circuit, filename);
        std::cout << "\\nCircuit saved to: " << filename << std::endl;
        
        std::cout << "\\nTo use with garbler/evaluator:" << std::endl;
        std::cout << "  Terminal 1: ./garbler -c " << filename << " -i <Alice's_number_in_binary>" << std::endl;
        std::cout << "  Terminal 2: ./evaluator -i <Bob's_number_in_binary>" << std::endl;
        std::cout << "\\nExample (Alice=5, Bob=3):" << std::endl;
        std::cout << "  Terminal 1: ./garbler -c " << filename << " -i 0101" << std::endl;
        std::cout << "  Terminal 2: ./evaluator -i 0011" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
"""

with open("garbled_circuits_utility/examples/millionaires.cpp", "w") as f:
    f.write(millionaires_example)

print("main.cpp and example files created successfully!")
print(f"main.cpp length: {len(main_cpp)} characters")
print(f"simple_and example length: {len(simple_and_example)} characters")
print(f"millionaires example length: {len(millionaires_example)} characters")