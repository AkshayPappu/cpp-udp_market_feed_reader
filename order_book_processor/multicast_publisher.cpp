#include "multicast_publisher.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

MulticastPublisher::MulticastPublisher() 
    : socket_fd_(-1), port_(0), initialized_(false), messages_sent_(0), bytes_sent_(0) {
}

MulticastPublisher::~MulticastPublisher() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
    }
}

bool MulticastPublisher::initialize(const std::string& multicast_group, int port, int ttl) {
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
    
    // Set multicast TTL
    if (setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        std::cerr << "Failed to set multicast TTL" << std::endl;
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
    
    initialized_ = true;
    std::cout << "Multicast publisher initialized: " << multicast_group << ":" << port << std::endl;
    
    return true;
}

void MulticastPublisher::publish_order_book_update(const std::string& symbol, const OrderBook& book, uint64_t timestamp) {
    if (!initialized_) {
        std::cerr << "Multicast publisher not initialized" << std::endl;
        return;
    }
    
    std::string json_data = order_book_to_json(book);
    MulticastMessage message(MulticastMessageType::ORDER_BOOK_UPDATE, symbol, timestamp, json_data);
    
    if (send_message(message)) {
        messages_sent_++;
        bytes_sent_ += json_data.length();
    }
}

void MulticastPublisher::publish_trade_update(const std::string& symbol, double price, uint32_t size, 
                                            OrderSide aggressor_side, uint64_t timestamp) {
    if (!initialized_) {
        std::cerr << "Multicast publisher not initialized" << std::endl;
        return;
    }
    
    std::string json_data = trade_to_json(price, size, aggressor_side);
    MulticastMessage message(MulticastMessageType::TRADE_UPDATE, symbol, timestamp, json_data);
    
    if (send_message(message)) {
        messages_sent_++;
        bytes_sent_ += json_data.length();
    }
}

void MulticastPublisher::publish_heartbeat() {
    if (!initialized_) {
        return;
    }
    
    std::ostringstream json;
    json << "{\"messages_sent\":" << messages_sent_ << ",\"bytes_sent\":" << bytes_sent_ << "}";
    
    MulticastMessage message(MulticastMessageType::HEARTBEAT, "", 
                           std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::steady_clock::now().time_since_epoch()).count(),
                           json.str());
    
    send_message(message);
}

bool MulticastPublisher::send_message(const MulticastMessage& message) {
    if (socket_fd_ < 0) {
        return false;
    }
    
    // Create JSON message
    std::ostringstream json;
    json << "{";
    json << "\"type\":" << static_cast<int>(message.type) << ",";
    json << "\"symbol\":\"" << message.symbol << "\",";
    json << "\"timestamp\":" << message.timestamp << ",";
    json << "\"data\":" << message.data;
    json << "}";
    
    std::string json_str = json.str();
    
    // Send over multicast
    ssize_t bytes_sent = sendto(socket_fd_, json_str.c_str(), json_str.length(), 0,
                               (struct sockaddr*)&multicast_addr_, sizeof(multicast_addr_));
    
    if (bytes_sent < 0) {
        std::cerr << "Failed to send multicast message: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

std::string MulticastPublisher::order_book_to_json(const OrderBook& book) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(6);
    
    // Get best bid/ask
    auto best_bid = book.get_best_bid();
    auto best_ask = book.get_best_ask();
    
    json << "{";
    json << "\"best_bid_price\":" << best_bid.first << ",";
    json << "\"best_bid_size\":" << best_bid.second << ",";
    json << "\"best_ask_price\":" << best_ask.first << ",";
    json << "\"best_ask_size\":" << best_ask.second << ",";
    
    // Calculate spread and midprice
    double spread = 0.0;
    double midprice = 0.0;
    if (best_bid.first > 0 && best_ask.first > 0) {
        spread = best_ask.first - best_bid.first;
        midprice = (best_bid.first + best_ask.first) / 2.0;
    }
    
    json << "\"spread\":" << spread << ",";
    json << "\"midprice\":" << midprice << ",";
    
    // Calculate quote imbalance
    uint32_t total_size = best_bid.second + best_ask.second;
    double quote_imbalance = 0.0;
    if (total_size > 0) {
        quote_imbalance = (static_cast<double>(best_bid.second) - 
                          static_cast<double>(best_ask.second)) / total_size;
    }
    
    json << "\"quote_imbalance\":" << quote_imbalance;
    json << "}";
    
    return json.str();
}

std::string MulticastPublisher::trade_to_json(double price, uint32_t size, OrderSide aggressor_side) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(6);
    
    json << "{";
    json << "\"price\":" << price << ",";
    json << "\"size\":" << size << ",";
    json << "\"aggressor_side\":\"" << (aggressor_side == OrderSide::BID ? "BID" : "ASK") << "\"";
    json << "}";
    
    return json.str();
}
