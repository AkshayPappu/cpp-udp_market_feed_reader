#ifndef MULTICAST_PUBLISHER_HPP
#define MULTICAST_PUBLISHER_HPP

#include <string>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../order_book_processor/orderbook.hpp"

// Message types for multicast
enum class MulticastMessageType {
    ORDER_BOOK_UPDATE,
    TRADE_UPDATE,
    HEARTBEAT
};

// Multicast message structure
struct MulticastMessage {
    MulticastMessageType type;
    std::string symbol;
    uint64_t timestamp;
    std::string data; // JSON payload
    
    MulticastMessage() = default;
    MulticastMessage(MulticastMessageType t, const std::string& s, uint64_t ts, const std::string& d)
        : type(t), symbol(s), timestamp(ts), data(d) {}
};

// UDP Multicast Publisher
class MulticastPublisher {
public:
    MulticastPublisher();
    ~MulticastPublisher();
    
    // Initialize multicast socket
    bool initialize(const std::string& multicast_group, int port, int ttl = 1);
    
    // Publish order book updates
    void publish_order_book_update(const std::string& symbol, const OrderBook& book, uint64_t timestamp);
    
    // Publish trade updates
    void publish_trade_update(const std::string& symbol, double price, uint32_t size, 
                            OrderSide aggressor_side, uint64_t timestamp);
    
    // Publish heartbeat
    void publish_heartbeat();
    
    // Check if initialized
    bool is_initialized() const { return socket_fd_ >= 0; }
    
    // Get multicast group and port
    std::string get_multicast_group() const { return multicast_group_; }
    int get_port() const { return port_; }
    
private:
    // Send message over multicast
    bool send_message(const MulticastMessage& message);
    
    // Convert order book to JSON
    std::string order_book_to_json(const OrderBook& book);
    
    // Convert trade to JSON
    std::string trade_to_json(double price, uint32_t size, OrderSide aggressor_side);
    
    // Member variables
    int socket_fd_;
    struct sockaddr_in multicast_addr_;
    std::string multicast_group_;
    int port_;
    bool initialized_;
    
    // Message counters for debugging
    uint64_t messages_sent_;
    uint64_t bytes_sent_;
};

#endif // MULTICAST_PUBLISHER_HPP
