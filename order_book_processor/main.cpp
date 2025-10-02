#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <signal.h>

#include "queue.hpp"
#include "listener.hpp"
#include "quote.hpp"
#include "orderbook.hpp"
#include "multicast_publisher.hpp"
#include <map>
#include <set>

// Global flag for graceful shutdown
std::atomic<bool> shutdown_flag{false};


// Global order books for each symbol
std::map<std::string, OrderBook> order_books;


// Global multicast publisher
std::unique_ptr<MulticastPublisher> multicast_publisher;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal, shutting down gracefully..." << std::endl;
        shutdown_flag.store(true);
    }
}

// Producer function - runs in main thread
void ingress_producer(SPSCRingBuffer<OrderBookEvent>& queue, UDPListener& listener) {
    std::cout << "Starting ingress producer..." << std::endl;
    
    // Producer statistics (atomic counters, no printing)
    static std::atomic<uint64_t> events_pushed{0};
    static std::atomic<uint64_t> total_push_latency{0};
    static std::atomic<uint64_t> events_dropped{0};
    
    // Set callback to push order book events to queue
    listener.set_order_book_callback([&queue](const OrderBookEvent& event) {
        // Monotonic timestamp for enqueue event
        auto enq_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // Prepare a copy to store enqueue timestamp (SPSC copies anyway)
        OrderBookEvent e = event;
        e.enqueued_mono_ns = enq_ns;
        
        // Try to push event to ring buffer
        if (queue.try_push(e)) {
            // Successfully pushed event to queue - just update counters
            // No printing from producer thread (one writer rule)
            uint64_t udp_to_queue = (e.udp_rx_mono_ns > 0 && static_cast<uint64_t>(enq_ns) >= e.udp_rx_mono_ns)
                ? (static_cast<uint64_t>(enq_ns) - e.udp_rx_mono_ns) : 0ULL;
            events_pushed.fetch_add(1, std::memory_order_relaxed);
            total_push_latency.fetch_add(udp_to_queue, std::memory_order_relaxed);
        } else {
            // Queue is full - just update error counter
            // No printing from producer thread
            events_dropped.fetch_add(1, std::memory_order_relaxed);
        }
    });
    
    // Start listening
    listener.listen();
}

