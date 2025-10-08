#include "socket_utils.h"
#include <cstring>
#include <stdexcept>
#include <errno.h>
#include <sstream>

// SocketUtils implementation
int SocketUtils::create_server_socket(int port) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        throw_network_error("socket creation");
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_socket);
        throw_network_error("setsockopt");
    }
    
    // Bind to address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_socket);
        throw_network_error("bind");
    }
    
    // Listen for connections
    if (listen(server_socket, 1) < 0) {
        close(server_socket);
        throw_network_error("listen");
    }
    
    LOG_INFO("Server socket created and listening on port " << port);
    return server_socket;
}

int SocketUtils::accept_client(int server_socket) {
    struct sockaddr_in client_address;
    socklen_t client_addr_len = sizeof(client_address);
    
    int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_addr_len);
    if (client_socket < 0) {
        throw_network_error("accept");
    }
    
    LOG_INFO("Client connected from " << inet_ntoa(client_address.sin_addr) 
             << ":" << ntohs(client_address.sin_port));
    return client_socket;
}

int SocketUtils::connect_to_server(const std::string& hostname, int port) {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        throw_network_error("socket creation");
    }
    
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    
    // Try to convert hostname to IP address
    if (inet_pton(AF_INET, hostname.c_str(), &server_address.sin_addr) <= 0) {
        // Not a valid IP address, try hostname resolution
        struct hostent* host_entry = gethostbyname(hostname.c_str());
        if (host_entry == nullptr) {
            close(client_socket);
            throw NetworkException("Failed to resolve hostname: " + hostname);
        }
        
        // Copy the IP address from hostname resolution
        memcpy(&server_address.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
    }
    
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        close(client_socket);
        throw_network_error("connect");
    }
    
    LOG_INFO("Connected to server " << hostname << ":" << port);
    return client_socket;
}

void SocketUtils::send_message(int socket, const Message& message) {
    auto serialized = serialize_message(message);
    send_data(socket, serialized);
}

Message SocketUtils::receive_message(int socket) {
    // First receive message header (type + size)
    uint8_t header[5]; // 1 byte type + 4 bytes size
    receive_all(socket, header, 5);
    
    MessageType type = static_cast<MessageType>(header[0]);
    uint32_t size = (header[1] << 24) | (header[2] << 16) | (header[3] << 8) | header[4];
    
    // Receive message data
    std::vector<uint8_t> data;
    if (size > 0) {
        if (size > MAX_MESSAGE_SIZE) {
            throw NetworkException("Message size too large: " + std::to_string(size));
        }
        data = receive_data(socket, size);
    }
    
    return Message(type, data);
}

void SocketUtils::send_data(int socket, const std::vector<uint8_t>& data) {
    send_all(socket, data.data(), data.size());
}

std::vector<uint8_t> SocketUtils::receive_data(int socket, size_t size) {
    std::vector<uint8_t> data(size);
    receive_all(socket, data.data(), size);
    return data;
}

void SocketUtils::send_wire_label(int socket, const WireLabel& label) {
    send_all(socket, label.data(), WIRE_LABEL_SIZE);
}

WireLabel SocketUtils::receive_wire_label(int socket) {
    WireLabel label;
    receive_all(socket, label.data(), WIRE_LABEL_SIZE);
    return label;
}

void SocketUtils::send_wire_labels(int socket, const std::vector<WireLabel>& labels) {
    uint32_t count = labels.size();
    send_all(socket, &count, sizeof(count));
    
    for (const auto& label : labels) {
        send_wire_label(socket, label);
    }
}

std::vector<WireLabel> SocketUtils::receive_wire_labels(int socket, size_t count) {
    std::vector<WireLabel> labels;
    labels.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        labels.push_back(receive_wire_label(socket));
    }
    
    return labels;
}

void SocketUtils::close_socket(int socket) {
    if (socket >= 0) {
        close(socket);
    }
}

void SocketUtils::set_socket_timeout(int socket, int timeout_seconds) {
    struct timeval timeout;
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        throw_network_error("set receive timeout");
    }
    
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        throw_network_error("set send timeout");
    }
}

