#pragma once

#include "common.h"
#include "crypto_utils.h"
#include <fstream>
#include <sstream>
#include <chrono>

/**
 * Core garbled circuits implementation
 * Handles circuit parsing, garbling, and evaluation
 */
class GarbledCircuitManager {
public:
    GarbledCircuitManager();
    ~GarbledCircuitManager() = default;
    
    // Non-copyable but movable
    GarbledCircuitManager(const GarbledCircuitManager&) = delete;
    GarbledCircuitManager& operator=(const GarbledCircuitManager&) = delete;
    GarbledCircuitManager(GarbledCircuitManager&& other) noexcept = default;
    GarbledCircuitManager& operator=(GarbledCircuitManager&& other) noexcept = default;
    
    /**
     * Circuit loading and parsing
     */
    
    // Load circuit from file
    Circuit load_circuit_from_file(const std::string& filename);
    
    // Parse circuit from string
    Circuit parse_circuit(const std::string& circuit_description);
    
    // Save circuit to file
    void save_circuit_to_file(const Circuit& circuit, const std::string& filename);
    
    // Convert circuit to string representation
    std::string circuit_to_string(const Circuit& circuit);
    
    /**
     * Circuit validation and analysis
     */
    
    // Validate circuit structure
    bool validate_circuit(const Circuit& circuit);
    
    // Get circuit statistics
    struct CircuitStats {
        int total_wires;
        int gate_counts[8]; // Count for each gate type
        int circuit_depth;
        int critical_path_length;
    };
    CircuitStats analyze_circuit(const Circuit& circuit);
    
    // Check if circuit is well-formed
    bool is_well_formed(const Circuit& circuit);
    
    /**
     * Circuit transformation
     */
    
    // Convert to NAND-only circuit
    Circuit convert_to_nand_only(const Circuit& circuit);
    
    // Optimize circuit (basic optimizations)
    Circuit optimize_circuit(const Circuit& circuit);
    
    /**
     * Example circuits
     */
    
    // Create simple AND gate circuit
    static Circuit create_and_gate_circuit();
    
    // Create simple OR gate circuit
    static Circuit create_or_gate_circuit();
    
    // Create XOR gate circuit
    static Circuit create_xor_gate_circuit();
    
    // Create comparison circuit (for millionaire's problem)
    static Circuit create_comparison_circuit(int bit_width);
    
    // Create adder circuit
    static Circuit create_adder_circuit(int bit_width);

private:
    // Helper functions for parsing
    std::vector<std::string> split_string(const std::string& str, char delimiter);
    std::string trim_string(const std::string& str);
    void parse_circuit_line(const std::string& line, Circuit& circuit);
    
    // Helper functions for validation
    bool check_wire_consistency(const Circuit& circuit);
    bool check_gate_validity(const Circuit& circuit);
    
    // Helper functions for optimization
    Circuit remove_redundant_gates(const Circuit& circuit);
    Circuit merge_consecutive_gates(const Circuit& circuit);
};

/**
 * Garbler class - responsible for creating garbled circuits
 */
class Garbler {
public:
    explicit Garbler(bool use_pandp = false);
    ~Garbler() = default;
    
    /**
     * Main garbling functions
     */
    
    // Garble a circuit
    GarbledCircuit garble_circuit(const Circuit& circuit);
    
    // Get input encoding for garbler's inputs
    std::vector<WireLabel> encode_inputs(const GarbledCircuit& gc, 
                                       const std::vector<bool>& inputs,
                                       const std::vector<int>& wire_indices);
    
    // Decode outputs
    std::vector<bool> decode_outputs(const GarbledCircuit& gc, 
                                   const std::vector<WireLabel>& output_labels);
    
    /**
     * Wire label management
     */
    
    // Generate labels for all wires
    void generate_wire_labels(GarbledCircuit& gc);
    
    // Get input labels for OT (evaluator's inputs)
    std::vector<std::pair<WireLabel, WireLabel>> get_ot_input_pairs(
        const GarbledCircuit& gc, const std::vector<int>& wire_indices);
    
    /**
     * Statistics and information
     */
    
    // Get garbled circuit size
    size_t get_garbled_circuit_size(const GarbledCircuit& gc);
    
    // Print garbling statistics
    void print_garbling_stats(const GarbledCircuit& gc);

private:
    std::map<int, std::pair<WireLabel, WireLabel>> wire_labels; // wire_id -> (label0, label1)
    bool use_pandp_ = false;
    
    // Core garbling functions
    GarbledGate garble_gate(const Gate& gate, int gate_id);
    void assign_wire_labels(const Circuit& circuit);
    
    // Gate-specific garbling
    GarbledGate garble_and_gate(const Gate& gate, int gate_id);
    GarbledGate garble_or_gate(const Gate& gate, int gate_id);
    GarbledGate garble_xor_gate(const Gate& gate, int gate_id);
    GarbledGate garble_nand_gate(const Gate& gate, int gate_id);
    GarbledGate garble_not_gate(const Gate& gate, int gate_id);
    
