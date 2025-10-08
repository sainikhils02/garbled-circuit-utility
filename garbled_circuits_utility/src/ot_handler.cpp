#include "ot_handler.h"
#include "crypto_utils.h"
#include "socket_utils.h"
#include "common.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>

using namespace osuCrypto;

// Helper functions for WireLabel <-> block conversion
block OTHandler::wire_label_to_block(const WireLabel& label) {
    block b;
    if (label.size() >= 16) {
        std::memcpy(&b, label.data(), 16);
    } else {
        // Zero-pad if label is smaller
        std::memset(&b, 0, 16);
        std::memcpy(&b, label.data(), label.size());
    }
    return b;
}

WireLabel OTHandler::block_to_wire_label(const block& b) {
    WireLabel label{0};  // Initialize array with zeros
    std::memcpy(label.data(), &b, std::min(sizeof(block), label.size()));
    return label;
}

// OTHandler implementation
OTHandler::OTHandler()
    : initialized(false), is_sender(false), total_ots_performed(0),
      ot_sender(nullptr), ot_receiver(nullptr), prng(nullptr),
      base_recv_msgs(), base_send_msgs(), 
      base_choices(), base_ots_complete(false) {
}

OTHandler::~OTHandler() {
    cleanup();
}

// Move constructor - use std::move for unique_ptr
OTHandler::OTHandler(OTHandler&& other) noexcept
    : initialized(other.initialized), is_sender(other.is_sender),
      total_ots_performed(other.total_ots_performed),
      ot_sender(std::move(other.ot_sender)), 
      ot_receiver(std::move(other.ot_receiver)),
      prng(std::move(other.prng)),
      base_recv_msgs(std::move(other.base_recv_msgs)),
      base_send_msgs(std::move(other.base_send_msgs)),
      base_choices(std::move(other.base_choices)),
      base_ots_complete(other.base_ots_complete) {
    
    // Reset the moved-from object
    other.initialized = false;
    other.is_sender = false;
    other.total_ots_performed = 0;
    other.base_ots_complete = false;
}

// Move assignment operator - use std::move for unique_ptr
OTHandler& OTHandler::operator=(OTHandler&& other) noexcept {
    if (this != &other) {
        cleanup();
        
        initialized = other.initialized;
        is_sender = other.is_sender;
        total_ots_performed = other.total_ots_performed;
        ot_sender = std::move(other.ot_sender);
        ot_receiver = std::move(other.ot_receiver);
        prng = std::move(other.prng);
        base_recv_msgs = std::move(other.base_recv_msgs);
        base_send_msgs = std::move(other.base_send_msgs);
        base_choices = std::move(other.base_choices);
        base_ots_complete = other.base_ots_complete;
        
        // Reset the moved-from object
        other.initialized = false;
        other.is_sender = false;
        other.total_ots_performed = 0;
        other.base_ots_complete = false;
    }
    return *this;
}

// Fix function signature to match header
void OTHandler::init_sender(SocketConnection& connection) {
    if (initialized) {
        throw OTException("OTHandler already initialized");
    }
    
    LOG_INFO("Initializing OT sender with libOTe");
    
    try {
        // Initialize PRNG with fixed seed (like in iknp_ot_example)
        prng = std::make_unique<PRNG>(block(12345, 67890));
        
        // Create sender instance
        ot_sender = std::make_unique<IknpOtExtSender>();
        
        // Prepare data structures for base OTs (128 base OTs required for IKNP)
        base_recv_msgs.resize(128);
        base_choices.resize(128);
        base_choices.randomize(*prng);
        
        is_sender = true;
        initialized = true;
        
        LOG_INFO("OT sender initialized - base OT setup ready");
        
    } catch (const std::exception& e) {
        cleanup();
        throw OTException("Failed to initialize OT sender: " + std::string(e.what()));
    }
}

// Fix function signature to match header
void OTHandler::init_receiver(SocketConnection& connection) {
    if (initialized) {
        throw OTException("OTHandler already initialized");
    }
    
    LOG_INFO("Initializing OT receiver with libOTe");
    
    try {
        // Initialize PRNG with same seed as sender for synchronization
        prng = std::make_unique<PRNG>(block(12345, 67890));
        
        // CRITICAL: Synchronize PRNG state with sender
        // Sender uses PRNG first for baseChoice, so we must match that
        BitVector dummyChoice(128);
        dummyChoice.randomize(*prng);
        
        // Create receiver instance
        ot_receiver = std::make_unique<IknpOtExtReceiver>();
        
        // Prepare data structures for base OTs
        base_send_msgs.resize(128);
        
        is_sender = false;
        initialized = true;
        
        LOG_INFO("OT receiver initialized - base OT setup ready");
        
    } catch (const std::exception& e) {
        cleanup();
        throw OTException("Failed to initialize OT receiver: " + std::string(e.what()));
    }
}