bool SocketUtils::socket_ready_for_read(int socket, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = socket;
    pfd.events = POLLIN;
    
    int result = poll(&pfd, 1, timeout_ms);
    if (result < 0) {
        throw_network_error("poll");
    }
    
    return result > 0 && (pfd.revents & POLLIN);
}

bool SocketUtils::socket_ready_for_write(int socket, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = socket;
    pfd.events = POLLOUT;
    
    int result = poll(&pfd, 1, timeout_ms);
    if (result < 0) {
        throw_network_error("poll");
    }
    
    return result > 0 && (pfd.revents & POLLOUT);
}

std::string SocketUtils::get_local_ip() {
    // This is a simplified implementation
    // In practice, you might want to use getifaddrs() for more robust IP detection
    return "127.0.0.1";
}

std::vector<uint8_t> SocketUtils::serialize_message(const Message& message) {
    std::vector<uint8_t> serialized;
    
    // Add message type
    serialized.push_back(static_cast<uint8_t>(message.type));
    
    // Add message size (big-endian)
    uint32_t size = message.size;
    serialized.push_back((size >> 24) & 0xFF);
    serialized.push_back((size >> 16) & 0xFF);
    serialized.push_back((size >> 8) & 0xFF);
    serialized.push_back(size & 0xFF);
    
    // Add message data
    serialized.insert(serialized.end(), message.data.begin(), message.data.end());
    
    return serialized;
}

Message SocketUtils::deserialize_message(const std::vector<uint8_t>& data) {
    if (data.size() < 5) {
        throw NetworkException("Invalid message data: too small");
    }
    
    MessageType type = static_cast<MessageType>(data[0]);
    uint32_t size = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
    
    if (data.size() != 5 + size) {
        throw NetworkException("Invalid message data: size mismatch");
    }
    
    std::vector<uint8_t> msg_data(data.begin() + 5, data.end());
    return Message(type, msg_data);
}

void SocketUtils::send_all(int socket, const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    size_t total_sent = 0;
    
    while (total_sent < size) {
        ssize_t sent = send(socket, bytes + total_sent, size - total_sent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Try again
            }
            throw_network_error("send");
        }
        total_sent += sent;
    }
}

void SocketUtils::receive_all(int socket, void* data, size_t size) {
    uint8_t* bytes = static_cast<uint8_t*>(data);
    size_t total_received = 0;
    
    while (total_received < size) {
        ssize_t received = recv(socket, bytes + total_received, size - total_received, 0);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Try again
            }
            throw_network_error("recv");
        } else if (received == 0) {
            throw NetworkException("Connection closed by peer");
        }
        total_received += received;
    }
}

void SocketUtils::throw_network_error(const std::string& operation) {
    std::stringstream ss;
    ss << operation << " failed: " << std::strerror(errno) << " (errno=" << errno << ")";
    throw NetworkException(ss.str());
}

// SocketConnection implementation
SocketConnection::SocketConnection(int port) 
    : server_socket(-1), comm_socket(-1), is_server(true) {
    server_socket = SocketUtils::create_server_socket(port);
}

SocketConnection::SocketConnection(const std::string& hostname, int port)
    : server_socket(-1), comm_socket(-1), is_server(false) {
    comm_socket = SocketUtils::connect_to_server(hostname, port);
}

SocketConnection::~SocketConnection() {
    cleanup();
}

SocketConnection::SocketConnection(SocketConnection&& other) noexcept
    : server_socket(other.server_socket), comm_socket(other.comm_socket), is_server(other.is_server) {
    other.server_socket = -1;
    other.comm_socket = -1;
}

SocketConnection& SocketConnection::operator=(SocketConnection&& other) noexcept {
    if (this != &other) {
        cleanup();
        server_socket = other.server_socket;
        comm_socket = other.comm_socket;
        is_server = other.is_server;
        other.server_socket = -1;
        other.comm_socket = -1;
    }
    return *this;
}

