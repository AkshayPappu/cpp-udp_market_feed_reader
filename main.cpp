#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <signal.h>

#include "queue.hpp"
#include "listener.hpp"
#include "quote.hpp"

// Global flag for graceful shutdown
std::atomic<bool> shutdown_flag{false};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal, shutting down gracefully..." << std::endl;
        shutdown_flag.store(true);
    }
}

// Producer function - runs in main thread
void ingress_producer(SPSCRingBuffer<Quote>& queue, UDPListener& listener) {
    std::cout << "Starting ingress producer..." << std::endl;
    
    // TODO: Implement producer logic
    // - Set up quote callback to push to queue
    // - Start listening for UDP messages
    // - Handle incoming quotes and push to ring buffer
    
    // Producer statistics (atomic counters, no printing)
    static std::atomic<uint64_t> quotes_pushed{0};
    static std::atomic<uint64_t> total_push_latency{0};
    static std::atomic<uint64_t> quotes_dropped{0};
    
    // Set callback to push quotes to queue
    listener.set_quote_callback([&queue](const Quote& quote) {
        // Monotonic timestamp for enqueue event
        auto enq_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // Prepare a copy to store enqueue timestamp (SPSC copies anyway)
        Quote q = quote;
        q.enqueued_mono_ns = enq_ns;
        
        // Try to push quote to ring buffer
        if (queue.try_push(q)) {
            // Successfully pushed quote to queue - just update counters
            // No printing from producer thread (one writer rule)
            uint64_t udp_to_queue = (q.udp_rx_mono_ns > 0 && enq_ns >= q.udp_rx_mono_ns)
                ? (enq_ns - q.udp_rx_mono_ns) : 0ULL;
            quotes_pushed.fetch_add(1, std::memory_order_relaxed);
            total_push_latency.fetch_add(udp_to_queue, std::memory_order_relaxed);
        } else {
            // Queue is full - just update error counter
            // No printing from producer thread
            quotes_dropped.fetch_add(1, std::memory_order_relaxed);
        }
    });
    
    // Start listening
    listener.listen();
}

// Consumer function - runs in separate thread
void print_consumer(SPSCRingBuffer<Quote>& queue) {
    std::cout << "Starting print consumer..." << std::endl;
    
    // TODO: Implement consumer logic
    // - Continuously read from queue
    // - Print quote information
    // - Handle queue empty scenarios
    // - Add any necessary processing
    
    while (!shutdown_flag.load()) {
        Quote quote;
        
        // TODO: Implement quote consumption logic
        // - Try to pop quote from queue
        // - Handle queue empty scenarios
        // - Print quote information
        // - Add appropriate sleep/yield logic
        
        // Try to pop quote from queue
        if (queue.try_pop(quote)) {
            // Get current monotonic timestamp when strategy thread sees the quote
            auto deq_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            // Calculate latency metrics (monotonic, single epoch)
            uint64_t exch_to_udp = (quote.exchange_mono_ns > 0 && quote.udp_rx_mono_ns >= quote.exchange_mono_ns)
                ? (quote.udp_rx_mono_ns - quote.exchange_mono_ns) : 0ULL;
            uint64_t udp_to_queue = (quote.enqueued_mono_ns >= quote.udp_rx_mono_ns)
                ? (quote.enqueued_mono_ns - quote.udp_rx_mono_ns) : 0ULL;
            uint64_t queue_to_strategy = (deq_ns >= quote.enqueued_mono_ns)
                ? (deq_ns - quote.enqueued_mono_ns) : 0ULL;
            uint64_t total_latency = exch_to_udp + udp_to_queue + queue_to_strategy;
            
            // Update statistics counters
            static uint64_t total_quotes = 0;
            static uint64_t total_exchange_latency = 0;
            static uint64_t total_queue_latency = 0;
            static uint64_t total_strategy_latency = 0;
            static uint64_t total_end_to_end = 0;
            
            total_quotes++;
            total_exchange_latency += exch_to_udp;
            total_queue_latency += udp_to_queue;
            total_strategy_latency += queue_to_strategy;
            total_end_to_end += total_latency;
            
            // Successfully received quote - print detailed information with latency
            std::cout << "\n=== QUOTE RECEIVED ===" << std::endl;
            std::cout << "Symbol: " << quote.symbol << std::endl;
            std::cout << "Bid: " << quote.bid_price << " x " << quote.bid_size << std::endl;
            std::cout << "Ask: " << quote.ask_price << " x " << quote.ask_size << std::endl;
            std::cout << "Exchange: " << quote.exchange << std::endl;
            std::cout << "--- LATENCY BREAKDOWN ---" << std::endl;
            std::cout << "Exchange → UDP Receive: " << exch_to_udp << " ns (" 
                      << (exch_to_udp / 1000.0) << " μs)" << std::endl;
            std::cout << "UDP Receive → Queue: " << udp_to_queue << " ns (" 
                      << (udp_to_queue / 1000.0) << " μs)" << std::endl;
            std::cout << "Queue → Strategy: " << queue_to_strategy << " ns (" 
                      << (queue_to_strategy / 1000.0) << " μs)" << std::endl;
            std::cout << "TOTAL LATENCY: " << total_latency << " ns (" 
                      << (total_latency / 1000.0) << " μs)" << std::endl;
            
            // Print statistics every 10 quotes
            if (total_quotes % 10 == 0) {
                std::cout << "\n📊 PERFORMANCE STATISTICS (Last " << total_quotes << " quotes):" << std::endl;
                std::cout << "Avg Exchange→UDP: " << (total_exchange_latency / total_quotes) << " ns" << std::endl;
                std::cout << "Avg UDP→Queue: " << (total_queue_latency / total_quotes) << " ns" << std::endl;
                std::cout << "Avg Queue→Strategy: " << (total_strategy_latency / total_quotes) << " ns" << std::endl;
                std::cout << "Avg Total Latency: " << (total_end_to_end / total_quotes) << " ns" << std::endl;
                
                // Note: Producer statistics are tracked but not accessible from consumer
                // This maintains the one writer rule - only consumer prints
                std::cout << "Producer Stats - Pushed: [tracked], Dropped: [tracked], Avg Push Latency: [tracked]" << std::endl;
                std::cout << "=====================" << std::endl;
            }
        } else {
            // No quotes available - yield CPU to other threads
            // This prevents busy-waiting and reduces CPU usage
            std::this_thread::yield();
        }
    }
    
    std::cout << "Print consumer shutting down..." << std::endl;
}

int main() {
    std::cout << "UDP Quote Printer - Starting up..." << std::endl;
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // TODO: Configure these parameters
    const uint16_t udp_port = 12345;  // Change to your UDP port
    const size_t queue_capacity = 10000;  // Adjust based on your needs
    
    try {
        // Initialize components
        SPSCRingBuffer<Quote> quote_queue(queue_capacity);
        UDPListener listener(udp_port);
        
        // Initialize listener
        if (!listener.initialize()) {
            std::cerr << "Failed to initialize UDP listener" << std::endl;
            return 1;
        }
        
        // Set shutdown flag for graceful shutdown
        listener.set_shutdown_flag(&shutdown_flag);
        
        // Start consumer thread
        std::thread consumer_thread(print_consumer, std::ref(quote_queue));
        
        // Run producer in main thread
        ingress_producer(quote_queue, listener);
        
        // Wait for consumer thread to finish
        if (consumer_thread.joinable()) {
            consumer_thread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "UDP Quote Printer - Shutdown complete" << std::endl;
    return 0;
} 