#pragma once

#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

/**
 * Socket utilities for network communication between garbler and evaluator
 */
class SocketUtils {
public:
    /**
     * Server-side functions (for garbler)
     */
    
    // Create and bind server socket
    static int create_server_socket(int port);
    
    // Wait for client connection
    static int accept_client(int server_socket);
    
    /**
     * Client-side functions (for evaluator)  
     */
    
    // Connect to server
    static int connect_to_server(const std::string& hostname, int port);
    
    /**
     * Communication functions (for both sides)
     */
    
    // Send message over socket
    static void send_message(int socket, const Message& message);
    
    // Receive message from socket
    static Message receive_message(int socket);
    
    // Send raw data
    static void send_data(int socket, const std::vector<uint8_t>& data);
    
    // Receive raw data of specified size
    static std::vector<uint8_t> receive_data(int socket, size_t size);
    
    // Send wire label
    static void send_wire_label(int socket, const WireLabel& label);
    
    // Receive wire label
    static WireLabel receive_wire_label(int socket);
    
    // Send multiple wire labels
    static void send_wire_labels(int socket, const std::vector<WireLabel>& labels);
    
    // Receive multiple wire labels
    static std::vector<WireLabel> receive_wire_labels(int socket, size_t count);
    
    /**
     * Utility functions
     */
    
    // Close socket safely
    static void close_socket(int socket);
    
    // Set socket timeout
    static void set_socket_timeout(int socket, int timeout_seconds);
    
    // Check if socket is ready for reading
    static bool socket_ready_for_read(int socket, int timeout_ms);
    
    // Check if socket is ready for writing
    static bool socket_ready_for_write(int socket, int timeout_ms);
    
    // Get local IP address
    static std::string get_local_ip();
    
    // Serialize message to bytes
    static std::vector<uint8_t> serialize_message(const Message& message);
    
    // Deserialize bytes to message
    static Message deserialize_message(const std::vector<uint8_t>& data);

private:
    // Send all data (handles partial sends)
    static void send_all(int socket, const void* data, size_t size);
    
    // Receive all data (handles partial receives)
    static void receive_all(int socket, void* data, size_t size);
    
    // Convert network error to exception
    static void throw_network_error(const std::string& operation);
};

/**
 * RAII wrapper for socket management
 */
class SocketConnection {
public:
    // Constructor for server-side (garbler)
    explicit SocketConnection(int port);
    
    // Constructor for client-side (evaluator)
    SocketConnection(const std::string& hostname, int port);
    
    // Destructor - automatically closes sockets
    ~SocketConnection();
    
    // Non-copyable but movable
    SocketConnection(const SocketConnection&) = delete;
    SocketConnection& operator=(const SocketConnection&) = delete;
    SocketConnection(SocketConnection&& other) noexcept;
    SocketConnection& operator=(SocketConnection&& other) noexcept;
    
    // Get the communication socket
    int get_socket() const { return comm_socket; }
    
    // Check if connection is valid
    bool is_connected() const { return comm_socket >= 0; }
    
    // Wait for client (server-side only)
    void wait_for_client();
    
    // Close connection
    void close();

private:
    int server_socket;  // Server listening socket (garbler only)
    int comm_socket;    // Communication socket (both)
    bool is_server;     // True if this is server-side connection
    
    void cleanup();
};

/**
 * Protocol manager for structured communication
 */
class ProtocolManager {
public:
    explicit ProtocolManager(std::unique_ptr<SocketConnection> connection);
    ~ProtocolManager() = default;
    
    // Non-copyable but movable
    ProtocolManager(const ProtocolManager&) = delete;
    ProtocolManager& operator=(const ProtocolManager&) = delete;
    ProtocolManager(ProtocolManager&& other) noexcept = default;
    ProtocolManager& operator=(ProtocolManager&& other) noexcept = default;
    
    /**
     * Protocol message exchange functions
     */
    
    // Send hello message
    void send_hello(const std::string& party_name);
    
    // Receive hello message
    std::string receive_hello();
    
    // Send circuit (garbler -> evaluator)
    void send_circuit(const GarbledCircuit& garbled_circuit);
    
    // Receive circuit (evaluator <- garbler)
    GarbledCircuit receive_circuit();
    
    // Send input labels (garbler -> evaluator)
    void send_input_labels(const std::vector<WireLabel>& labels);
    
    // Receive input labels (evaluator <- garbler)
    std::vector<WireLabel> receive_input_labels(size_t count);
    
    // Send result (evaluator -> garbler)
    void send_result(const std::vector<uint8_t>& result);
    
    // Receive result (garbler <- evaluator)
    std::vector<uint8_t> receive_result();
    
    // Send error message
    void send_error(const std::string& error_message);
    
    // Receive any message (for error handling)
    Message receive_any_message();
    
    // Send goodbye
    void send_goodbye();
    
    // Check if connection is still alive
    bool is_connected() const;
    std::unique_ptr<SocketConnection> connection;
    

private:
    
    // Serialize garbled circuit for network transmission
    std::vector<uint8_t> serialize_garbled_circuit(const GarbledCircuit& gc);
    
    // Deserialize network data to garbled circuit
    GarbledCircuit deserialize_garbled_circuit(const std::vector<uint8_t>& data);
};