// Consumer function - runs in separate thread
void print_consumer(SPSCRingBuffer<OrderBookEvent>& queue) {
    std::cout << "Starting print consumer..." << std::endl;
    
    while (!shutdown_flag.load()) {
        OrderBookEvent event;
        
        // Try to pop event from queue
        if (queue.try_pop(event)) {
            // Get current monotonic timestamp when strategy thread sees the event
            auto deq_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            // Calculate latency metrics (monotonic, single epoch)
            uint64_t exch_to_udp = (event.exchange_mono_ns > 0 && event.udp_rx_mono_ns >= event.exchange_mono_ns)
                ? (event.udp_rx_mono_ns - event.exchange_mono_ns) : 0ULL;
            uint64_t udp_to_queue = (event.enqueued_mono_ns >= event.udp_rx_mono_ns)
                ? (event.enqueued_mono_ns - event.udp_rx_mono_ns) : 0ULL;
            uint64_t queue_to_strategy = (static_cast<uint64_t>(deq_ns) >= event.enqueued_mono_ns)
                ? (static_cast<uint64_t>(deq_ns) - event.enqueued_mono_ns) : 0ULL;
            uint64_t total_latency = exch_to_udp + udp_to_queue + queue_to_strategy;
            
            // Update statistics counters
            static uint64_t total_events = 0;
            static uint64_t total_exchange_latency = 0;
            static uint64_t total_queue_latency = 0;
            static uint64_t total_strategy_latency = 0;
            static uint64_t total_end_to_end = 0;
            
            total_events++;
            total_exchange_latency += exch_to_udp;
            total_queue_latency += udp_to_queue;
            total_strategy_latency += queue_to_strategy;
            total_end_to_end += total_latency;
            
            // Update order book based on event type
            if (event.symbol.empty()) {
                std::cout << "Warning: Received event with empty symbol" << std::endl;
                continue;
            }
            
            OrderBook& book = order_books[event.symbol];
            
            switch (event.event_type) {
                case OrderBookEventType::ADD_ORDER:
                    // O(1) add order by order_id
                    book.add_order(event.order_id, event.side, event.price, event.size, 
                                 event.symbol, event.timestamp);
                    break;
                case OrderBookEventType::MODIFY_ORDER:
                    // O(1) modify order by order_id
                    book.modify_order(event.order_id, event.size);
                    break;
                case OrderBookEventType::CANCEL_ORDER:
                    // O(1) cancel order by order_id
                    book.cancel_order(event.order_id);
                    break;
                case OrderBookEventType::DELETE_ORDER:
                    // O(1) cancel order by order_id
                    book.cancel_order(event.order_id);
                    break;
                case OrderBookEventType::TRADE:
                    // Trades don't directly modify the order book in this simple implementation
                    // API is now standalone - no direct updates needed
                    
                    // Publish trade to multicast
                    if (multicast_publisher) {
                        multicast_publisher->publish_trade_update(event.symbol, event.trade_price, 
                                                               event.trade_size,
                                                               event.is_aggressor ? OrderSide::BID : OrderSide::ASK,
                                                               event.timestamp);
                    }
                    break;
                default:
                    break;
            }
                        
            // Publish to multicast
            if (multicast_publisher) {
                uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                multicast_publisher->publish_order_book_update(event.symbol, book, timestamp);
            }
            
            // Print event information
            std::cout << "\n=== ORDER BOOK EVENT RECEIVED ===" << std::endl;
            std::cout << "Symbol: " << event.symbol << std::endl;
            std::cout << "Event Type: " << static_cast<int>(event.event_type) << std::endl;
            std::cout << "Order ID: " << event.order_id << std::endl;
            std::cout << "Side: " << static_cast<int>(event.side) << std::endl;
            std::cout << "Price: " << event.price << std::endl;
            std::cout << "Size: " << event.size << std::endl;
            
            if (event.event_type == OrderBookEventType::TRADE) {
                std::cout << "Trade Price: " << event.trade_price << std::endl;
                std::cout << "Trade Size: " << event.trade_size << std::endl;
                std::cout << "Is Aggressor: " << (event.is_aggressor ? "Yes" : "No") << std::endl;
            }
            
            // Show current order book state
            auto best_bid = book.get_best_bid();
            auto best_ask = book.get_best_ask();
            double spread = book.get_spread();
            
            std::cout << "--- CURRENT ORDER BOOK ---" << std::endl;
            std::cout << "Best Bid: " << best_bid.first << " x " << best_bid.second << std::endl;
            std::cout << "Best Ask: " << best_ask.first << " x " << best_ask.second << std::endl;
            std::cout << "Spread: " << spread << std::endl;
            
            std::cout << "--- LATENCY BREAKDOWN ---" << std::endl;
            std::cout << "Exchange → UDP Receive: " << exch_to_udp << " ns (" 
                      << (exch_to_udp / 1000.0) << " μs)" << std::endl;
            std::cout << "UDP Receive → Queue: " << udp_to_queue << " ns (" 
                      << (udp_to_queue / 1000.0) << " μs)" << std::endl;
            std::cout << "Queue → Strategy: " << queue_to_strategy << " ns (" 
                      << (queue_to_strategy / 1000.0) << " μs)" << std::endl;
            std::cout << "TOTAL LATENCY: " << total_latency << " ns (" 
                      << (total_latency / 1000.0) << " μs)" << std::endl;
            
            // Print statistics every 10 events
            if (total_events % 10 == 0) {
                std::cout << "\n📊 PERFORMANCE STATISTICS (Last " << total_events << " events):" << std::endl;
                std::cout << "Avg Exchange→UDP: " << (total_exchange_latency / total_events) << " ns" << std::endl;
                std::cout << "Avg UDP→Queue: " << (total_queue_latency / total_events) << " ns" << std::endl;
                std::cout << "Avg Queue→Strategy: " << (total_strategy_latency / total_events) << " ns" << std::endl;
                std::cout << "Avg Total Latency: " << (total_end_to_end / total_events) << " ns" << std::endl;
                
                // Note: Producer statistics are tracked but not accessible from consumer
                // This maintains the one writer rule - only consumer prints
                std::cout << "Producer Stats - Pushed: [tracked], Dropped: [tracked], Avg Push Latency: [tracked]" << std::endl;
                std::cout << "=====================" << std::endl;
            }
        } else {
            // No events available - yield CPU to other threads
            // This prevents busy-waiting and reduces CPU usage
            std::this_thread::yield();
        }
    }
    
    std::cout << "Print consumer shutting down..." << std::endl;
}

int main() {
    std::cout << "Multicast Market Feed Subscriber - Starting up..." << std::endl;
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Configurable parameters
    const std::string multicast_group = "224.0.0.1";  // Standard multicast group
    const uint16_t multicast_port = 12345;           // Port for market data multicast
    const size_t queue_capacity = 10000;
    
    try {
        // Initialize multicast publisher (API is now standalone)
        multicast_publisher = std::make_unique<MulticastPublisher>();
        if (!multicast_publisher->initialize("224.0.0.1", 12346)) {
            std::cerr << "Failed to initialize multicast publisher" << std::endl;
            return 1;
        }
        
        // Initialize components
        SPSCRingBuffer<OrderBookEvent> event_queue(queue_capacity);
        UDPListener listener(multicast_group, multicast_port);
        
        // Initialize multicast listener
        if (!listener.initialize()) {
            std::cerr << "Failed to initialize multicast listener" << std::endl;
            return 1;
        }
        
        // Set shutdown flag for graceful shutdown
        listener.set_shutdown_flag(&shutdown_flag);
        
        // Start consumer thread
        std::thread consumer_thread(print_consumer, std::ref(event_queue));
        
        // Run producer in main thread
        ingress_producer(event_queue, listener);
        
        // Wait for consumer thread to finish
        if (consumer_thread.joinable()) {
            consumer_thread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    }
        
    std::cout << "Multicast Market Feed Subscriber - Shutdown complete" << std::endl;
    return 0;
} 