#include "multicast_subscriber.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

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

MulticastSubscriber::MulticastSubscriber() 
    : socket_fd_(-1), port_(0), listening_(false), messages_received_(0), bytes_received_(0), parse_errors_(0) {
}

MulticastSubscriber::~MulticastSubscriber() {
    stop_listening();
    if (socket_fd_ >= 0) {
        close(socket_fd_);
    }
}

bool MulticastSubscriber::initialize(const std::string& multicast_group, int port) {
    multicast_group_ = multicast_group;
    port_ = port;
    
    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "Failed to create multicast socket" << std::endl;
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR" << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    // Set up local address
    memset(&local_addr_, 0, sizeof(local_addr_));
    local_addr_.sin_family = AF_INET;
    local_addr_.sin_addr.s_addr = INADDR_ANY;
    local_addr_.sin_port = htons(port);
    
    // Bind to local address
    if (bind(socket_fd_, (struct sockaddr*)&local_addr_, sizeof(local_addr_)) < 0) {
        std::cerr << "Failed to bind multicast socket: " << strerror(errno) << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    // Set up multicast address
    memset(&multicast_addr_, 0, sizeof(multicast_addr_));
    multicast_addr_.sin_family = AF_INET;
    multicast_addr_.sin_port = htons(port);
    
    if (inet_pton(AF_INET, multicast_group.c_str(), &multicast_addr_.sin_addr) <= 0) {
        std::cerr << "Invalid multicast group address: " << multicast_group << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    // Join multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = multicast_addr_.sin_addr.s_addr;
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        std::cerr << "Failed to join multicast group: " << strerror(errno) << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    std::cout << "Multicast subscriber initialized: " << multicast_group << ":" << port << std::endl;
    
    return true;
}

bool MulticastSubscriber::start_listening() {
    if (listening_.load()) {
        return true;
    }
    
    if (socket_fd_ < 0) {
        std::cerr << "Multicast subscriber not initialized" << std::endl;
        return false;
    }
    
    listening_.store(true);
    listen_thread_ = std::make_unique<std::thread>(&MulticastSubscriber::listen_loop, this);
    
    std::cout << "Multicast subscriber started listening" << std::endl;
    return true;
}

void MulticastSubscriber::stop_listening() {
    if (!listening_.load()) {
        return;
    }
    
    listening_.store(false);
    
    if (listen_thread_ && listen_thread_->joinable()) {
        listen_thread_->join();
    }
    
    std::cout << "Multicast subscriber stopped listening" << std::endl;
}

void MulticastSubscriber::listen_loop() {
    char buffer[4096];
    
    while (listening_.load()) {
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);
        
        ssize_t bytes_received = recvfrom(socket_fd_, buffer, sizeof(buffer) - 1, 0,
                                         (struct sockaddr*)&sender_addr, &sender_len);
        
        if (bytes_received < 0) {
            if (listening_.load()) {
                std::cerr << "Failed to receive multicast message: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        buffer[bytes_received] = '\0';
        std::string json_str(buffer);
        
        messages_received_++;
        bytes_received_ += bytes_received;
        
        // Parse and handle message
        MulticastMessage message;
        if (parse_message(json_str, message)) {
            switch (message.type) {
                case MulticastMessageType::ORDER_BOOK_UPDATE:
                    handle_order_book_update(message.symbol, message.data);
                    break;
                case MulticastMessageType::TRADE_UPDATE:
                    handle_trade_update(message.symbol, message.data);
                    break;
                case MulticastMessageType::HEARTBEAT:
                    handle_heartbeat(message.data);
                    break;
                default:
                    std::cerr << "Unknown message type: " << static_cast<int>(message.type) << std::endl;
                    break;
            }
        } else {
            parse_errors_++;
            std::cerr << "Failed to parse message: " << json_str << std::endl;
        }
    }
}

bool MulticastSubscriber::parse_message(const std::string& json_str, MulticastMessage& message) {
    // Simple JSON parsing (in production, use a proper JSON library)
    // This is a basic implementation for demonstration
    
    try {
        // Find type
        size_t type_pos = json_str.find("\"type\":");
        if (type_pos == std::string::npos) return false;
        
        size_t type_start = json_str.find_first_of("0123456789", type_pos);
        size_t type_end = json_str.find_first_not_of("0123456789", type_start);
        int type_val = std::stoi(json_str.substr(type_start, type_end - type_start));
        message.type = static_cast<MulticastMessageType>(type_val);
        
        // Find symbol
        size_t symbol_pos = json_str.find("\"symbol\":");
        if (symbol_pos != std::string::npos) {
            size_t symbol_start = json_str.find("\"", symbol_pos + 9) + 1;
            size_t symbol_end = json_str.find("\"", symbol_start);
            message.symbol = json_str.substr(symbol_start, symbol_end - symbol_start);
        }
        
        // Find timestamp
        size_t timestamp_pos = json_str.find("\"timestamp\":");
        if (timestamp_pos != std::string::npos) {
            size_t timestamp_start = json_str.find_first_of("0123456789", timestamp_pos);
            size_t timestamp_end = json_str.find_first_not_of("0123456789", timestamp_start);
            message.timestamp = std::stoull(json_str.substr(timestamp_start, timestamp_end - timestamp_start));
        }
        
        // Find data
        size_t data_pos = json_str.find("\"data\":");
        if (data_pos != std::string::npos) {
            size_t data_start = json_str.find("{", data_pos);
            size_t data_end = json_str.find_last_of("}") + 1;
            message.data = json_str.substr(data_start, data_end - data_start);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

void MulticastSubscriber::handle_order_book_update(const std::string& symbol, const std::string& data) {
    if (order_book_callback_) {
        order_book_callback_(symbol, data);
    }
}

void MulticastSubscriber::handle_trade_update(const std::string& symbol, const std::string& data) {
    if (trade_callback_) {
        trade_callback_(symbol, data);
    }
}

void MulticastSubscriber::handle_heartbeat(const std::string& data) {
    if (heartbeat_callback_) {
        heartbeat_callback_(data);
    }
}

void MulticastSubscriber::set_order_book_callback(std::function<void(const std::string&, const std::string&)> callback) {
    order_book_callback_ = callback;
}

void MulticastSubscriber::set_trade_callback(std::function<void(const std::string&, const std::string&)> callback) {
    trade_callback_ = callback;
}

void MulticastSubscriber::set_heartbeat_callback(std::function<void(const std::string&)> callback) {
    heartbeat_callback_ = callback;
}
