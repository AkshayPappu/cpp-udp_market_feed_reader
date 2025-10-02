#ifndef LISTENER_HPP
#define LISTENER_HPP

#include <string>
#include <cstdint>
#include <functional>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <sstream>
#include "quote.hpp"

class UDPListener {
private:
    // member variables for UDP socket
    // - Socket file descriptor
    // - Port number
    // - Multicast group (optional)
    // - Buffer for receiving data
    // - Callback function for quote processing (old, unused)
    
    int socket_fd_;
    uint16_t port_;
    std::string multicast_group_;
    struct ip_mreq mreq_;
    bool is_multicast_;
    std::function<void(const Quote&)> quote_callback_;
    std::function<void(const OrderBookEvent&)> order_book_callback_;
    std::atomic<bool>* shutdown_flag_;  // Pointer to global shutdown flag

public:
    // Constructor for unicast
    explicit UDPListener(uint16_t port) 
        : socket_fd_(-1), port_(port), multicast_group_(""), is_multicast_(false),
          quote_callback_(nullptr), order_book_callback_(nullptr), shutdown_flag_(nullptr) {
        // Constructor just initializes member variables
        // socket_fd_ = -1 indicates no socket is open yet
        // quote_callback_ is set to nullptr initially
        // shutdown_flag_ is set to nullptr initially
    }
    
    // Constructor for multicast
    UDPListener(const std::string& multicast_group, uint16_t port)
        : socket_fd_(-1), port_(port), multicast_group_(multicast_group), is_multicast_(true),
          quote_callback_(nullptr), order_book_callback_(nullptr), shutdown_flag_(nullptr) {
    }
    
    // Destructor
    ~UDPListener() {
        // Destructor ensures socket is properly closed
        if (socket_fd_ != -1) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }
    
    // Setup and teardown
    bool initialize() {
        // Create UDP socket
        // AF_INET = IPv4, SOCK_DGRAM = UDP protocol
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ == -1) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Set socket options for reuse address and port
        // This allows the socket to bind to an address that's already in use
        int opt = 1;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        // Set up server address structure
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;           // IPv4
        server_addr.sin_addr.s_addr = INADDR_ANY;   // Listen on all interfaces
        server_addr.sin_port = htons(port_);        // Convert port to network byte order
        
