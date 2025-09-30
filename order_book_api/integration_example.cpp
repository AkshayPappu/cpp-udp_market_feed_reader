// Example integration of Order Book API with the main order book processor
// This shows how to modify main.cpp to include the API

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <signal.h>

#include "queue.hpp"
#include "listener.hpp"
#include "quote.hpp"
#include "orderbook.hpp"
#include "simple_api.hpp"

// Global flag for graceful shutdown
std::atomic<bool> shutdown_flag{false};

// Global order books for each symbol
std::map<std::string, OrderBook> order_books;

// Global API instance
std::unique_ptr<SimpleOrderBookAPI> api;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "\nReceived shutdown signal, shutting down gracefully..." << std::endl;
    shutdown_flag.store(true);
}

// Producer thread function
void ingress_producer(SPSCRingBuffer<OrderBookEvent>& queue, UDPListener& listener) {
    std::cout << "Starting UDP listener on port 12345..." << std::endl;
    
    listener.set_order_book_callback([&queue](const OrderBookEvent& event) {
        // Add timestamp for enqueueing
        OrderBookEvent e = event;
        e.enqueued_mono_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // Push to queue
        if (!queue.try_push(e)) {
            std::cerr << "Queue full, dropping event" << std::endl;
        }
    });
    
    listener.listen();
}

// Consumer thread function
void print_consumer(SPSCRingBuffer<OrderBookEvent>& queue) {
    std::cout << "Starting order book processor..." << std::endl;
    
    while (!shutdown_flag.load()) {
        OrderBookEvent event;
        if (queue.try_pop(event)) {
            // Calculate latency
            uint64_t deq_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            uint64_t exchange_to_udp = (event.udp_rx_mono_ns > 0 && event.exchange_mono_ns > 0)
                ? event.udp_rx_mono_ns - event.exchange_mono_ns : 0;
            
            uint64_t queue_to_strategy = (deq_ns >= event.enqueued_mono_ns)
                ? deq_ns - event.enqueued_mono_ns : 0;
            
            uint64_t total_latency = exchange_to_udp + queue_to_strategy;
            
            // Process the event and update order book
            OrderBook& book = order_books[event.symbol];
            
            switch (event.event_type) {
                case OrderBookEventType::ADD_ORDER:
                    book.add_order(event.side, event.price, event.size);
                    break;
                case OrderBookEventType::MODIFY_ORDER:
                    book.modify_order(event.side, event.price, event.size);
                    break;
                case OrderBookEventType::CANCEL_ORDER:
                case OrderBookEventType::DELETE_ORDER:
                    book.cancel_order(event.side, event.price, event.size);
                    break;
                case OrderBookEventType::TRADE:
                    // Update API with trade information
                    if (api) {
                        api->update_trade(event.symbol, event.trade_price, event.trade_size, 
                                        event.is_aggressor ? OrderSide::BID : OrderSide::ASK, 
                                        event.timestamp);
                    }
                    break;
                default:
                    break;
            }
            
            // Update API with current order book state
            if (api) {
                api->update_order_book(event.symbol, book);
                api->increment_event_count(event.symbol);
            }
            
            // Print event details (optional, for debugging)
            if (event.event_type == OrderBookEventType::TRADE) {
                std::cout << "TRADE: " << event.symbol << " " << event.trade_price 
                         << "x" << event.trade_size << " (aggressor: " 
                         << (event.is_aggressor ? "Yes" : "No") << ")" << std::endl;
            }
            
            // Print performance stats every 100 events
            static int event_count = 0;
            if (++event_count % 100 == 0) {
                std::cout << "Processed " << event_count << " events. "
                         << "Avg latency: " << (total_latency / 1000) << "μs" << std::endl;
            }
        } else {
            std::this_thread::yield();
        }
    }
    
    std::cout << "Order book processor shutting down..." << std::endl;
}

int main() {
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Configuration
    const size_t queue_capacity = 1024;
    const uint16_t udp_port = 12345;
    const int api_port = 8080;
    
    std::cout << "=== Order Book Processor with API ===" << std::endl;
    std::cout << "UDP Port: " << udp_port << std::endl;
    std::cout << "API Port: " << api_port << std::endl;
    std::cout << "Queue Capacity: " << queue_capacity << std::endl;
    std::cout << std::endl;
    
    // Initialize API
    api = std::make_unique<SimpleOrderBookAPI>(api_port);
    if (!api->start()) {
        std::cerr << "Failed to start API server" << std::endl;
        return 1;
    }
    
    // Initialize components
    SPSCRingBuffer<OrderBookEvent> event_queue(queue_capacity);
    UDPListener listener(udp_port);
    
    // Start threads
    std::thread producer_thread(ingress_producer, std::ref(event_queue), std::ref(listener));
    std::thread consumer_thread(print_consumer, std::ref(event_queue));
    
    std::cout << "System running. Press Ctrl+C to stop." << std::endl;
    std::cout << "API available at: http://localhost:" << api_port << std::endl;
    std::cout << "Try: curl http://localhost:" << api_port << "/api/health" << std::endl;
    std::cout << std::endl;
    
    // Wait for shutdown signal
    while (!shutdown_flag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Shutdown
    std::cout << "Shutting down..." << std::endl;
    
    // Wait for threads to finish
    if (producer_thread.joinable()) {
        producer_thread.join();
    }
    if (consumer_thread.joinable()) {
        consumer_thread.join();
    }
    
    // Stop API
    api->stop();
    
    std::cout << "Shutdown complete." << std::endl;
    return 0;
}
