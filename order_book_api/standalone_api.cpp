// Standalone API Server with Multicast Subscriber
// This runs independently from the order book processor

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <signal.h>
#include <map>
#include <sstream>
#include <iomanip>

#include "simple_api.hpp"
#include "multicast_subscriber.hpp"

// Global flag for graceful shutdown
std::atomic<bool> shutdown_flag{false};

// Global API instance
std::unique_ptr<SimpleOrderBookAPI> api;
std::unique_ptr<MulticastSubscriber> subscriber;

// Signal handler for graceful shutdown
void signal_handler(int /* signal */) {
    std::cout << "\nReceived shutdown signal, shutting down gracefully..." << std::endl;
    shutdown_flag.store(true);
}

// Parse JSON data and update API
void update_api_from_json(const std::string& symbol, const std::string& json_data) {
    if (!api) return;
    
    try {
        // Simple JSON parsing for order book data
        // In production, use a proper JSON library like nlohmann/json
        
        // Extract best bid/ask from JSON
        double best_bid_price = 0.0, best_ask_price = 0.0;
        uint32_t best_bid_size = 0, best_ask_size = 0;
        
        // Find best_bid_price
        size_t pos = json_data.find("\"best_bid_price\":");
        if (pos != std::string::npos) {
            size_t start = json_data.find_first_of("0123456789.", pos);
            size_t end = json_data.find_first_not_of("0123456789.", start);
            best_bid_price = std::stod(json_data.substr(start, end - start));
        }
        
        // Find best_bid_size
        pos = json_data.find("\"best_bid_size\":");
        if (pos != std::string::npos) {
            size_t start = json_data.find_first_of("0123456789", pos);
            size_t end = json_data.find_first_not_of("0123456789", start);
            best_bid_size = std::stoul(json_data.substr(start, end - start));
        }
        
        // Find best_ask_price
        pos = json_data.find("\"best_ask_price\":");
        if (pos != std::string::npos) {
            size_t start = json_data.find_first_of("0123456789.", pos);
            size_t end = json_data.find_first_not_of("0123456789.", start);
            best_ask_price = std::stod(json_data.substr(start, end - start));
        }
        
        // Find best_ask_size
        pos = json_data.find("\"best_ask_size\":");
        if (pos != std::string::npos) {
            size_t start = json_data.find_first_of("0123456789", pos);
            size_t end = json_data.find_first_not_of("0123456789", start);
            best_ask_size = std::stoul(json_data.substr(start, end - start));
        }
        
        // Create a simple order book with just the best bid/ask
        OrderBook book;
        if (best_bid_price > 0 && best_bid_size > 0) {
            book.add_order(OrderSide::BID, best_bid_price, best_bid_size);
        }
        if (best_ask_price > 0 && best_ask_size > 0) {
            book.add_order(OrderSide::ASK, best_ask_price, best_ask_size);
        }
        
        // Update API
        api->update_order_book(symbol, book);
        api->increment_event_count(symbol);
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing order book JSON: " << e.what() << std::endl;
    }
}

// Parse trade JSON and update API
void update_trade_from_json(const std::string& symbol, const std::string& json_data) {
    if (!api) return;
    
    try {
        // Extract trade data from JSON
        double price = 0.0;
        uint32_t size = 0;
        OrderSide aggressor_side = OrderSide::UNKNOWN;
        
        // Find price
        size_t pos = json_data.find("\"price\":");
        if (pos != std::string::npos) {
            size_t start = json_data.find_first_of("0123456789.", pos);
            size_t end = json_data.find_first_not_of("0123456789.", start);
            price = std::stod(json_data.substr(start, end - start));
        }
        
        // Find size
        pos = json_data.find("\"size\":");
        if (pos != std::string::npos) {
            size_t start = json_data.find_first_of("0123456789", pos);
            size_t end = json_data.find_first_not_of("0123456789", start);
            size = std::stoul(json_data.substr(start, end - start));
        }
        
        // Find aggressor_side
        pos = json_data.find("\"aggressor_side\":");
        if (pos != std::string::npos) {
            size_t start = json_data.find("\"", pos + 16) + 1;
            size_t end = json_data.find("\"", start);
            std::string side_str = json_data.substr(start, end - start);
            aggressor_side = (side_str == "BID") ? OrderSide::BID : OrderSide::ASK;
        }
        
        // Update API
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        api->update_trade(symbol, price, size, aggressor_side, timestamp);
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing trade JSON: " << e.what() << std::endl;
    }
}

// Handle heartbeat messages
void handle_heartbeat(const std::string& /* data */) {
    // Optional: Log heartbeat or update statistics
    static int heartbeat_count = 0;
    if (++heartbeat_count % 100 == 0) {
        std::cout << "Received " << heartbeat_count << " heartbeats" << std::endl;
    }
}

int main(int /* argc */, char* /* argv */[]) {
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Configuration
    const int api_port = 8080;
    const std::string multicast_group = "224.0.0.1";
    const int multicast_port = 12346;
    
    std::cout << "=== Standalone Order Book API Server ===" << std::endl;
    std::cout << "API Port: " << api_port << std::endl;
    std::cout << "Multicast Group: " << multicast_group << ":" << multicast_port << std::endl;
    std::cout << std::endl;
    
    try {
        // Initialize API
        api = std::make_unique<SimpleOrderBookAPI>(api_port);
        if (!api->start()) {
            std::cerr << "Failed to start API server" << std::endl;
            return 1;
        }
        
        // Initialize multicast subscriber
        subscriber = std::make_unique<MulticastSubscriber>();
        if (!subscriber->initialize(multicast_group, multicast_port)) {
            std::cerr << "Failed to initialize multicast subscriber" << std::endl;
            return 1;
        }
        
        // Set up callbacks
        subscriber->set_order_book_callback(update_api_from_json);
        subscriber->set_trade_callback(update_trade_from_json);
        subscriber->set_heartbeat_callback(handle_heartbeat);
        
        // Start listening for multicast messages
        if (!subscriber->start_listening()) {
            std::cerr << "Failed to start multicast listener" << std::endl;
            return 1;
        }
        
        std::cout << "System running. Press Ctrl+C to stop." << std::endl;
        std::cout << "API available at: http://localhost:" << api_port << std::endl;
        std::cout << "Try: curl http://localhost:" << api_port << "/api/health" << std::endl;
        std::cout << std::endl;
        
        // Wait for shutdown signal
        while (!shutdown_flag.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Print statistics every 10 seconds
            static auto last_stats_time = std::chrono::steady_clock::now();
            if (std::chrono::steady_clock::now() - last_stats_time > std::chrono::seconds(10)) {
                std::cout << "Stats - Messages: " << subscriber->get_messages_received() 
                         << ", Bytes: " << subscriber->get_bytes_received()
                         << ", Errors: " << subscriber->get_parse_errors() << std::endl;
                last_stats_time = std::chrono::steady_clock::now();
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    }
    
    // Shutdown
    std::cout << "Shutting down..." << std::endl;
    
    if (subscriber) {
        subscriber->stop_listening();
    }
    
    if (api) {
        api->stop();
    }
    
    std::cout << "Shutdown complete." << std::endl;
    return 0;
}
