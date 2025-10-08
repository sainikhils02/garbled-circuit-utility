#pragma once

#include "common.h"
#include "socket_utils.h"

#include </mnt/c/Users/saini/Downloads/UGP/coproto/coproto/coproto.h>
#include </mnt/c/Users/saini/Downloads/UGP/libOTe/libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h>
#include </mnt/c/Users/saini/Downloads/UGP/libOTe/libOTe/TwoChooseOne/Iknp/IknpOtExtReceiver.h>
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
     * Advanced libOTe IKNP OT extension methods 
     * (for when you have proper coproto::Socket bridge)
     */
    void perform_iknp_ot_extension_send(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                                        coproto::Socket& socket);
    
    std::vector<WireLabel> perform_iknp_ot_extension_receive(const std::vector<bool>& choices,
                                                            coproto::Socket& socket);

    /**
     * Utility functions
     */
    void reset();
    bool is_initialized() const { return initialized; }
    bool is_sender_role() const { return is_sender; }
    
    // Get statistics
    size_t get_total_ots() const { return total_ots_performed; }
    
private:
    bool initialized;
    bool is_sender;
    size_t total_ots_performed;
    
    // libOTe components
    std::unique_ptr<IknpOtExtSender> ot_sender;
    std::unique_ptr<IknpOtExtReceiver> ot_receiver;
    std::unique_ptr<PRNG> prng;
    
    // Base OT storage
    std::vector<block> base_recv_msgs;  // For sender (receives in base OT)
    std::vector<std::array<block, 2>> base_send_msgs;  // For receiver (sends in base OT)
    BitVector base_choices;  // Sender's choices in base OT
    
    bool base_ots_complete;
    
    // Internal methods
    void cleanup();
    void perform_base_ots_as_sender(coproto::Socket& socket);
    void perform_base_ots_as_receiver(coproto::Socket& socket);
    
    // Conversion utilities
    block wire_label_to_block(const WireLabel& label);
    WireLabel block_to_wire_label(const block& blk);
    
    // Socket conversion (SocketConnection -> coproto::Socket)
    coproto::Socket create_coproto_socket(SocketConnection& connection);
};

/**
 * Simplified interface for basic OT operations
 * Handles setup automatically
 */
class SimpleOT {
public:
    /**
     * One-shot OT operations (handles full setup internally)
     */
    static void send_batch_ot(const std::vector<std::pair<WireLabel, WireLabel>>& label_pairs,
                             SocketConnection& connection);
    
    static std::vector<WireLabel> receive_batch_ot(const std::vector<bool>& choices,
                                                  SocketConnection& connection);

    /**
     * Individual wire label OT operations
     */
    static void send_wire_label_ot(const WireLabel& label0, const WireLabel& label1,
                                   SocketConnection& connection);
    
    static WireLabel receive_wire_label_ot(bool choice, SocketConnection& connection);

private:
    SimpleOT() = default;  // Static class
};

/**
 * Exception class for OT-related errors
 */
class OTException : public std::runtime_error {
public:
    explicit OTException(const std::string& message) 
        : std::runtime_error("OT Error: " + message) {}
};
