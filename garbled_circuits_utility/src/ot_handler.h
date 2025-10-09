#pragma once

#include "common.h"
#include "socket_utils.h"

#include </mnt/c/Users/saini/Downloads/UGP/coproto/coproto/coproto.h>
#include </mnt/c/Users/saini/Downloads/UGP/libOTe/libOTe/Base/SimplestOT.h>
#include </mnt/c/Users/saini/Downloads/UGP/libOTe/cryptoTools/cryptoTools/Common/Defines.h>
#include </mnt/c/Users/saini/Downloads/UGP/libOTe/cryptoTools/cryptoTools/Common/BitVector.h>
#include </mnt/c/Users/saini/Downloads/UGP/libOTe/cryptoTools/cryptoTools/Crypto/PRNG.h>
#include </mnt/c/Users/saini/Downloads/UGP/coproto/coproto/Socket/AsioSocket.h>

using namespace osuCrypto;

/**
 * Wrapper for libOTe functionality
 * Provides interface for oblivious transfer operations in garbled circuits
 */
class OTHandler {
public:
    /**
     * Constructor/Destructor
     */
    OTHandler();
    ~OTHandler();

    // Move semantics
    OTHandler(OTHandler&& other) noexcept;
    OTHandler& operator=(OTHandler&& other) noexcept;

    // Delete copy semantics
    OTHandler(const OTHandler&) = delete;
    OTHandler& operator=(const OTHandler&) = delete;

    /**
     * Initialization
     */
    void init_sender(SocketConnection& connection);
    void init_receiver(SocketConnection& connection);

    /**
     * Main OT operations
     */
    // Sender: send pairs of wire labels, receiver gets one based on their choices
    bool send_ot(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                 SocketConnection& connection);

    // Receiver: receive wire labels based on choice bits
    std::vector<WireLabel> receive_ot(const std::vector<bool>& choices,
                                     SocketConnection& connection);

    /**
     * Utility functions
     */
    void reset();
    bool is_initialized() const { return initialized; }
    bool is_sender_role() const { return is_sender; }
    
    size_t get_total_ots() const { return total_ots_performed; }
    
private:
    bool initialized;
    bool is_sender;
    size_t total_ots_performed;
    std::unique_ptr<PRNG> prng;

    // Internal methods / helpers
    void cleanup();
    block wire_label_to_block(const WireLabel& label);
    WireLabel block_to_wire_label(const block& blk);

    // Endpoint resolution for Asio-based SimplestOT (env GC_OT_ENDPOINT or default 127.0.0.1:9100)
    std::string resolve_endpoint() const;
    void simplest_ot_send(size_t n, std::vector<std::array<block,2>>& outPairs, const std::string& endpoint);
    void simplest_ot_receive(size_t n, const std::vector<bool>& choices, std::vector<block>& outMsgs, const std::string& endpoint);
    void kdf_mask_labels(const std::vector<std::pair<WireLabel,WireLabel>>& in,
                         const std::vector<std::array<block,2>>& otBlocks,
                         std::vector<std::array<WireLabel,2>>& masked) const;
    void derive_chosen_labels(const std::vector<std::array<WireLabel,2>>& masked,
                              const std::vector<block>& recvBlocks,
                              const std::vector<bool>& choices,
                              std::vector<WireLabel>& out) const;
};

/**
 * Simplified interface for basic OT operations
 * Handles setup automatically
 */

/**
 * Exception class for OT-related errors
 */
class OTException : public std::runtime_error {
public:
    explicit OTException(const std::string& message) 
        : std::runtime_error("OT Error: " + message) {}
};