        // Bind socket to address and port
        // This tells the OS which port to listen on for incoming UDP packets
        if (bind(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind socket to port " << port_ << ": " << strerror(errno) << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        // If this is a multicast listener, join the multicast group
        if (is_multicast_ && !multicast_group_.empty()) {
            // Set up multicast group membership
            mreq_.imr_multiaddr.s_addr = inet_addr(multicast_group_.c_str());
            mreq_.imr_interface.s_addr = INADDR_ANY;  // Use default interface
            
            // Join multicast group
            if (setsockopt(socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq_, sizeof(mreq_)) < 0) {
                std::cerr << "Failed to join multicast group " << multicast_group_ 
                          << ": " << strerror(errno) << std::endl;
                close(socket_fd_);
                socket_fd_ = -1;
                return false;
            }
            
            std::cout << "Multicast listener joined group " << multicast_group_ 
                      << " on port " << port_ << std::endl;
        } else {
            // Verify binding was successful for unicast
            struct sockaddr_in bound_addr;
            socklen_t bound_addr_len = sizeof(bound_addr);
            if (getsockname(socket_fd_, (struct sockaddr*)&bound_addr, &bound_addr_len) == 0) {
                std::cout << "UDP listener bound to " << inet_ntoa(bound_addr.sin_addr) 
                          << ":" << ntohs(bound_addr.sin_port) << std::endl;
            }
            
            std::cout << "UDP listener initialized on port " << port_ << std::endl;
        }
        
        return true;
    }
    
    void shutdown() {
        if (socket_fd_ != -1) {
            // Leave multicast group if this was a multicast listener
            if (is_multicast_ && !multicast_group_.empty()) {
                if (setsockopt(socket_fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq_, sizeof(mreq_)) < 0) {
                    std::cerr << "Warning: Failed to leave multicast group: " << strerror(errno) << std::endl;
                }
                std::cout << "Multicast listener left group " << multicast_group_ << std::endl;
            }
            
            close(socket_fd_);
            socket_fd_ = -1;
            std::cout << "UDP listener shutdown complete" << std::endl;
        }
    }
    
    // Main listening loop
    void listen() {
        if (socket_fd_ == -1) {
            std::cerr << "Cannot listen: socket not initialized" << std::endl;
            return;
        }
        
        if (is_multicast_) {
            std::cout << "Listening for multicast packets from " << multicast_group_ 
                      << ":" << port_ << "..." << std::endl;
        } else {
            std::cout << "Listening for UDP packets on port " << port_ << "..." << std::endl;
        }
        std::cout << "Socket FD: " << socket_fd_ << std::endl;
        
        // Buffer to receive incoming data
        char buffer[1024];  // size based on data format
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        while (true) {  // Keep listening until explicitly told to stop
            // Clear client address structure
            memset(&client_addr, 0, sizeof(client_addr));
            
            // Receive UDP packet (non-blocking with MSG_DONTWAIT)
            // recvfrom() blocks until data arrives, then copies data to buffer
            ssize_t bytes_received = recvfrom(socket_fd_, buffer, sizeof(buffer), 
                                            MSG_DONTWAIT, 
                                            (struct sockaddr*)&client_addr, &client_addr_len);
            
            if (bytes_received > 0) {
                // Successfully received data
                // Parse the received JSON data into an OrderBookEvent structure
                if (order_book_callback_) {
                    try {
                        // Convert buffer to string for parsing
                        std::string json_str(buffer, bytes_received);
                        
                        // Parse JSON into OrderBookEvent
                        OrderBookEvent event = parse_json_order_book_event(json_str);
                        
                        // Monotonic receive timestamp for latency measurement
                        event.udp_rx_mono_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
                        
                        // Call the callback function with the parsed event
                        order_book_callback_(event);
                        
                    } catch (const std::exception& e) {
                        std::cerr << "Error parsing order book event: " << e.what() << std::endl;
                        std::cerr << "Raw data: " << std::string(buffer, bytes_received) << std::endl;
                        // Don't crash - continue listening
                    }
                }
                // Legacy quote callback support
                else if (quote_callback_) {
                    try {
                        // Convert buffer to string for parsing
                        std::string json_str(buffer, bytes_received);
                        
                        // Simple JSON parsing (for learning - in production use a proper library)
                        Quote quote = parse_json_quote(json_str);
                        
                        // Monotonic receive timestamp for latency measurement
                        quote.udp_rx_mono_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
                        
                        // Call the callback function with the parsed quote
                        quote_callback_(quote);
                        
                    } catch (const std::exception& e) {
                        std::cerr << "Error parsing quote: " << e.what() << std::endl;
                        std::cerr << "Raw data: " << std::string(buffer, bytes_received) << std::endl;
                        // Don't crash - continue listening
                    }
                }
                
            } else if (bytes_received == -1) {
                // Error or no data available
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available right now (non-blocking mode)
                    // This is normal in non-blocking UDP listening
                    // Add a small sleep to prevent busy-waiting and check shutdown flag
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    
                    // Debug removed: previously printed periodic 'Still listening...' messages
                } else {
                    // Actual error occurred
                    std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
                    break;
                }
            }
            
            // Check shutdown flag periodically for responsive shutdown
            if (shutdown_flag_ && *shutdown_flag_) {
                break;
            }
        }
        
        std::cout << "UDP listener stopped" << std::endl;
    }
    
    // Set callback for quote processing
    void set_quote_callback(std::function<void(const Quote&)> callback) {
        quote_callback_ = callback;
    }
    
    // Set callback for order book event processing
    void set_order_book_callback(std::function<void(const OrderBookEvent&)> callback) {
        order_book_callback_ = callback;
    }
    
    // Set shutdown flag for graceful shutdown
    void set_shutdown_flag(std::atomic<bool>* flag) {
        shutdown_flag_ = flag;
    }
    
    // Utility functions
    bool is_listening() const {
        return socket_fd_ != -1;
    }
    