void SocketConnection::wait_for_client() {
    if (!is_server || server_socket < 0) {
        throw NetworkException("Not a server connection");
    }
    
    if (comm_socket >= 0) {
        SocketUtils::close_socket(comm_socket);
    }
    
    comm_socket = SocketUtils::accept_client(server_socket);
}

void SocketConnection::close() {
    cleanup();
}

void SocketConnection::cleanup() {
    if (comm_socket >= 0) {
        SocketUtils::close_socket(comm_socket);
        comm_socket = -1;
    }
    if (server_socket >= 0) {
        SocketUtils::close_socket(server_socket);
        server_socket = -1;
    }
}

// ProtocolManager implementation
ProtocolManager::ProtocolManager(std::unique_ptr<SocketConnection> conn) 
    : connection(std::move(conn)) {
    if (!connection || !connection->is_connected()) {
        throw NetworkException("Invalid connection provided to ProtocolManager");
    }
}

void ProtocolManager::send_hello(const std::string& party_name) {
    std::vector<uint8_t> data(party_name.begin(), party_name.end());
    Message msg(MessageType::HELLO, data);
    SocketUtils::send_message(connection->get_socket(), msg);
}

std::string ProtocolManager::receive_hello() {
    Message msg = SocketUtils::receive_message(connection->get_socket());
    if (msg.type != MessageType::HELLO) {
        throw NetworkException("Expected HELLO message");
    }
    return std::string(msg.data.begin(), msg.data.end());
}

void ProtocolManager::send_circuit(const GarbledCircuit& garbled_circuit) {
    std::cout << "[PROTOCOL] Sending garbled circuit to evaluator" << std::endl;
    std::cout << "           Circuit: " << garbled_circuit.circuit.gates.size() << " gates, " 
              << garbled_circuit.circuit.num_inputs << " inputs, " 
              << garbled_circuit.circuit.num_outputs << " outputs" << std::endl;
    
    auto serialized = serialize_garbled_circuit(garbled_circuit);
    std::cout << "           Serialized size: " << serialized.size() << " bytes" << std::endl;
    Message msg(MessageType::CIRCUIT, serialized);
    SocketUtils::send_message(connection->get_socket(), msg);
    std::cout << "[PROTOCOL] Circuit transmission completed" << std::endl;
}

GarbledCircuit ProtocolManager::receive_circuit() {
    std::cout << "[PROTOCOL] Waiting to receive garbled circuit..." << std::endl;
    Message msg = SocketUtils::receive_message(connection->get_socket());
    std::cout << "[PROTOCOL] Received circuit data (" << msg.data.size() << " bytes)" << std::endl;
    if (msg.type != MessageType::CIRCUIT) {
        throw NetworkException("Expected CIRCUIT message");
    }
    auto gc = deserialize_garbled_circuit(msg.data);
    std::cout << "[PROTOCOL] Circuit deserialization completed" << std::endl;
    std::cout << "           Circuit: " << gc.circuit.gates.size() << " gates, " 
              << gc.circuit.num_inputs << " inputs, " 
              << gc.circuit.num_outputs << " outputs" << std::endl;
    return gc;
}

void ProtocolManager::send_input_labels(const std::vector<WireLabel>& labels) {
    std::vector<uint8_t> data;
    
    // Add count
    uint32_t count = labels.size();
    data.push_back((count >> 24) & 0xFF);
    data.push_back((count >> 16) & 0xFF);
    data.push_back((count >> 8) & 0xFF);
    data.push_back(count & 0xFF);
    
    // Add labels
    for (const auto& label : labels) {
        data.insert(data.end(), label.begin(), label.end());
    }
    
    Message msg(MessageType::INPUT_LABELS, data);
    SocketUtils::send_message(connection->get_socket(), msg);
}

