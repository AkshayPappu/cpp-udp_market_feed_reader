#include "simple_api.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cstdlib>

SimpleOrderBookAPI::SimpleOrderBookAPI(int port) 
    : port_(port), server_socket_(-1), running_(false) {
}

SimpleOrderBookAPI::~SimpleOrderBookAPI() {
    stop();
}

bool SimpleOrderBookAPI::start() {
    if (running_.load()) {
        return true;
    }
    
    // Create socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(server_socket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind socket to port " << port_ << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Listen for connections
    if (listen(server_socket_, 10) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(server_socket_);
        return false;
    }
    
    // Start server thread
    server_thread_ = std::make_unique<std::thread>(&SimpleOrderBookAPI::server_thread_function, this);
    
    // Wait a moment for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return running_.load();
}

void SimpleOrderBookAPI::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
    
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
}

void SimpleOrderBookAPI::server_thread_function() {
    running_.store(true);
    std::cout << "Simple Order Book API server started on port " << port_ << std::endl;
    
    while (running_.load()) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_address, &client_len);
        if (client_socket < 0) {
            if (running_.load()) {
                std::cerr << "Failed to accept client connection" << std::endl;
            }
            continue;
        }
        
        // Handle client in a separate thread
        std::thread client_thread(&SimpleOrderBookAPI::handle_client, this, client_socket);
        client_thread.detach();
    }
    
    std::cout << "Simple Order Book API server stopped" << std::endl;
}

void SimpleOrderBookAPI::handle_client(int client_socket) {
    char buffer[4096];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes_received] = '\0';
    std::string request(buffer);
    
    // Parse HTTP request
    std::string method = parse_method(request);
    std::string uri = parse_uri(request);
    
    std::string response;
    
    // Route requests
    if (method == "GET") {
        if (uri == "/api/symbols") {
            response = handle_get_symbols();
        } else if (uri.find("/api/metrics/") == 0) {
            std::string symbol = uri.substr(13); // Remove "/api/metrics/"
            response = handle_get_metrics(symbol);
        } else if (uri.find("/api/depth/") == 0) {
            std::string symbol = uri.substr(11); // Remove "/api/depth/"
            response = handle_get_depth(symbol);
        } else if (uri.find("/api/trades/") == 0) {
            std::string symbol = uri.substr(12); // Remove "/api/trades/"
            response = handle_get_trades(symbol);
        } else if (uri == "/api/health") {
            response = handle_get_health();
        } else {
            response = create_http_response("{\"error\": \"Not found\"}", 404);
        }
    } else {
        response = create_http_response("{\"error\": \"Method not allowed\"}", 405);
    }
    
    // Send response
    send(client_socket, response.c_str(), response.length(), 0);
    close(client_socket);
}

std::string SimpleOrderBookAPI::parse_uri(const std::string& request) {
    size_t start = request.find(' ');
    if (start == std::string::npos) return "";
    
    size_t end = request.find(' ', start + 1);
    if (end == std::string::npos) return "";
    
    return request.substr(start + 1, end - start - 1);
}

std::string SimpleOrderBookAPI::parse_method(const std::string& request) {
    size_t end = request.find(' ');
    if (end == std::string::npos) return "";
    
    return request.substr(0, end);
}

std::string SimpleOrderBookAPI::handle_get_symbols() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::ostringstream json;
    json << "{\"symbols\": [";
    
    bool first = true;
    for (const auto& pair : symbol_metrics_) {
        if (!first) json << ",";
        json << "\"" << pair.first << "\"";
        first = false;
    }
    
    json << "]}";
    
    return create_http_response(json.str());
}

std::string SimpleOrderBookAPI::handle_get_metrics(const std::string& symbol) {
    MarketMetrics metrics = get_metrics(symbol);
    std::string json = metrics_to_json(metrics);
    return create_http_response(json);
}

std::string SimpleOrderBookAPI::handle_get_depth(const std::string& symbol) {
    MarketMetrics metrics = get_metrics(symbol);
    
    std::ostringstream json;
    json << "{\"symbol\": \"" << symbol << "\",";
    json << "\"bid_depth\": " << depth_to_json(metrics.bid_depth, "bid") << ",";
    json << "\"ask_depth\": " << depth_to_json(metrics.ask_depth, "ask") << "}";
    
    return create_http_response(json.str());
}

std::string SimpleOrderBookAPI::handle_get_trades(const std::string& symbol) {
    MarketMetrics metrics = get_metrics(symbol);
    std::string json = trade_to_json(metrics.last_trade);
    return create_http_response(json);
}

std::string SimpleOrderBookAPI::handle_get_health() {
    std::ostringstream json;
    json << "{\"status\": \"healthy\",";
    json << "\"running\": " << (is_running() ? "true" : "false") << ",";
    json << "\"port\": " << port_ << ",";
    json << "\"symbols_count\": " << get_available_symbols().size() << "}";
    
    return create_http_response(json.str());
}

std::string SimpleOrderBookAPI::create_http_response(const std::string& body, int status_code) {
    std::ostringstream response;
    
    if (status_code == 200) {
        response << "HTTP/1.1 200 OK\r\n";
    } else if (status_code == 404) {
        response << "HTTP/1.1 404 Not Found\r\n";
    } else if (status_code == 405) {
        response << "HTTP/1.1 405 Method Not Allowed\r\n";
    } else {
        response << "HTTP/1.1 400 Bad Request\r\n";
    }
    
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    
    return response.str();
}

std::string SimpleOrderBookAPI::metrics_to_json(const MarketMetrics& metrics) {
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

std::string SimpleOrderBookAPI::depth_to_json(const std::vector<DepthLevel>& depth, const std::string& /* side */) {
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

std::string SimpleOrderBookAPI::trade_to_json(const TradeInfo& trade) {
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

void SimpleOrderBookAPI::update_order_book(const std::string& symbol, const OrderBook& book) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    MarketMetrics metrics = calculate_metrics(book);
    metrics.last_update_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Preserve existing event count if symbol already exists
    auto it = symbol_metrics_.find(symbol);
    if (it != symbol_metrics_.end()) {
        metrics.total_events_processed = it->second.total_events_processed;
        metrics.last_trade = it->second.last_trade; // Preserve trade info too
    }
    
    symbol_metrics_[symbol] = metrics;
}

void SimpleOrderBookAPI::update_trade(const std::string& symbol, double price, uint32_t size, 
                                     OrderSide aggressor_side, uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (symbol_metrics_.find(symbol) != symbol_metrics_.end()) {
        symbol_metrics_[symbol].last_trade = TradeInfo(price, size, aggressor_side, timestamp);
    }
}

void SimpleOrderBookAPI::increment_event_count(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (symbol_metrics_.find(symbol) != symbol_metrics_.end()) {
        symbol_metrics_[symbol].total_events_processed++;
    }
}

MarketMetrics SimpleOrderBookAPI::get_metrics(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto it = symbol_metrics_.find(symbol);
    if (it != symbol_metrics_.end()) {
        return it->second;
    }
    
    return MarketMetrics(); // Return empty metrics if symbol not found
}

std::vector<std::string> SimpleOrderBookAPI::get_available_symbols() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<std::string> symbols;
    for (const auto& pair : symbol_metrics_) {
        symbols.push_back(pair.first);
    }
    
    return symbols;
}

MarketMetrics SimpleOrderBookAPI::calculate_metrics(const OrderBook& book) const {
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

// Test main function
// Test main function removed to avoid multiple main definitions
