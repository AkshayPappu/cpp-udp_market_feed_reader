#include "order_book_api.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cstdlib>

// Include mongoose for HTTP server
#include "mongoose.h"

// Static instance for HTTP handler callbacks
OrderBookAPI* OrderBookAPI::instance_ = nullptr;

OrderBookAPI::OrderBookAPI(int port) 
    : port_(port), running_(false), mg_context_(nullptr) {
    instance_ = this;
}

OrderBookAPI::~OrderBookAPI() {
    stop();
    instance_ = nullptr;
}

bool OrderBookAPI::start() {
    if (running_.load()) {
        return true;
    }
    
    // Start HTTP server in a separate thread
    server_thread_ = std::make_unique<std::thread>(&OrderBookAPI::server_thread_function, this);
    
    // Wait a moment for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return running_.load();
}

void OrderBookAPI::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    mg_context_ = nullptr;
}

void OrderBookAPI::server_thread_function() {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    
    std::string port_str = std::to_string(port_);
    std::string listen_addr = "http://0.0.0.0:" + port_str;
    
    mg_connection* conn = mg_http_listen(&mgr, listen_addr.c_str(), http_handler, this);
    
    if (!conn) {
        std::cerr << "Failed to start HTTP server on port " << port_ << std::endl;
        return;
    }
    
    running_.store(true);
    std::cout << "Order Book API server started on port " << port_ << std::endl;
    
    while (running_.load()) {
        mg_mgr_poll(&mgr, 100);
    }
    
    mg_mgr_free(&mgr);
    std::cout << "Order Book API server stopped" << std::endl;
}

void OrderBookAPI::http_handler(struct mg_connection* conn, int ev, void* ev_data, void* fn_data) {
    if (ev != MG_EV_HTTP_MSG) {
        return;
    }
    
    struct mg_http_message* hm = (struct mg_http_message*)ev_data;
    OrderBookAPI* api = static_cast<OrderBookAPI*>(fn_data);
    
    std::string uri(mg_str(hm->uri).ptr, mg_str(hm->uri).len);
    std::string method(mg_str(hm->method).ptr, mg_str(hm->method).len);
    
    // Route requests
    if (method == "GET") {
        if (uri == "/api/symbols") {
            handle_get_symbols(conn, api);
        } else if (uri.find("/api/metrics/") == 0) {
            handle_get_metrics(conn, api);
        } else if (uri.find("/api/depth/") == 0) {
            handle_get_depth(conn, api);
        } else if (uri.find("/api/trades/") == 0) {
            handle_get_trades(conn, api);
        } else if (uri == "/api/health") {
            handle_get_health(conn, api);
        } else {
            mg_http_reply(conn, 404, "Content-Type: application/json\r\n", 
                         "{\"error\": \"Not found\"}");
        }
    } else {
        mg_http_reply(conn, 405, "Content-Type: application/json\r\n", 
                     "{\"error\": \"Method not allowed\"}");
    }
}

void OrderBookAPI::handle_get_symbols(struct mg_connection* conn, void* user_data) {
    OrderBookAPI* api = static_cast<OrderBookAPI*>(user_data);
    
    std::lock_guard<std::mutex> lock(api->data_mutex_);
    
    std::ostringstream json;
    json << "{\"symbols\": [";
    
    bool first = true;
    for (const auto& pair : api->symbol_metrics_) {
        if (!first) json << ",";
        json << "\"" << pair.first << "\"";
        first = false;
    }
    
    json << "]}";
    
    mg_http_reply(conn, 200, "Content-Type: application/json\r\n", "%s", json.str().c_str());
}