    uint16_t get_port() const {
        return port_;
    }
    
    // Disable copy constructor and assignment
    UDPListener(const UDPListener&) = delete;
    UDPListener& operator=(const UDPListener&) = delete;

private:
    // Helper function to parse JSON order book events
    OrderBookEvent parse_json_order_book_event(const std::string& json_str) {
        OrderBookEvent event;

        auto find_string = [&](const char* key) -> std::string {
            std::string pattern = std::string("\"") + key + "\"";
            size_t k = json_str.find(pattern);
            if (k == std::string::npos) return {};
            k = json_str.find(':', k);
            if (k == std::string::npos) return {};
            // skip spaces
            while (k + 1 < json_str.size() && (json_str[k+1] == ' ')) ++k;
            if (k + 2 < json_str.size() && json_str[k+1] == '"') {
                size_t start = k + 2;
                size_t end = json_str.find('"', start);
                if (end == std::string::npos) return {};
                return json_str.substr(start, end - start);
            }
            return {};
        };

        auto find_number = [&](const char* key) -> std::string {
            std::string pattern = std::string("\"") + key + "\"";
            size_t k = json_str.find(pattern);
            if (k == std::string::npos) return {};
            k = json_str.find(':', k);
            if (k == std::string::npos) return {};
            // move to first digit/sign/decimal
            size_t i = k + 1;
            while (i < json_str.size() && (json_str[i] == ' ')) ++i;
            size_t start = i;
            while (i < json_str.size() && (std::isdigit(json_str[i]) || json_str[i] == '.' || json_str[i] == '-' )) ++i;
            return json_str.substr(start, i - start);
        };

        auto find_bool = [&](const char* key) -> bool {
            std::string pattern = std::string("\"") + key + "\"";
            size_t k = json_str.find(pattern);
            if (k == std::string::npos) return false;
            k = json_str.find(':', k);
            if (k == std::string::npos) return false;
            // skip spaces
            while (k + 1 < json_str.size() && (json_str[k+1] == ' ')) ++k;
            if (k + 4 < json_str.size() && json_str.substr(k+1, 4) == "true") return true;
            if (k + 5 < json_str.size() && json_str.substr(k+1, 5) == "false") return false;
            return false;
        };

        // Extract basic fields
        std::string sym = find_string("symbol");
        std::string ex = find_string("exchange");
        std::string event_type_str = find_string("event_type");
        std::string side_str = find_string("side");
        std::string order_id = find_string("order_id");
        
        // Extract numeric fields
        std::string price_str = find_number("price");
        std::string size_str = find_number("size");
        std::string remaining_size_str = find_number("remaining_size");
        std::string trade_price_str = find_number("trade_price");
        std::string trade_size_str = find_number("trade_size");
        std::string ts = find_number("timestamp");
        std::string seq = find_number("sequence_number");
        std::string ex_mono = find_number("exchange_mono_ns");
        
        // Extract boolean fields
        bool is_aggressor = find_bool("is_aggressor");
        bool is_trading_halted = find_bool("is_trading_halted");
        
        // Extract status message
        std::string status_msg = find_string("status_message");

        // Set basic fields
        if (!sym.empty()) event.symbol = sym;
        if (!ex.empty()) event.exchange = ex;
        if (!order_id.empty()) event.order_id = order_id;
        if (!status_msg.empty()) event.status_message = status_msg;
        
        // Parse event type
        if (event_type_str == "ADD_ORDER") event.event_type = OrderBookEventType::ADD_ORDER;
        else if (event_type_str == "MODIFY_ORDER") event.event_type = OrderBookEventType::MODIFY_ORDER;
        else if (event_type_str == "CANCEL_ORDER") event.event_type = OrderBookEventType::CANCEL_ORDER;
        else if (event_type_str == "DELETE_ORDER") event.event_type = OrderBookEventType::DELETE_ORDER;
        else if (event_type_str == "TRADE") event.event_type = OrderBookEventType::TRADE;
        else if (event_type_str == "QUOTE_UPDATE") event.event_type = OrderBookEventType::QUOTE_UPDATE;
        else if (event_type_str == "MARKET_STATUS") event.event_type = OrderBookEventType::MARKET_STATUS;
        else event.event_type = OrderBookEventType::UNKNOWN;
        
        // Parse side
        if (side_str == "BID") event.side = OrderSide::BID;
        else if (side_str == "ASK") event.side = OrderSide::ASK;
        else event.side = OrderSide::UNKNOWN;
        
        // Parse numeric fields
        if (!price_str.empty()) event.price = std::stod(price_str);
        if (!size_str.empty()) event.size = static_cast<uint32_t>(std::stoul(size_str));
        if (!remaining_size_str.empty()) event.remaining_size = static_cast<uint32_t>(std::stoul(remaining_size_str));
        if (!trade_price_str.empty()) event.trade_price = std::stod(trade_price_str);
        if (!trade_size_str.empty()) event.trade_size = static_cast<uint32_t>(std::stoul(trade_size_str));
        if (!ts.empty()) event.timestamp = std::stoull(ts);
        if (!seq.empty()) event.sequence_number = std::stoull(seq);
        if (!ex_mono.empty()) event.exchange_mono_ns = std::stoull(ex_mono);
        
        // Set boolean fields
        event.is_aggressor = is_aggressor;
        event.is_trading_halted = is_trading_halted;

        return event;
    }

