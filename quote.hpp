#ifndef QUOTE_HPP
#define QUOTE_HPP

#include <cstdint>
#include <string>

struct Quote {
    // Core market data
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