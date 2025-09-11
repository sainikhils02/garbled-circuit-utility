#include "ot_handler.h"
#include "crypto_utils.h"
#include <coproto/Socket/AsioSocket.h>
#include <coproto/coproto.h>

// OTHandler implementation

OTHandler::OTHandler()
    : initialized(false), is_sender(false), total_ots_performed(0),
      base_ots_complete(false) {
    
    // Initialize PRNG with a random seed
    std::random_device rd;
    auto seed = block(rd(), rd());
    prng = std::make_unique<PRNG>(seed);
}

OTHandler::~OTHandler() {
    cleanup();
}

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
    
    other.initialized = false;
    other.is_sender = false;
    other.total_ots_performed = 0;
    other.base_ots_complete = false;
}

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
        
        other.initialized = false;
        other.is_sender = false;
        other.total_ots_performed = 0;
        other.base_ots_complete = false;
    }
    return *this;
}

void OTHandler::init_sender(SocketConnection& connection) {
    if (initialized) {
        throw OTException("OTHandler already initialized");
    }
    
    LOG_INFO("Initializing OT sender with libOTe");
    
    try {
        is_sender = true;
        ot_sender = std::make_unique<IknpOtExtSender>();
        
        // Create coproto socket from SocketConnection
        auto socket = create_coproto_socket(connection);
        
        // Perform base OTs (sender receives in base OT phase)
        perform_base_ots_as_sender(socket);
        
        // Set base OTs for IKNP extension
        ot_sender->setBaseOts(base_recv_msgs, base_choices);
        
        initialized = true;
        LOG_INFO("OT sender initialized successfully");
        
    } catch (const std::exception& e) {
        throw OTException("Failed to initialize sender: " + std::string(e.what()));
    }
}

void OTHandler::init_receiver(SocketConnection& connection) {
    if (initialized) {
        throw OTException("OTHandler already initialized");
    }
    
    LOG_INFO("Initializing OT receiver with libOTe");
    
    try {
        is_sender = false;
        ot_receiver = std::make_unique<IknpOtExtReceiver>();
        
        // Create coproto socket from SocketConnection  
        auto socket = create_coproto_socket(connection);
        
        // Perform base OTs (receiver sends in base OT phase)
        perform_base_ots_as_receiver(socket);
        
        // Set base OTs for IKNP extension
        ot_receiver->setBaseOts(base_send_msgs);
        
        initialized = true;
        LOG_INFO("OT receiver initialized successfully");
        
    } catch (const std::exception& e) {
        throw OTException("Failed to initialize receiver: " + std::string(e.what()));
    }
}