void OrderBookAPI::handle_get_metrics(struct mg_connection* conn, void* user_data) {
    OrderBookAPI* api = static_cast<OrderBookAPI*>(user_data);
    
    // Extract symbol from URI (e.g., /api/metrics/AAPL)
    // We need to get the URI from the connection context
    struct mg_http_message* hm = (struct mg_http_message*)conn->data;
    std::string uri(mg_str(hm->uri).ptr, mg_str(hm->uri).len);
    size_t pos = uri.find_last_of('/');
    if (pos == std::string::npos) {
        mg_http_reply(conn, 400, "Content-Type: application/json\r\n", 
                     "{\"error\": \"Invalid symbol\"}");
        return;
    }
    
    std::string symbol = uri.substr(pos + 1);
    
    MarketMetrics metrics = api->get_metrics(symbol);
    std::string json = metrics_to_json(metrics);
    
    mg_http_reply(conn, 200, "Content-Type: application/json\r\n", "%s", json.c_str());
}

void OrderBookAPI::handle_get_depth(struct mg_connection* conn, void* user_data) {
    OrderBookAPI* api = static_cast<OrderBookAPI*>(user_data);
    
    // Extract symbol from URI (e.g., /api/depth/AAPL)
    std::string uri = conn->uri;
    size_t pos = uri.find_last_of('/');
    if (pos == std::string::npos) {
        mg_http_reply(conn, 400, "Content-Type: application/json\r\n", 
                     "{\"error\": \"Invalid symbol\"}");
        return;
    }
    
    std::string symbol = uri.substr(pos + 1);
    
    MarketMetrics metrics = api->get_metrics(symbol);
    
    std::ostringstream json;
    json << "{\"symbol\": \"" << symbol << "\",";
    json << "\"bid_depth\": " << depth_to_json(metrics.bid_depth, "bid") << ",";
    json << "\"ask_depth\": " << depth_to_json(metrics.ask_depth, "ask") << "}";
    
    mg_http_reply(conn, 200, "Content-Type: application/json\r\n", "%s", json.str().c_str());
}

void OrderBookAPI::handle_get_trades(struct mg_connection* conn, void* user_data) {
    OrderBookAPI* api = static_cast<OrderBookAPI*>(user_data);
    
    // Extract symbol from URI (e.g., /api/trades/AAPL)
    std::string uri = conn->uri;
    size_t pos = uri.find_last_of('/');
    if (pos == std::string::npos) {
        mg_http_reply(conn, 400, "Content-Type: application/json\r\n", 
                     "{\"error\": \"Invalid symbol\"}");
        return;
    }
    
    std::string symbol = uri.substr(pos + 1);
    
    MarketMetrics metrics = api->get_metrics(symbol);
    std::string json = trade_to_json(metrics.last_trade);
    
    mg_http_reply(conn, 200, "Content-Type: application/json\r\n", "%s", json.c_str());
}

void OrderBookAPI::handle_get_health(struct mg_connection* conn, void* user_data) {
    OrderBookAPI* api = static_cast<OrderBookAPI*>(user_data);
    
    std::ostringstream json;
    json << "{\"status\": \"healthy\",";
    json << "\"running\": " << (api->is_running() ? "true" : "false") << ",";
    json << "\"port\": " << api->port_ << ",";
    json << "\"symbols_count\": " << api->get_available_symbols().size() << "}";
    
    mg_http_reply(conn, 200, "Content-Type: application/json\r\n", "%s", json.str().c_str());
}

std::string OrderBookAPI::metrics_to_json(const MarketMetrics& metrics) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(6);
    
    json << "{";
    json << "\"best_bid_price\": " << metrics.best_bid_price << ",";
    json << "\"best_bid_size\": " << metrics.best_bid_size << ",";
    json << "\"best_ask_price\": " << metrics.best_ask_price << ",";
    json << "\"best_ask_size\": " << metrics.best_ask_size << ",";
    json << "\"spread\": " << metrics.spread << ",";
    json << "\"midprice\": " << metrics.midprice << ",";
    json << "\"quote_imbalance\": " << metrics.quote_imbalance << ",";
    json << "\"last_update_timestamp\": " << metrics.last_update_timestamp << ",";
    json << "\"total_events_processed\": " << metrics.total_events_processed;
    json << "}";
    
    return json.str();
}