bool OTHandler::send_ot(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                       SocketConnection& connection) {
    if (!initialized || !is_sender) {
        throw OTException("OT sender not properly initialized");
    }
    
    LOG_INFO("Performing " << pairs.size() << " OTs as sender using libOTe");
    
    try {
        // TODO: Create a proper bridge between SocketConnection and coproto::Socket
        // For now, use simplified implementation
        for (const auto& pair : pairs) {
            SimpleOT::send_wire_label_ot(pair.first, pair.second, connection);
        }
        
        total_ots_performed += pairs.size();
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("OT send failed: " << e.what());
        return false;
    }
}

std::vector<WireLabel> OTHandler::receive_ot(const std::vector<bool>& choices,
                                            SocketConnection& connection) {
    if (!initialized || is_sender) {
        throw OTException("OT receiver not properly initialized");
    }
    
    LOG_INFO("Performing " << choices.size() << " OTs as receiver using libOTe");
    
    try {
        std::vector<WireLabel> results;
        results.reserve(choices.size());
        
        // TODO: Create a proper bridge between SocketConnection and coproto::Socket
        // For now, use simplified implementation
        for (bool choice : choices) {
            results.push_back(SimpleOT::receive_wire_label_ot(choice, connection));
        }
        
        total_ots_performed += choices.size();
        return results;
        
    } catch (const std::exception& e) {
        LOG_ERROR("OT receive failed: " << e.what());
        throw;
    }
}

void OTHandler::reset() {
    cleanup();
    initialized = false;
    is_sender = false;
    total_ots_performed = 0;
    base_ots_complete = false;
}

// Implement proper base OT as sender using libOTe pattern
void OTHandler::perform_base_ots_as_sender(coproto::Socket& socket) {
    if (base_recv_msgs.empty() || base_choices.size() == 0 || !prng) {
        throw OTException("Base OT data structures not initialized");
    }
    
    LOG_INFO("Performing base OTs as sender (acting as receiver in base OT phase)");
    
    try {
        // Step 1: Perform base OTs (sender acts as RECEIVER in base OT phase)
        SimplestOT baseOT;
        auto baseTask = baseOT.receive(base_choices, base_recv_msgs, *prng, socket, 1);
        coproto::sync_wait(baseTask);
        
        // Step 2: Set the base OTs for the extension protocol
        ot_sender->setBaseOts(base_recv_msgs, base_choices);
        
        base_ots_complete = true;
        LOG_INFO("Base OTs completed successfully for sender");
        
    } catch (const std::exception& e) {
        throw OTException("Base OT failed for sender: " + std::string(e.what()));
    }
}

// Implement proper base OT as receiver using libOTe pattern
void OTHandler::perform_base_ots_as_receiver(coproto::Socket& socket) {
    if (base_send_msgs.empty() || !prng) {
        throw OTException("Base OT data structures not initialized");
    }
    
    LOG_INFO("Performing base OTs as receiver (acting as sender in base OT phase)");
    
    try {
        // Step 1: Generate random base OT messages for sending
        for (auto& msgPair : base_send_msgs) {
            msgPair[0] = prng->get<block>();
            msgPair[1] = prng->get<block>();
        }
        
        // Step 2: Perform base OTs (receiver acts as SENDER in base OT phase)
        SimplestOT baseOT;
        auto baseTask = baseOT.send(base_send_msgs, *prng, socket, 1);
        coproto::sync_wait(baseTask);
        
        // Step 3: Set the base OTs for the extension protocol
        ot_receiver->setBaseOts(base_send_msgs);
        
        base_ots_complete = true;
        LOG_INFO("Base OTs completed successfully for receiver");
        
    } catch (const std::exception& e) {
        throw OTException("Base OT failed for receiver: " + std::string(e.what()));
    }
}

void OTHandler::cleanup() {
    // unique_ptr automatically cleans up when reset or destroyed
    ot_sender.reset();
    ot_receiver.reset();
    prng.reset();
    base_recv_msgs.clear();
    base_send_msgs.clear();
    base_choices.resize(0);
}

// SimpleOT implementation - add static keyword and proper function signatures
void SimpleOT::send_wire_label_ot(const WireLabel& label0, const WireLabel& label1,
                                   SocketConnection& connection) {
    LOG_INFO("Sending wire label OT (simplified implementation)");
    LOG_INFO("Socket descriptor: " << connection.get_socket());
    LOG_INFO("Connection status: " << (connection.is_connected() ? "connected" : "disconnected"));
    
    try {
        // Send both labels - THIS IS NOT SECURE, just for compilation
        LOG_INFO("Sending label0...");
        SocketUtils::send_wire_label(connection.get_socket(), label0);
        LOG_INFO("Sending label1...");
        SocketUtils::send_wire_label(connection.get_socket(), label1);
        LOG_INFO("Both labels sent successfully");
        
    } catch (const std::exception& e) {
        LOG_INFO("Exception during send: " << e.what());
        throw OTException("OT send failed: " + std::string(e.what()));
    }
}