bool OTHandler::send_ot(const std::vector<std::pair<WireLabel, WireLabel>>& pairs,
                       SocketConnection& connection) {
    if (!initialized || !is_sender) {
        throw OTException("OT sender not properly initialized");
    }
    
    LOG_INFO("Performing " << pairs.size() << " OTs as sender using IKNP extension");
    
    try {
        // Convert WireLabel pairs to block arrays
        std::vector<std::array<block, 2>> send_msgs(pairs.size());
        for (size_t i = 0; i < pairs.size(); ++i) {
            send_msgs[i][0] = wire_label_to_block(pairs[i].first);
            send_msgs[i][1] = wire_label_to_block(pairs[i].second);
        }
        
        // Create coproto socket
        auto socket = create_coproto_socket(connection);
        
        // Perform IKNP OT extension
        auto task = ot_sender->send(send_msgs, *prng, socket, 1);
        coproto::sync_wait(task);
        
        total_ots_performed += pairs.size();
        LOG_INFO("Successfully completed " << pairs.size() << " OTs as sender");
        
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
    
    LOG_INFO("Performing " << choices.size() << " OTs as receiver using IKNP extension");
    
    try {
        // Convert choices to BitVector
        BitVector choice_bits(choices.size());
        for (size_t i = 0; i < choices.size(); ++i) {
            choice_bits[i] = choices[i];
        }
        
        // Prepare receive buffer
        std::vector<block> recv_msgs(choices.size());
        
        // Create coproto socket
        auto socket = create_coproto_socket(connection);
        
        // Perform IKNP OT extension
        auto task = ot_receiver->receive(choice_bits, recv_msgs, *prng, socket);
        coproto::sync_wait(task);
        
        // Convert blocks back to WireLabels
        std::vector<WireLabel> results;
        results.reserve(recv_msgs.size());
        for (const auto& block_msg : recv_msgs) {
            results.push_back(block_to_wire_label(block_msg));
        }
        
        total_ots_performed += choices.size();
        LOG_INFO("Successfully completed " << choices.size() << " OTs as receiver");
        
        return results;
        
    } catch (const std::exception& e) {
        LOG_ERROR("OT receive failed: " << e.what());
        throw OTException("OT receive failed: " + std::string(e.what()));
    }
}

void OTHandler::reset() {
    cleanup();
    initialized = false;
    is_sender = false;
    base_ots_complete = false;
    total_ots_performed = 0;
}

void OTHandler::cleanup() {
    ot_sender.reset();
    ot_receiver.reset();
    base_recv_msgs.clear();
    base_send_msgs.clear();
    base_choices.resize(0);
}

void OTHandler::perform_base_ots_as_sender(coproto::Socket& socket) {
    LOG_INFO("Performing base OTs as sender (receiver in base phase)");
    
    const size_t BASE_OT_COUNT = 128;
    
    base_recv_msgs.resize(BASE_OT_COUNT);
    base_choices.resize(BASE_OT_COUNT);
    base_choices.randomize(*prng);
    
    SimplestOT base_ot;
    auto task = base_ot.receive(base_choices, base_recv_msgs, *prng, socket);
    coproto::sync_wait(task);
    
    base_ots_complete = true;
    LOG_INFO("Base OTs completed as sender");
}

void OTHandler::perform_base_ots_as_receiver(coproto::Socket& socket) {
    LOG_INFO("Performing base OTs as receiver (sender in base phase)");
    
    const size_t BASE_OT_COUNT = 128;
    
    base_send_msgs.resize(BASE_OT_COUNT);
    for (auto& msg_pair : base_send_msgs) {
        msg_pair[0] = prng->get<block>();
        msg_pair[1] = prng->get<block>();
    }
    
    SimplestOT base_ot;
    auto task = base_ot.send(base_send_msgs, *prng, socket, 1);
    coproto::sync_wait(task);
    
    base_ots_complete = true;
    LOG_INFO("Base OTs completed as receiver");
}

block OTHandler::wire_label_to_block(const WireLabel& label) {
    block result;
    // Zero out the block first
    std::memset(&result, 0, sizeof(block));
    // Copy wire label data (up to block size)
    std::memcpy(&result, label.data(), std::min(sizeof(block), label.size()));
    return result;
}

WireLabel OTHandler::block_to_wire_label(const block& blk) {
    WireLabel result;
    // Zero out the wire label first
    result.fill(0);
    // Copy block data (up to wire label size)
    std::memcpy(result.data(), &blk, std::min(sizeof(block), result.size()));
    return result;
}

coproto::Socket OTHandler::create_coproto_socket(SocketConnection& connection) {
    // This is a simplified conversion - in practice you'd need to properly
    // wrap the SocketConnection to work with coproto::Socket interface
    // For now, we'll create a new socket connection based on the existing one
    
    // Extract connection details from SocketConnection
    // This assumes SocketConnection has methods to get host/port
    // You may need to modify this based on your actual SocketConnection implementation
    
    throw std::runtime_error("Socket conversion not yet implemented - need to integrate SocketConnection with coproto::Socket");
}

// SimpleOT implementation

void SimpleOT::send_batch_ot(const std::vector<std::pair<WireLabel, WireLabel>>& label_pairs,
                            SocketConnection& connection) {
    LOG_INFO("Performing batch OT send with " << label_pairs.size() << " OTs");
    
    try {
        OTHandler handler;
        handler.init_sender(connection);
        
        bool success = handler.send_ot(label_pairs, connection);
        if (!success) {
            throw OTException("Batch OT send failed");
        }
        
    } catch (const std::exception& e) {
        throw OTException("SimpleOT send failed: " + std::string(e.what()));
    }
}

std::vector<WireLabel> SimpleOT::receive_batch_ot(const std::vector<bool>& choices,
                                                 SocketConnection& connection) {
    LOG_INFO("Performing batch OT receive with " << choices.size() << " OTs");
    
    try {
        OTHandler handler;
        handler.init_receiver(connection);
        
        return handler.receive_ot(choices, connection);
        
    } catch (const std::exception& e) {
        throw OTException("SimpleOT receive failed: " + std::string(e.what()));
    }
}
