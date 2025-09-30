#ifndef ORDER_BOOK_API_HPP
#define ORDER_BOOK_API_HPP

#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <chrono>
#include "orderbook.hpp"

// Forward declaration for HTTP server
struct mg_context;
struct mg_connection;

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

// Order Book API Server
class OrderBookAPI {
public:
    OrderBookAPI(int port = 8080);
    ~OrderBookAPI();
    
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
    static void http_handler(struct mg_connection* conn, int ev, void* ev_data, void* fn_data);
    
    // API endpoint handlers
    static void handle_get_symbols(struct mg_connection* conn, void* user_data);
    static void handle_get_metrics(struct mg_connection* conn, void* user_data);
    static void handle_get_depth(struct mg_connection* conn, void* user_data);
    static void handle_get_trades(struct mg_connection* conn, void* user_data);
    static void handle_get_health(struct mg_connection* conn, void* user_data);
    
    // JSON response helpers
    static std::string metrics_to_json(const MarketMetrics& metrics);
    static std::string depth_to_json(const std::vector<DepthLevel>& depth, const std::string& side);
    static std::string trade_to_json(const TradeInfo& trade);
    static std::string error_response(const std::string& message, int code = 400);
    static std::string success_response(const std::string& data);
    
    // Calculate metrics from order book
    MarketMetrics calculate_metrics(const OrderBook& book) const;
    
    // Member variables
    int port_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> server_thread_;
    struct mg_context* mg_context_;
    
    // Data storage
    mutable std::mutex data_mutex_;
    std::map<std::string, MarketMetrics> symbol_metrics_;
    int depth_levels_ = 5;  // Default to top 5 levels
    
    // HTTP server context
    static OrderBookAPI* instance_;
};

#endif // ORDER_BOOK_API_HPP
