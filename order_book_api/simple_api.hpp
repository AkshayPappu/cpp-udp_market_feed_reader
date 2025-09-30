#ifndef SIMPLE_API_HPP
#define SIMPLE_API_HPP

#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <chrono>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "orderbook.hpp"

// Trade information for aggressor side tracking
struct TradeInfo {
    double price = 0.0;
    uint32_t size = 0;
    OrderSide aggressor_side = OrderSide::UNKNOWN;
    uint64_t timestamp = 0;
    
    TradeInfo() = default;
    TradeInfo(double p, uint32_t s, OrderSide side, uint64_t ts)
        : price(p), size(s), aggressor_side(side), timestamp(ts) {}
};

// Depth level for book snapshots
struct DepthLevel {
    double price = 0.0;
    uint32_t size = 0;
    
    DepthLevel() = default;
    DepthLevel(double p, uint32_t s) : price(p), size(s) {}
};

// Market metrics for a symbol
struct MarketMetrics {
    // Tier 1: Must-Have (MVP)
    double best_bid_price = 0.0;
    uint32_t best_bid_size = 0;
    double best_ask_price = 0.0;
    uint32_t best_ask_size = 0;
    double spread = 0.0;
    double midprice = 0.0;
    double quote_imbalance = 0.0;
    
    // Depth snapshot (top N levels)
    std::vector<DepthLevel> bid_depth;
    std::vector<DepthLevel> ask_depth;
    
    // Trade information
    TradeInfo last_trade;
    
    // Metadata
    uint64_t last_update_timestamp = 0;
    uint64_t total_events_processed = 0;
    
    MarketMetrics() = default;
};

// Simple HTTP API Server
class SimpleOrderBookAPI {
public:
    SimpleOrderBookAPI(int port = 8080);
    ~SimpleOrderBookAPI();
    
    // Start/stop the API server
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Update order book data (called by the main strategy)
    void update_order_book(const std::string& symbol, const OrderBook& book);
    void update_trade(const std::string& symbol, double price, uint32_t size, 
                     OrderSide aggressor_side, uint64_t timestamp);
    void increment_event_count(const std::string& symbol);
    
    // Get current metrics for a symbol
    MarketMetrics get_metrics(const std::string& symbol) const;
    std::vector<std::string> get_available_symbols() const;
    
    // Configuration
    void set_depth_levels(int levels) { depth_levels_ = levels; }
    int get_depth_levels() const { return depth_levels_; }
    
private:
    // HTTP server management
    void server_thread_function();
    void handle_client(int client_socket);
    
    // HTTP request parsing
    std::string parse_uri(const std::string& request);
    std::string parse_method(const std::string& request);
    
    // API endpoint handlers
    std::string handle_get_symbols();
    std::string handle_get_metrics(const std::string& symbol);
    std::string handle_get_depth(const std::string& symbol);
    std::string handle_get_trades(const std::string& symbol);
    std::string handle_get_health();
    
    // JSON response helpers
    std::string metrics_to_json(const MarketMetrics& metrics);
    std::string depth_to_json(const std::vector<DepthLevel>& depth, const std::string& side);
    std::string trade_to_json(const TradeInfo& trade);
    std::string error_response(const std::string& message, int code = 400);
    std::string success_response(const std::string& data);
    
    // Calculate metrics from order book
    MarketMetrics calculate_metrics(const OrderBook& book) const;
    
    // HTTP response helpers
    std::string create_http_response(const std::string& body, int status_code = 200);
    
    // Member variables
    int port_;
    int server_socket_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> server_thread_;
    
    // Data storage
    mutable std::mutex data_mutex_;
    std::map<std::string, MarketMetrics> symbol_metrics_;
    int depth_levels_ = 5;  // Default to top 5 levels
};

#endif // SIMPLE_API_HPP