    // Helper functions
    void generate_garbled_table(GarbledGate& garbled_gate,
                              const Gate& gate, 
                              int gate_id,
                              const WireLabel& out_label0,
                              const WireLabel& out_label1,
                              const WireLabel& in1_label0,
                              const WireLabel& in1_label1,
                              const WireLabel& in2_label0 = {},
                              const WireLabel& in2_label1 = {});
    
    void permute_garbled_table(GarbledGate& garbled_gate);
    static inline uint8_t perm_bit(const WireLabel& lbl) { return lbl[WIRE_LABEL_SIZE - 1] & 0x01; }
};

/**
 * Evaluator class - responsible for evaluating garbled circuits
 */
class Evaluator {
public:
    explicit Evaluator(bool use_pandp = false);
    ~Evaluator() = default;
    
    /**
     * Main evaluation functions
     */
    
    // Evaluate a garbled circuit
    std::vector<WireLabel> evaluate_circuit(const GarbledCircuit& gc,
                                          const std::vector<WireLabel>& input_labels);
    
    // Evaluate with mixed inputs (some labels, some plaintext for testing)
    std::vector<WireLabel> evaluate_with_mixed_inputs(const GarbledCircuit& gc,
                                                    const std::vector<WireLabel>& garbler_labels,
                                                    const std::vector<WireLabel>& evaluator_labels);
    
    /**
     * Gate evaluation
     */
    
    // Evaluate a single garbled gate
    WireLabel evaluate_gate(const GarbledGate& garbled_gate,
                          const WireLabel& input1_label,
                          const WireLabel& input2_label,
                          int gate_id);
    
    // Evaluate unary gate (like NOT)
    WireLabel evaluate_unary_gate(const GarbledGate& garbled_gate,
                                const WireLabel& input_label,
                                int gate_id);
    
    /**
     * Utility functions
     */
    
    // Validate evaluation inputs
    bool validate_inputs(const GarbledCircuit& gc, const std::vector<WireLabel>& inputs);
    
    // Get evaluation statistics
    struct EvaluationStats {
        int gates_evaluated = 0;
        int decryption_attempts = 0;
        int successful_decryptions = 0;
        int cipher_decryptions = 0;
        std::chrono::microseconds total_time{0};
    };
    
    EvaluationStats get_evaluation_stats() const { return eval_stats; }
    void reset_stats() { eval_stats = {}; }

private:
    EvaluationStats eval_stats;
    std::map<int, WireLabel> wire_values; // wire_id -> current label
    bool use_pandp_ = false;
    
    // Core evaluation functions
    WireLabel try_decrypt_gate(const GarbledGate& garbled_gate,
                             const WireLabel& input1,
                             const WireLabel& input2,
                             int gate_id);
    
    WireLabel try_decrypt_unary_gate(const GarbledGate& garbled_gate,
                                   const WireLabel& input,
                                   int gate_id);
    
    // Helper functions
    bool is_valid_gate_output(const std::vector<uint8_t>& decrypted_data);
    void update_evaluation_stats(bool success);
    static inline uint8_t perm_bit(const WireLabel& lbl) { return lbl[WIRE_LABEL_SIZE - 1] & 0x01; }
};

/**
 * Utility functions for circuit operations
 */
class CircuitUtils {
public:
    // Plaintext evaluation (for testing)
    static std::vector<bool> evaluate_plaintext(const Circuit& circuit, 
                                               const std::vector<bool>& inputs);
    
    // Compare garbled and plaintext evaluation results
    static bool verify_evaluation(const Circuit& circuit,
                                const std::vector<bool>& inputs,
                                const std::vector<bool>& garbled_output);
    
    // Generate random test inputs
    static std::vector<bool> generate_random_inputs(size_t num_inputs);
    
    // Convert between different representations
    static std::vector<bool> labels_to_bits(const std::vector<WireLabel>& labels,
                                           const GarbledCircuit& gc);
    
    static std::vector<bool> int_to_bits(int value, int bit_width);
    static int bits_to_int(const std::vector<bool>& bits);
    
    // Circuit testing
    static bool test_circuit_correctness(const Circuit& circuit, 
                                       size_t num_tests = 100);
    
    // Print functions
    static void print_circuit(const Circuit& circuit);
    static void print_gate(const Gate& gate, int index);
    static void print_inputs_outputs(const std::vector<bool>& inputs,
                                   const std::vector<bool>& outputs);
};

/**
 * File format handlers
 */
namespace FileFormats {
    // Bristol format parser/writer
    Circuit load_bristol_circuit(const std::string& filename);
    void save_bristol_circuit(const Circuit& circuit, const std::string& filename);
    
    // Simple format parser/writer (human-readable)
    Circuit load_simple_circuit(const std::string& filename);
    void save_simple_circuit(const Circuit& circuit, const std::string& filename);
    
    // Binary format for efficient storage
    Circuit load_binary_circuit(const std::string& filename);
    void save_binary_circuit(const Circuit& circuit, const std::string& filename);
}