    // Old, unused function for parsing quotes
    Quote parse_json_quote(const std::string& json_str) {
        Quote quote;

        auto find_string = [&](const char* key) -> std::string {
            std::string pattern = std::string("\"") + key + "\"";
            size_t k = json_str.find(pattern);
            if (k == std::string::npos) return {};
            k = json_str.find(':', k);
            if (k == std::string::npos) return {};
            // skip spaces
            while (k + 1 < json_str.size() && (json_str[k+1] == ' ')) ++k;
            if (k + 2 < json_str.size() && json_str[k+1] == '"') {
                size_t start = k + 2;
                size_t end = json_str.find('"', start);
                if (end == std::string::npos) return {};
                return json_str.substr(start, end - start);
            }
            return {};
        };

        auto find_number = [&](const char* key) -> std::string {
            std::string pattern = std::string("\"") + key + "\"";
            size_t k = json_str.find(pattern);
            if (k == std::string::npos) return {};
            k = json_str.find(':', k);
            if (k == std::string::npos) return {};
            // move to first digit/sign/decimal
            size_t i = k + 1;
            while (i < json_str.size() && (json_str[i] == ' ')) ++i;
            size_t start = i;
            while (i < json_str.size() && (std::isdigit(json_str[i]) || json_str[i] == '.' || json_str[i] == '-' )) ++i;
            return json_str.substr(start, i - start);
        };

        // Extract fields tolerant to spaces
        std::string sym = find_string("symbol");
        std::string ex = find_string("exchange");
        std::string bid_p = find_number("bid_price");
        std::string bid_s = find_number("bid_size");
        std::string ask_p = find_number("ask_price");
        std::string ask_s = find_number("ask_size");
        std::string ts = find_number("timestamp");
        std::string ex_mono = find_number("exchange_mono_ns");

        if (!sym.empty()) quote.symbol = sym; else quote.symbol.clear();
        if (!ex.empty()) quote.exchange = ex; else quote.exchange.clear();
        if (!bid_p.empty()) quote.bid_price = std::stod(bid_p); else quote.bid_price = 0.0;
        if (!bid_s.empty()) quote.bid_size = static_cast<uint32_t>(std::stoul(bid_s)); else quote.bid_size = 0;
        if (!ask_p.empty()) quote.ask_price = std::stod(ask_p); else quote.ask_price = 0.0;
        if (!ask_s.empty()) quote.ask_size = static_cast<uint32_t>(std::stoul(ask_s)); else quote.ask_size = 0;
        if (!ts.empty()) quote.timestamp = std::stoull(ts); else quote.timestamp = 0ULL;
        if (!ex_mono.empty()) quote.exchange_mono_ns = std::stoull(ex_mono); else quote.exchange_mono_ns = 0ULL;

        return quote;
    }
};

#endif // LISTENER_HPP 