std::vector<WireLabel> ProtocolManager::receive_input_labels(size_t expected_count) {
    Message msg = SocketUtils::receive_message(connection->get_socket());
    if (msg.type != MessageType::INPUT_LABELS) {
        throw NetworkException("Expected INPUT_LABELS message");
    }
    
    if (msg.data.size() < 4) {
        throw NetworkException("Invalid input labels message");
    }
    
    uint32_t count = (msg.data[0] << 24) | (msg.data[1] << 16) | (msg.data[2] << 8) | msg.data[3];
    if (count != expected_count) {
        throw NetworkException("Input labels count mismatch");
    }
    
    std::vector<WireLabel> labels;
    labels.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        size_t offset = 4 + i * WIRE_LABEL_SIZE;
        if (msg.data.size() < offset + WIRE_LABEL_SIZE) {
            throw NetworkException("Insufficient data for labels");
        }
        
        WireLabel label;
        std::copy(msg.data.begin() + offset, msg.data.begin() + offset + WIRE_LABEL_SIZE, label.begin());
        labels.push_back(label);
    }
    
    return labels;
}

void ProtocolManager::send_result(const std::vector<uint8_t>& result) {
    Message msg(MessageType::RESULT, result);
    SocketUtils::send_message(connection->get_socket(), msg);
}

std::vector<uint8_t> ProtocolManager::receive_result() {
    Message msg = SocketUtils::receive_message(connection->get_socket());
    if (msg.type != MessageType::RESULT) {
        throw NetworkException("Expected RESULT message");
    }
    return msg.data;
}

void ProtocolManager::send_error(const std::string& error_message) {
    std::vector<uint8_t> data(error_message.begin(), error_message.end());
    Message msg(MessageType::ERROR, data);
    SocketUtils::send_message(connection->get_socket(), msg);
}

Message ProtocolManager::receive_any_message() {
    return SocketUtils::receive_message(connection->get_socket());
}

void ProtocolManager::send_goodbye() {
    Message msg(MessageType::GOODBYE, {});
    SocketUtils::send_message(connection->get_socket(), msg);
}

bool ProtocolManager::is_connected() const {
    return connection && connection->is_connected();
}

std::vector<uint8_t> ProtocolManager::serialize_garbled_circuit(const GarbledCircuit& gc) {
    std::vector<uint8_t> data;
    
    // Add circuit basic info
    uint32_t num_gates = gc.circuit.num_gates;
    uint32_t num_inputs = gc.circuit.num_inputs;
    uint32_t num_outputs = gc.circuit.num_outputs;
    
    data.push_back((num_gates >> 24) & 0xFF);
    data.push_back((num_gates >> 16) & 0xFF);
    data.push_back((num_gates >> 8) & 0xFF);
    data.push_back(num_gates & 0xFF);
    
    data.push_back((num_inputs >> 24) & 0xFF);
    data.push_back((num_inputs >> 16) & 0xFF);
    data.push_back((num_inputs >> 8) & 0xFF);
    data.push_back(num_inputs & 0xFF);
    
    data.push_back((num_outputs >> 24) & 0xFF);
    data.push_back((num_outputs >> 16) & 0xFF);
    data.push_back((num_outputs >> 8) & 0xFF);
    data.push_back(num_outputs & 0xFF);
    
    // Add input wires
    for (int wire : gc.circuit.input_wires) {
        data.push_back((wire >> 24) & 0xFF);
        data.push_back((wire >> 16) & 0xFF);
        data.push_back((wire >> 8) & 0xFF);
        data.push_back(wire & 0xFF);
    }
    
    // Add output wires  
    for (int wire : gc.circuit.output_wires) {
        data.push_back((wire >> 24) & 0xFF);
        data.push_back((wire >> 16) & 0xFF);
        data.push_back((wire >> 8) & 0xFF);
        data.push_back(wire & 0xFF);
    }
    
    // Add gates
    for (const auto& gate : gc.circuit.gates) {
        data.push_back((gate.input_wire1 >> 24) & 0xFF);
        data.push_back((gate.input_wire1 >> 16) & 0xFF);
        data.push_back((gate.input_wire1 >> 8) & 0xFF);
        data.push_back(gate.input_wire1 & 0xFF);
        
        data.push_back((gate.input_wire2 >> 24) & 0xFF);
        data.push_back((gate.input_wire2 >> 16) & 0xFF);
        data.push_back((gate.input_wire2 >> 8) & 0xFF);
        data.push_back(gate.input_wire2 & 0xFF);
        
        data.push_back((gate.output_wire >> 24) & 0xFF);
        data.push_back((gate.output_wire >> 16) & 0xFF);
        data.push_back((gate.output_wire >> 8) & 0xFF);
        data.push_back(gate.output_wire & 0xFF);
        
        data.push_back(static_cast<uint8_t>(gate.type));
    }
    
    // Add garbled gates (ciphertexts)
    for (size_t i = 0; i < gc.garbled_gates.size(); ++i) {
        const auto& garbled_gate = gc.garbled_gates[i];
        for (size_t j = 0; j < garbled_gate.ciphertexts.size(); ++j) {
            const auto& ciphertext = garbled_gate.ciphertexts[j];
            data.insert(data.end(), ciphertext.begin(), ciphertext.end());
        }
    }
    
    return data;
}

