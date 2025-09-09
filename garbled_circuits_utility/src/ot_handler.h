#pragma once

#include "common.h"
#include "socket_utils.h"

// Forward declarations for libOTe types
// Note: In a real implementation, these would include actual libOTe headers
namespace osuCrypto {
    class IOBase;
    class PRNG;
    class Channel;
}

/**
 * Wrapper for libOTe functionality
 * Provides a simple interface for oblivious transfer operations
 */
class OTHandler {
public:
    /**
     * Constructor
     */
    OTHandler();
    ~OTHandler();
    
    // Non-copyable but movable
    OTHandler(const OTHandler&) = delete;
    OTHandler& operator=(const OTHandler&) = delete;
    OTHandler(OTHandler&& other) noexcept;
    OTHandler& operator=(OTHandler&& other) noexcept;
    
    /**
     * Sender-side functions (for garbler)
     */
    
    // Initialize as OT sender
    void init_sender();
    
    // Perform 1-out-of-2 OT as sender
    // pairs: vector of (message0, message1) pairs
    // Returns: true on success
    bool send_ot(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                 SocketConnection& connection);
    
    // Perform multiple parallel OTs as sender
    bool send_parallel_ot(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                         SocketConnection& connection);
    
    /**
     * Receiver-side functions (for evaluator)
     */
    
    // Initialize as OT receiver
    void init_receiver();
    
    // Perform 1-out-of-2 OT as receiver
    // choices: bit vector indicating which message to receive (0 or 1)
    // Returns: received messages
    std::vector<WireLabel> receive_ot(const std::vector<bool>& choices,
                                    SocketConnection& connection);
    
    // Perform multiple parallel OTs as receiver
    std::vector<WireLabel> receive_parallel_ot(const std::vector<bool>& choices,
                                             SocketConnection& connection);
    
    /**
     * Utility functions
     */
    
    // Get the number of OTs that can be performed in parallel
    size_t get_max_parallel_ots() const { return max_parallel_ots; }
    
    // Set the number of parallel OTs
    void set_max_parallel_ots(size_t count) { max_parallel_ots = count; }
    
    // Check if handler is initialized
    bool is_initialized() const { return initialized; }
    
    // Reset handler state
    void reset();

private:
    bool initialized;
    bool is_sender;
    size_t max_parallel_ots;
    
    // libOTe objects (would be actual libOTe types in real implementation)
    void* ot_sender;      // Placeholder for libOTe sender
    void* ot_receiver;    // Placeholder for libOTe receiver
    void* prng;           // Placeholder for PRNG
    
    // Internal helper functions
    void cleanup();
    void init_libote_if_needed();
    
    // Convert between our types and libOTe types
    void* wire_label_to_block(const WireLabel& label);
    WireLabel block_to_wire_label(const void* block);
    
    // Network communication helpers
    void send_ot_setup_message(SocketConnection& connection, const std::vector<uint8_t>& data);
    std::vector<uint8_t> receive_ot_setup_message(SocketConnection& connection);
};

/**
 * Simple OT interface for basic operations
 * This provides a simplified interface that doesn't require libOTe knowledge
 */
class SimpleOT {
public:
    /**
     * Perform 1-out-of-2 OT for wire labels
     * This is a blocking operation that handles the full OT protocol
     */
    
    // Sender side: send one of two wire labels based on receiver's choice
    static void send_wire_label_ot(const WireLabel& label0, 
                                 const WireLabel& label1,
                                 SocketConnection& connection);
    
    // Receiver side: receive one wire label based on choice bit
    static WireLabel receive_wire_label_ot(bool choice, 
                                         SocketConnection& connection);
    
    /**
     * Batch OT operations
     */
    
    // Send multiple OTs in parallel
    static void send_batch_ot(const std::vector<std::pair<WireLabel, WireLabel>>& label_pairs,
                            SocketConnection& connection);
    
    // Receive multiple OTs in parallel  
    static std::vector<WireLabel> receive_batch_ot(const std::vector<bool>& choices,
                                                  SocketConnection& connection);

private:
    // Fallback implementation using simple secure channel
    // This is used when libOTe is not available or for testing
    static void fallback_send_ot(const WireLabel& label0, 
                               const WireLabel& label1,
                               SocketConnection& connection);
    
    static WireLabel fallback_receive_ot(bool choice, 
                                       SocketConnection& connection);
};

/**
 * OT Extension handler
 * Provides efficient OT extension for multiple OTs
 */
class OTExtension {
public:
    OTExtension();
    ~OTExtension();
    
    // Setup OT extension with base OTs
    void setup(size_t num_base_ots, size_t num_ots, bool is_sender);
    
    // Perform extended OTs
    void extend_sender(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                      SocketConnection& connection);
    
    std::vector<WireLabel> extend_receiver(const std::vector<bool>& choices,
                                         SocketConnection& connection);
    
    // Check if extension is set up
    bool is_setup() const { return setup_complete; }

private:
    bool setup_complete;
    size_t base_ot_count;
    size_t extended_ot_count;
    bool sender_role;
    
    // Extension state (simplified)
    std::vector<WireLabel> base_ot_keys;
    std::vector<bool> base_ot_choices;
    
    void perform_base_ots(SocketConnection& connection);
    void generate_extension_matrix();
};

// Global OT configuration
struct OTConfig {
    bool use_libote = true;           // Use libOTe if available
    bool use_extension = true;        // Use OT extension for efficiency
    size_t base_ot_count = 128;      // Number of base OTs for extension
    size_t parallel_ot_limit = 1000; // Maximum parallel OTs
    int timeout_seconds = 30;        // Timeout for OT operations
    
    static OTConfig& get_instance() {
        static OTConfig instance;
        return instance;
    }
};
