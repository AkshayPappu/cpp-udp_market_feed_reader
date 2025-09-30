#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <map>
#include <cstdint>
#include "quote.hpp"

// Simple order book structure for tracking market depth
struct OrderBook {
    std::map<double, uint32_t, std::greater<double>> bids;  // price -> size (descending)
    std::map<double, uint32_t> asks;                        // price -> size (ascending)
    
    void add_order(OrderSide side, double price, uint32_t size) {
        if (side == OrderSide::BID) {
            bids[price] += size;
        } else if (side == OrderSide::ASK) {
            asks[price] += size;
        }
    }
    
    void modify_order(OrderSide side, double price, uint32_t new_size) {
        if (side == OrderSide::BID) {
            bids[price] = new_size;
        } else if (side == OrderSide::ASK) {
            asks[price] = new_size;
        }
    }
    
    void cancel_order(OrderSide side, double price, uint32_t size) {
        if (side == OrderSide::BID) {
            bids[price] -= size;
            if (bids[price] == 0) bids.erase(price);
        } else if (side == OrderSide::ASK) {
            asks[price] -= size;
            if (asks[price] == 0) asks.erase(price);
        }
    }
    
    std::pair<double, uint32_t> get_best_bid() const {
        if (bids.empty()) return {0.0, 0};
        auto it = bids.begin();
        return {it->first, it->second};
    }
    
    std::pair<double, uint32_t> get_best_ask() const {
        if (asks.empty()) return {0.0, 0};
        auto it = asks.begin();
        return {it->first, it->second};
    }
    
    double get_spread() const {
        auto bid = get_best_bid();
        auto ask = get_best_ask();
        if (bid.first > 0 && ask.first > 0) {
            return ask.first - bid.first;
        }
        return 0.0;
    }
    
    // Get total size at a specific price level
    uint32_t get_size_at_price(OrderSide side, double price) const {
        if (side == OrderSide::BID) {
            auto it = bids.find(price);
            return (it != bids.end()) ? it->second : 0;
        } else if (side == OrderSide::ASK) {
            auto it = asks.find(price);
            return (it != asks.end()) ? it->second : 0;
        }
        return 0;
    }
    
    // Get number of price levels on each side
    size_t get_bid_levels() const {
        return bids.size();
    }
    
    size_t get_ask_levels() const {
        return asks.size();
    }
    
    // Check if order book is empty
    bool is_empty() const {
        return bids.empty() && asks.empty();
    }
    
    // Clear the order book
    void clear() {
        bids.clear();
        asks.clear();
    }
};

#endif // ORDERBOOK_HPP
