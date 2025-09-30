#ifndef MULTICAST_SUBSCRIBER_HPP
#define MULTICAST_SUBSCRIBER_HPP

#include <string>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <functional>
#include "../order_book_processor/orderbook.hpp"

// Forward declaration
struct MulticastMessage;

// UDP Multicast Subscriber
class MulticastSubscriber {
public:
    MulticastSubscriber();
    ~MulticastSubscriber();
    
    // Initialize multicast socket
    bool initialize(const std::string& multicast_group, int port);
    
    // Start listening for multicast messages
    bool start_listening();
    
    // Stop listening
    void stop_listening();
    
    // Check if listening
    bool is_listening() const { return listening_.load(); }
    
    // Set callbacks for different message types
    void set_order_book_callback(std::function<void(const std::string&, const std::string&)> callback);
    void set_trade_callback(std::function<void(const std::string&, const std::string&)> callback);
    void set_heartbeat_callback(std::function<void(const std::string&)> callback);
    
    // Get statistics
    uint64_t get_messages_received() const { return messages_received_; }
    uint64_t get_bytes_received() const { return bytes_received_; }
    uint64_t get_parse_errors() const { return parse_errors_; }
    
    // Get multicast group and port
    std::string get_multicast_group() const { return multicast_group_; }
    int get_port() const { return port_; }
    
private:
    // Main listening loop
    void listen_loop();
    
    // Parse incoming message
    bool parse_message(const std::string& json_str, MulticastMessage& message);
    
    // Handle different message types
    void handle_order_book_update(const std::string& symbol, const std::string& data);
    void handle_trade_update(const std::string& symbol, const std::string& data);
    void handle_heartbeat(const std::string& data);
    
    // Member variables
    int socket_fd_;
    struct sockaddr_in multicast_addr_;
    struct sockaddr_in local_addr_;
    std::string multicast_group_;
    int port_;
    std::atomic<bool> listening_;
    std::unique_ptr<std::thread> listen_thread_;
    
    // Callbacks
    std::function<void(const std::string&, const std::string&)> order_book_callback_;
    std::function<void(const std::string&, const std::string&)> trade_callback_;
    std::function<void(const std::string&)> heartbeat_callback_;
    
    // Statistics
    std::atomic<uint64_t> messages_received_;
    std::atomic<uint64_t> bytes_received_;
    std::atomic<uint64_t> parse_errors_;
};

#endif // MULTICAST_SUBSCRIBER_HPP
