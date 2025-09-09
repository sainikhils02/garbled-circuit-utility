# Create ot_handler.h
ot_handler_h = """#pragma once

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
"""

with open("garbled_circuits_utility/src/ot_handler.h", "w") as f:
    f.write(ot_handler_h)

# Create ot_handler.cpp
ot_handler_cpp = """#include "ot_handler.h"
#include "crypto_utils.h"
#include <thread>
#include <chrono>

// Note: In a real implementation, this would include libOTe headers
// #include <libOTe/Tools/Tools.h>
// #include <libOTe/Base/naor-pinkas.h>
// #include <libOTe/TwoChooseOne/SilentOtExtSender.h>
// #include <libOTe/TwoChooseOne/SilentOtExtReceiver.h>

// OTHandler implementation
OTHandler::OTHandler() 
    : initialized(false), is_sender(false), max_parallel_ots(1000),
      ot_sender(nullptr), ot_receiver(nullptr), prng(nullptr) {
}

OTHandler::~OTHandler() {
    cleanup();
}

OTHandler::OTHandler(OTHandler&& other) noexcept
    : initialized(other.initialized), is_sender(other.is_sender), 
      max_parallel_ots(other.max_parallel_ots),
      ot_sender(other.ot_sender), ot_receiver(other.ot_receiver), prng(other.prng) {
    other.initialized = false;
    other.ot_sender = nullptr;
    other.ot_receiver = nullptr;
    other.prng = nullptr;
}

OTHandler& OTHandler::operator=(OTHandler&& other) noexcept {
    if (this != &other) {
        cleanup();
        initialized = other.initialized;
        is_sender = other.is_sender;
        max_parallel_ots = other.max_parallel_ots;
        ot_sender = other.ot_sender;
        ot_receiver = other.ot_receiver;
        prng = other.prng;
        
        other.initialized = false;
        other.ot_sender = nullptr;
        other.ot_receiver = nullptr;
        other.prng = nullptr;
    }
    return *this;
}

void OTHandler::init_sender() {
    if (initialized) {
        throw std::runtime_error("OTHandler already initialized");
    }
    
    LOG_INFO("Initializing OT sender");
    is_sender = true;
    
    // In a real implementation, this would initialize libOTe sender
    // For now, we'll use a placeholder
    init_libote_if_needed();
    
    initialized = true;
}

void OTHandler::init_receiver() {
    if (initialized) {
        throw std::runtime_error("OTHandler already initialized");
    }
    
    LOG_INFO("Initializing OT receiver");
    is_sender = false;
    
    // In a real implementation, this would initialize libOTe receiver
    init_libote_if_needed();
    
    initialized = true;
}

bool OTHandler::send_ot(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                       SocketConnection& connection) {
    if (!initialized || !is_sender) {
        throw std::runtime_error("OT sender not properly initialized");
    }
    
    LOG_INFO("Performing " << pairs.size() << " OTs as sender");
    
    try {
        // In a real implementation, this would use libOTe
        // For now, we'll use the simple fallback
        for (const auto& pair : pairs) {
            SimpleOT::send_wire_label_ot(pair.first, pair.second, connection);
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("OT send failed: " << e.what());
        return false;
    }
}

bool OTHandler::send_parallel_ot(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                                SocketConnection& connection) {
    if (!initialized || !is_sender) {
        throw std::runtime_error("OT sender not properly initialized");
    }
    
    LOG_INFO("Performing " << pairs.size() << " parallel OTs as sender");
    
    try {
        // For this basic implementation, we'll batch the operations
        SimpleOT::send_batch_ot(pairs, connection);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Parallel OT send failed: " << e.what());
        return false;
    }
}

std::vector<WireLabel> OTHandler::receive_ot(const std::vector<bool>& choices,
                                           SocketConnection& connection) {
    if (!initialized || is_sender) {
        throw std::runtime_error("OT receiver not properly initialized");
    }
    
    LOG_INFO("Performing " << choices.size() << " OTs as receiver");
    
    try {
        std::vector<WireLabel> results;
        results.reserve(choices.size());
        
        // In a real implementation, this would use libOTe
        // For now, we'll use the simple fallback
        for (bool choice : choices) {
            results.push_back(SimpleOT::receive_wire_label_ot(choice, connection));
        }
        
        return results;
    } catch (const std::exception& e) {
        LOG_ERROR("OT receive failed: " << e.what());
        throw;
    }
}

std::vector<WireLabel> OTHandler::receive_parallel_ot(const std::vector<bool>& choices,
                                                    SocketConnection& connection) {
    if (!initialized || is_sender) {
        throw std::runtime_error("OT receiver not properly initialized");
    }
    
    LOG_INFO("Performing " << choices.size() << " parallel OTs as receiver");
    
    try {
        return SimpleOT::receive_batch_ot(choices, connection);
    } catch (const std::exception& e) {
        LOG_ERROR("Parallel OT receive failed: " << e.what());
        throw;
    }
}

void OTHandler::reset() {
    cleanup();
    initialized = false;
    is_sender = false;
}

void OTHandler::cleanup() {
    // In a real implementation, this would clean up libOTe resources
    if (ot_sender) {
        // delete static_cast<SomeLibOTeType*>(ot_sender);
        ot_sender = nullptr;
    }
    if (ot_receiver) {
        // delete static_cast<SomeLibOTeType*>(ot_receiver);
        ot_receiver = nullptr;
    }
    if (prng) {
        // delete static_cast<PRNG*>(prng);
        prng = nullptr;
    }
}

void OTHandler::init_libote_if_needed() {
    // In a real implementation, this would initialize libOTe components
    // For now, we'll just set up placeholders
    LOG_INFO("Initializing libOTe components (placeholder)");
    
    // This would create actual libOTe objects:
    // prng = new osuCrypto::PRNG(osuCrypto::sysRandomSeed());
    // if (is_sender) {
    //     ot_sender = new osuCrypto::SilentOtExtSender();
    // } else {
    //     ot_receiver = new osuCrypto::SilentOtExtReceiver();
    // }
}

void* OTHandler::wire_label_to_block(const WireLabel& label) {
    // In a real implementation, this would convert WireLabel to libOTe::block
    return nullptr; // Placeholder
}

WireLabel OTHandler::block_to_wire_label(const void* block) {
    // In a real implementation, this would convert libOTe::block to WireLabel
    return CryptoUtils::generate_random_label(); // Placeholder
}

void OTHandler::send_ot_setup_message(SocketConnection& connection, const std::vector<uint8_t>& data) {
    Message msg(MessageType::OT_REQUEST, data);
    SocketUtils::send_message(connection.get_socket(), msg);
}

std::vector<uint8_t> OTHandler::receive_ot_setup_message(SocketConnection& connection) {
    Message msg = SocketUtils::receive_message(connection.get_socket());
    if (msg.type != MessageType::OT_REQUEST && msg.type != MessageType::OT_RESPONSE) {
        throw NetworkException("Expected OT setup message");
    }
    return msg.data;
}

// SimpleOT implementation
void SimpleOT::send_wire_label_ot(const WireLabel& label0, const WireLabel& label1,
                                SocketConnection& connection) {
    // This is a simplified OT implementation for demonstration
    // In practice, you would use a proper OT protocol
    
    LOG_INFO("Sending wire label OT (fallback implementation)");
    
    // For this basic implementation, we'll use the fallback
    fallback_send_ot(label0, label1, connection);
}

WireLabel SimpleOT::receive_wire_label_ot(bool choice, SocketConnection& connection) {
    LOG_INFO("Receiving wire label OT with choice=" << choice << " (fallback implementation)");
    
    return fallback_receive_ot(choice, connection);
}

void SimpleOT::send_batch_ot(const std::vector<std::pair<WireLabel, WireLabel>>& label_pairs,
                           SocketConnection& connection) {
    LOG_INFO("Sending batch OT for " << label_pairs.size() << " pairs");
    
    // Send count first
    uint32_t count = label_pairs.size();
    std::vector<uint8_t> count_data = {
        (uint8_t)((count >> 24) & 0xFF),
        (uint8_t)((count >> 16) & 0xFF),
        (uint8_t)((count >> 8) & 0xFF),
        (uint8_t)(count & 0xFF)
    };
    
    Message count_msg(MessageType::OT_REQUEST, count_data);
    SocketUtils::send_message(connection.get_socket(), count_msg);
    
    // Send each pair
    for (const auto& pair : label_pairs) {
        send_wire_label_ot(pair.first, pair.second, connection);
    }
}

std::vector<WireLabel> SimpleOT::receive_batch_ot(const std::vector<bool>& choices,
                                                SocketConnection& connection) {
    LOG_INFO("Receiving batch OT for " << choices.size() << " choices");
    
    // Receive count
    Message count_msg = SocketUtils::receive_message(connection.get_socket());
    if (count_msg.type != MessageType::OT_REQUEST || count_msg.data.size() != 4) {
        throw NetworkException("Invalid batch OT count message");
    }
    
    uint32_t count = (count_msg.data[0] << 24) | (count_msg.data[1] << 16) |
                    (count_msg.data[2] << 8) | count_msg.data[3];
    
    if (count != choices.size()) {
        throw NetworkException("Batch OT count mismatch");
    }
    
    std::vector<WireLabel> results;
    results.reserve(count);
    
    // Receive each label
    for (bool choice : choices) {
        results.push_back(receive_wire_label_ot(choice, connection));
    }
    
    return results;
}

void SimpleOT::fallback_send_ot(const WireLabel& label0, const WireLabel& label1,
                              SocketConnection& connection) {
    // This is a VERY simplified OT for demonstration purposes only
    // It's NOT cryptographically secure and should not be used in practice
    
    LOG_WARNING("Using insecure fallback OT implementation!");
    
    // Generate random choice for demonstration
    auto random_bit = CryptoUtils::generate_random_label()[0] & 1;
    
    // Send both labels encrypted (this is not a real OT!)
    std::vector<uint8_t> data;
    data.insert(data.end(), label0.begin(), label0.end());
    data.insert(data.end(), label1.begin(), label1.end());
    
    Message msg(MessageType::OT_RESPONSE, data);
    SocketUtils::send_message(connection.get_socket(), msg);
}

WireLabel SimpleOT::fallback_receive_ot(bool choice, SocketConnection& connection) {
    LOG_WARNING("Using insecure fallback OT implementation!");
    
    // Receive both labels (this is not a real OT!)
    Message msg = SocketUtils::receive_message(connection.get_socket());
    if (msg.type != MessageType::OT_RESPONSE || msg.data.size() != 2 * WIRE_LABEL_SIZE) {
        throw NetworkException("Invalid fallback OT response");
    }
    
    WireLabel result;
    size_t offset = choice ? WIRE_LABEL_SIZE : 0;
    std::copy(msg.data.begin() + offset, 
             msg.data.begin() + offset + WIRE_LABEL_SIZE, 
             result.begin());
    
    return result;
}

// OTExtension implementation
OTExtension::OTExtension() 
    : setup_complete(false), base_ot_count(0), extended_ot_count(0), sender_role(false) {
}

OTExtension::~OTExtension() {
    // Cleanup extension state
}

void OTExtension::setup(size_t num_base_ots, size_t num_ots, bool is_sender) {
    LOG_INFO("Setting up OT extension: " << num_base_ots << " base OTs, " 
             << num_ots << " extended OTs, sender=" << is_sender);
    
    base_ot_count = num_base_ots;
    extended_ot_count = num_ots;
    sender_role = is_sender;
    
    base_ot_keys.clear();
    base_ot_choices.clear();
    
    // Initialize base OT state
    base_ot_keys.reserve(base_ot_count);
    base_ot_choices.reserve(base_ot_count);
    
    for (size_t i = 0; i < base_ot_count; ++i) {
        base_ot_keys.push_back(CryptoUtils::generate_random_label());
        base_ot_choices.push_back((CryptoUtils::generate_random_label()[0] & 1) == 1);
    }
    
    setup_complete = true;
}

void OTExtension::extend_sender(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                              SocketConnection& connection) {
    if (!setup_complete) {
        throw std::runtime_error("OT extension not set up");
    }
    
    LOG_INFO("Performing OT extension as sender for " << pairs.size() << " OTs");
    
    // This is a placeholder for actual OT extension
    // In practice, this would implement the full OT extension protocol
    SimpleOT::send_batch_ot(pairs, connection);
}

std::vector<WireLabel> OTExtension::extend_receiver(const std::vector<bool>& choices,
                                                  SocketConnection& connection) {
    if (!setup_complete) {
        throw std::runtime_error("OT extension not set up");
    }
    
    LOG_INFO("Performing OT extension as receiver for " << choices.size() << " OTs");
    
    // This is a placeholder for actual OT extension
    // In practice, this would implement the full OT extension protocol
    return SimpleOT::receive_batch_ot(choices, connection);
}

void OTExtension::perform_base_ots(SocketConnection& connection) {
    // Perform the base OTs needed for extension
    // This is simplified for demonstration
}

void OTExtension::generate_extension_matrix() {
    // Generate the random matrix for OT extension
    // This is simplified for demonstration
}
"""

with open("garbled_circuits_utility/src/ot_handler.cpp", "w") as f:
    f.write(ot_handler_cpp)

print("ot_handler.h and ot_handler.cpp created successfully!")
print(f"Header length: {len(ot_handler_h)} characters")
print(f"Implementation length: {len(ot_handler_cpp)} characters")