GarbledCircuit ProtocolManager::deserialize_garbled_circuit(const std::vector<uint8_t>& data) {
    if (data.size() < 12) {
        throw NetworkException("Invalid garbled circuit data");
    }
    
    GarbledCircuit gc;
    size_t offset = 0;
    
    // Deserialize basic info
    uint32_t num_gates = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    uint32_t num_inputs = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    uint32_t num_outputs = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
    
    gc.circuit.num_gates = num_gates;
    gc.circuit.num_inputs = num_inputs;
    gc.circuit.num_outputs = num_outputs;
    offset = 12;
    
    // Deserialize input wires
    for (uint32_t i = 0; i < num_inputs; ++i) {
        if (offset + 4 > data.size()) throw NetworkException("Invalid circuit data: input wires");
        int wire = (data[offset] << 24) | (data[offset+1] << 16) | (data[offset+2] << 8) | data[offset+3];
        gc.circuit.input_wires.push_back(wire);
        offset += 4;
    }
    
    // Deserialize output wires
    for (uint32_t i = 0; i < num_outputs; ++i) {
        if (offset + 4 > data.size()) throw NetworkException("Invalid circuit data: output wires");
        int wire = (data[offset] << 24) | (data[offset+1] << 16) | (data[offset+2] << 8) | data[offset+3];
        gc.circuit.output_wires.push_back(wire);
        offset += 4;
    }
    
    // Deserialize gates
    for (uint32_t i = 0; i < num_gates; ++i) {
        if (offset + 13 > data.size()) throw NetworkException("Invalid circuit data: gates");
        
        int input1 = (data[offset] << 24) | (data[offset+1] << 16) | (data[offset+2] << 8) | data[offset+3];
        offset += 4;
        
        int input2 = (data[offset] << 24) | (data[offset+1] << 16) | (data[offset+2] << 8) | data[offset+3];
        offset += 4;
        
        int output = (data[offset] << 24) | (data[offset+1] << 16) | (data[offset+2] << 8) | data[offset+3];
        offset += 4;
        
        GateType gate_type = static_cast<GateType>(data[offset]);
        offset += 1;
        
        Gate gate(output, input1, input2, gate_type);
        gc.circuit.gates.push_back(gate);
    }
    
    // Deserialize garbled gates (ciphertexts)
    std::cout << "[DEBUG] Deserializing " << num_gates << " garbled gates, offset=" << offset << ", total size=" << data.size() << std::endl;
    gc.garbled_gates.resize(num_gates);
    for (uint32_t i = 0; i < num_gates; ++i) {
        for (int j = 0; j < 4; ++j) { // 4 ciphertexts per truth table
            size_t ciphertext_size = WIRE_LABEL_SIZE + 16; // Size as defined in GarbledGate
            if (offset + ciphertext_size > data.size()) {
                throw NetworkException("Invalid circuit data: garbled gates");
            }
            
            std::vector<uint8_t> ciphertext(data.begin() + offset, data.begin() + offset + ciphertext_size);
            gc.garbled_gates[i].ciphertexts[j] = ciphertext;
            offset += ciphertext_size;
        }
    }
    
    return gc;
}
