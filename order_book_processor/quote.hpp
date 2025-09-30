#ifndef QUOTE_HPP
#define QUOTE_HPP

#include <cstdint>
#include <string>

// Order book event types (Level 2/3 market data)
enum class OrderBookEventType {
    ADD_ORDER,      // New order added to book
    MODIFY_ORDER,   // Existing order modified (price/size)
    CANCEL_ORDER,   // Order cancelled
    DELETE_ORDER,   // Order removed from book
    TRADE,          // Order executed (trade print)
    QUOTE_UPDATE,   // Top-of-book quote update (Level 1)
    MARKET_STATUS,  // Session start/end, halts, etc.
    UNKNOWN         // Unknown event type
};

// Order side
enum class OrderSide {
    BID,    // Buy side
    ASK,    // Sell side
    UNKNOWN
};

struct OrderBookEvent {
    // Event identification
    OrderBookEventType event_type = OrderBookEventType::UNKNOWN;
    std::string symbol;
    std::string exchange;
    std::string order_id;        // Exchange order ID
    
    // Order details
    OrderSide side = OrderSide::UNKNOWN;
    double price = 0.0;
    uint32_t size = 0;           // Order size
    uint32_t remaining_size = 0; // For modify/cancel events
    
    // Trade details (for TRADE events)
    double trade_price = 0.0;
    uint32_t trade_size = 0;
    bool is_aggressor = false;   // True if this order was the aggressor
    
    // Market status (for MARKET_STATUS events)
    std::string status_message;
    bool is_trading_halted = false;
    
    // Timestamps
    uint64_t timestamp = 0;           // Exchange timestamp (wall clock)
    uint64_t sequence_number = 0;     // Exchange sequence number
    
    // Monotonic timestamps (nanoseconds, single epoch for latency measurement)
    uint64_t exchange_mono_ns = 0;    // When exchange generated the event
    uint64_t udp_rx_mono_ns = 0;      // When UDP listener received the packet
    uint64_t enqueued_mono_ns = 0;    // When producer enqueued into SPSC

    // Default constructor
    OrderBookEvent() = default;

    // Convenience constructors
    OrderBookEvent(OrderBookEventType type, const std::string& sym, const std::string& ex)
        : event_type(type), symbol(sym), exchange(ex) {}
        
    OrderBookEvent(OrderBookEventType type, const std::string& sym, const std::string& ex,
                   OrderSide s, double p, uint32_t sz, const std::string& oid)
        : event_type(type), symbol(sym), exchange(ex), order_id(oid),
          side(s), price(p), size(sz) {}
};

// Legacy Quote struct for backward compatibility
struct Quote {
    // Core market data (top-of-book)
    std::string symbol;
    double bid_price;
    uint32_t bid_size;
    double ask_price;
    uint32_t ask_size;

    // Wall-clock exchange timestamp from feed (may be simulated)
    uint64_t timestamp;           // Exchange timestamp (wall clock, if provided)
    std::string exchange;

    // Monotonic timestamps (nanoseconds, single epoch for the whole pipeline)
    // These are used for latency calculations
    uint64_t exchange_mono_ns = 0;   // When exchange generated (simulated) the update
    uint64_t udp_rx_mono_ns = 0;     // When UDP listener received the packet
    uint64_t enqueued_mono_ns = 0;   // When producer enqueued into SPSC

    // Legacy fields (not used in new monotonic pipeline)
    uint64_t receive_timestamp = 0;   // Deprecated
    uint64_t queue_push_timestamp = 0; // Deprecated
    uint64_t push_latency = 0;         // Deprecated
    bool queue_full = false;           // For overflow tracking if needed

    // Default constructor
    Quote() = default;

    // Convenience constructor
    Quote(const std::string& sym, double bid, uint32_t bid_sz,
          double ask, uint32_t ask_sz, uint64_t ts, const std::string& ex)
        : symbol(sym), bid_price(bid), bid_size(bid_sz),
          ask_price(ask), ask_size(ask_sz), timestamp(ts), exchange(ex) {}
};

#endif // QUOTE_HPP 