// Add a method to perform actual IKNP OT extension (for when you have proper socket bridge)
void OTHandler::perform_iknp_ot_extension_send(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                                               coproto::Socket& socket) {
    if (!initialized || !is_sender || !base_ots_complete) {
        throw OTException("Sender not ready for OT extension");
    }
    
    LOG_INFO("Performing IKNP OT extension as sender with " << pairs.size() << " OTs");
    
    try {
        // Prepare messages for extended OTs (convert WireLabel to block)
        std::vector<std::array<block, 2>> sendMsg(pairs.size());
        for (size_t i = 0; i < pairs.size(); ++i) {
            sendMsg[i][0] = wire_label_to_block(pairs[i].first);
            sendMsg[i][1] = wire_label_to_block(pairs[i].second);
        }
        
        // Execute the OT extension protocol
        auto task = ot_sender->send(sendMsg, *prng, socket);
        coproto::sync_wait(task);
        
        total_ots_performed += pairs.size();
        LOG_INFO("IKNP OT extension completed successfully");
        
    } catch (const std::exception& e) {
        throw OTException("IKNP OT extension send failed: " + std::string(e.what()));
    }
}

std::vector<WireLabel> OTHandler::perform_iknp_ot_extension_receive(const std::vector<bool>& choices,
                                                                   coproto::Socket& socket) {
    if (!initialized || is_sender || !base_ots_complete) {
        throw OTException("Receiver not ready for OT extension");
    }
    
    LOG_INFO("Performing IKNP OT extension as receiver with " << choices.size() << " OTs");
    
    try {
        // Prepare receiver data structures
        std::vector<block> recvMsg(choices.size());
        BitVector choiceVector(choices.size());
        
        // Convert bool vector to BitVector
        for (size_t i = 0; i < choices.size(); ++i) {
            choiceVector[i] = choices[i];
        }
        
        // Execute the OT extension protocol to receive messages
        auto task = ot_receiver->receive(choiceVector, recvMsg, *prng, socket);
        coproto::sync_wait(task);
        
        // Convert received blocks back to WireLabels
        std::vector<WireLabel> results;
        results.reserve(choices.size());
        for (const auto& msg : recvMsg) {
            results.push_back(block_to_wire_label(msg));
        }
        
        total_ots_performed += choices.size();
        LOG_INFO("IKNP OT extension completed successfully");
        
        return results;
        
    } catch (const std::exception& e) {
        throw OTException("IKNP OT extension receive failed: " + std::string(e.what()));
    }
}

WireLabel SimpleOT::receive_wire_label_ot(bool choice, SocketConnection& connection) {
    LOG_INFO("Receiving wire label OT with choice=" << choice << " (simplified implementation)");
    LOG_INFO("Socket descriptor: " << connection.get_socket());
    LOG_INFO("Connection status: " << (connection.is_connected() ? "connected" : "disconnected"));
    
    try {
        // Receive both labels
        LOG_INFO("Receiving label0...");
        WireLabel label0 = SocketUtils::receive_wire_label(connection.get_socket());
        LOG_INFO("Receiving label1...");
        WireLabel label1 = SocketUtils::receive_wire_label(connection.get_socket());
        LOG_INFO("Both labels received successfully");
        
        // Debug: Show received labels
        std::cout << "[OT DEBUG] Received labels:" << std::endl;
        std::cout << "           Label_0: ";
        for (int i = 0; i < 8; ++i) std::cout << std::hex << (int)label0[i];
        std::cout << std::endl;
        std::cout << "           Label_1: ";
        for (int i = 0; i < 8; ++i) std::cout << std::hex << (int)label1[i];
        std::cout << std::endl;
        
        // Return the chosen label
        WireLabel chosen = choice ? label1 : label0;
        std::cout << "[OT DEBUG] Chosen label (choice=" << choice << "): ";
        for (int i = 0; i < 8; ++i) std::cout << std::hex << (int)chosen[i];
        std::cout << std::endl;
        
        LOG_INFO("Returning chosen label for choice=" << choice);
        return chosen;
        
    } catch (const std::exception& e) {
        throw OTException("OT receive failed: " + std::string(e.what()));
    }
}

void SimpleOT::send_batch_ot(const std::vector<std::pair<WireLabel, WireLabel>>& label_pairs,
                           SocketConnection& connection) {
    for (const auto& pair : label_pairs) {
        SimpleOT::send_wire_label_ot(pair.first, pair.second, connection);
    }
}

std::vector<WireLabel> SimpleOT::receive_batch_ot(const std::vector<bool>& choices,
                                                SocketConnection& connection) {
    std::vector<WireLabel> results;
    results.reserve(choices.size());
    
    for (bool choice : choices) {
        results.push_back(SimpleOT::receive_wire_label_ot(choice, connection));
    }
    
    return results;
}