std::string OrderBookAPI::depth_to_json(const std::vector<DepthLevel>& depth, const std::string& side) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(6);
    
    json << "[";
    for (size_t i = 0; i < depth.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"price\": " << depth[i].price << ", \"size\": " << depth[i].size << "}";
    }
    json << "]";
    
    return json.str();
}

std::string OrderBookAPI::trade_to_json(const TradeInfo& trade) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(6);
    
    json << "{";
    json << "\"price\": " << trade.price << ",";
    json << "\"size\": " << trade.size << ",";
    json << "\"aggressor_side\": \"" << (trade.aggressor_side == OrderSide::BID ? "BID" : "ASK") << "\",";
    json << "\"timestamp\": " << trade.timestamp;
    json << "}";
    
    return json.str();
}

void OrderBookAPI::update_order_book(const std::string& symbol, const OrderBook& book) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    MarketMetrics metrics = calculate_metrics(book);
    metrics.last_update_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    symbol_metrics_[symbol] = metrics;
}

void OrderBookAPI::update_trade(const std::string& symbol, double price, uint32_t size, 
                               OrderSide aggressor_side, uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (symbol_metrics_.find(symbol) != symbol_metrics_.end()) {
        symbol_metrics_[symbol].last_trade = TradeInfo(price, size, aggressor_side, timestamp);
    }
}

void OrderBookAPI::increment_event_count(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (symbol_metrics_.find(symbol) != symbol_metrics_.end()) {
        symbol_metrics_[symbol].total_events_processed++;
    }
}

MarketMetrics OrderBookAPI::get_metrics(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto it = symbol_metrics_.find(symbol);
    if (it != symbol_metrics_.end()) {
        return it->second;
    }
    
    return MarketMetrics(); // Return empty metrics if symbol not found
}

std::vector<std::string> OrderBookAPI::get_available_symbols() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<std::string> symbols;
    for (const auto& pair : symbol_metrics_) {
        symbols.push_back(pair.first);
    }
    
    return symbols;
}

MarketMetrics OrderBookAPI::calculate_metrics(const OrderBook& book) const {
    MarketMetrics metrics;
    
    // Get best bid/ask
    auto best_bid = book.get_best_bid();
    auto best_ask = book.get_best_ask();
    
    metrics.best_bid_price = best_bid.first;
    metrics.best_bid_size = best_bid.second;
    metrics.best_ask_price = best_ask.first;
    metrics.best_ask_size = best_ask.second;
    
    // Calculate spread and midprice
    if (metrics.best_bid_price > 0 && metrics.best_ask_price > 0) {
        metrics.spread = metrics.best_ask_price - metrics.best_bid_price;
        metrics.midprice = (metrics.best_bid_price + metrics.best_ask_price) / 2.0;
    }
    
    // Calculate quote imbalance
    uint32_t total_size = metrics.best_bid_size + metrics.best_ask_size;
    if (total_size > 0) {
        metrics.quote_imbalance = (static_cast<double>(metrics.best_bid_size) - 
                                  static_cast<double>(metrics.best_ask_size)) / total_size;
    }
    
    // Get depth snapshot (top N levels)
    // Note: This is a simplified implementation. In a real system, you'd want
    // to maintain depth levels in the OrderBook class itself.
    metrics.bid_depth.clear();
    metrics.ask_depth.clear();
    
    // For now, just add the best bid/ask as depth levels
    if (metrics.best_bid_price > 0) {
        metrics.bid_depth.push_back(DepthLevel(metrics.best_bid_price, metrics.best_bid_size));
    }
    if (metrics.best_ask_price > 0) {
        metrics.ask_depth.push_back(DepthLevel(metrics.best_ask_price, metrics.best_ask_size));
    }
    
    return metrics;
}
