#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <map>
#include <unordered_map>
#include <deque>
#include <cstdint>
#include <string>
#include <vector>
#include "quote.hpp"

// Individual order structure
struct Order {
    std::string order_id;
    OrderSide side;
    double price;
    uint32_t size;
    uint64_t timestamp;
    std::string symbol;
    
    Order() = default;
    Order(const std::string& oid, OrderSide s, double p, uint32_t sz, uint64_t ts, const std::string& sym)
        : order_id(oid), side(s), price(p), size(sz), timestamp(ts), symbol(sym) {}
};

// Order entry in price level queue
struct OrderEntry {
    std::string order_id;
    uint32_t size;
    uint64_t timestamp;  // For price-time priority
    
    OrderEntry(const std::string& id, uint32_t s, uint64_t ts) 
        : order_id(id), size(s), timestamp(ts) {}
};

// Price level structure with FIFO ordering for price-time priority
struct PriceLevel {
    double price;
    uint32_t total_size;
    std::deque<OrderEntry> order_queue;  // FIFO queue for price-time priority
    std::unordered_map<std::string, std::deque<OrderEntry>::iterator> order_lookup;  // O(1) lookup by order_id
    
    PriceLevel() : price(0.0), total_size(0) {}
    PriceLevel(double p) : price(p), total_size(0) {}
    
    void add_order(const std::string& order_id, uint32_t size, uint64_t timestamp) {
        // Add to end of queue (FIFO)
        order_queue.emplace_back(order_id, size, timestamp);
        auto it = order_queue.end();
        --it;  // Get iterator to the newly added element
        order_lookup[order_id] = it;
        total_size += size;
    }
    
    void modify_order(const std::string& order_id, uint32_t new_size) {
        auto lookup_it = order_lookup.find(order_id);
        if (lookup_it != order_lookup.end()) {
            auto queue_it = lookup_it->second;
            total_size = total_size - queue_it->size + new_size;
            queue_it->size = new_size;
        }
    }
    
    void remove_order(const std::string& order_id) {
        auto lookup_it = order_lookup.find(order_id);
        if (lookup_it != order_lookup.end()) {
            auto queue_it = lookup_it->second;
            total_size -= queue_it->size;
            
            // Remove from queue (this invalidates iterators, so we need to rebuild lookup)
            order_queue.erase(queue_it);
            order_lookup.erase(lookup_it);
            
            // Rebuild lookup map with new iterators
            rebuild_lookup();
        }
    }
    
    bool empty() const {
        return order_queue.empty();
    }
    
    // Get the next order to execute (front of queue)
    const OrderEntry* get_next_order() const {
        if (order_queue.empty()) return nullptr;
        return &order_queue.front();
    }
    
    // Get all orders in FIFO order
    std::vector<OrderEntry> get_orders_fifo() const {
        std::vector<OrderEntry> result;
        for (const auto& entry : order_queue) {
            result.push_back(entry);
        }
        return result;
    }
    
private:
    void rebuild_lookup() {
        order_lookup.clear();
        for (auto it = order_queue.begin(); it != order_queue.end(); ++it) {
            order_lookup[it->order_id] = it;
        }
    }
};

// Optimized order book with O(1) operations using order_id
class OrderBook {
private:
    // Order lookup by order_id for O(1) access
    std::unordered_map<std::string, Order> orders_by_id;
    
    // Price level aggregation for efficient best bid/ask
    std::map<double, PriceLevel, std::greater<double>> bid_levels;  // price -> level (descending)
    std::map<double, PriceLevel> ask_levels;                        // price -> level (ascending)

public:
    OrderBook() = default;
    
    // O(1) add order by order_id
    bool add_order(const std::string& order_id, OrderSide side, double price, uint32_t size, 
                   const std::string& symbol = "", uint64_t timestamp = 0) {
        // Check if order already exists
        if (orders_by_id.find(order_id) != orders_by_id.end()) {
            return false;  // Order already exists
        }
        
        // Create order
        Order order(order_id, side, price, size, timestamp, symbol);
        
        // Store order by order_id for O(1) lookup
        orders_by_id[order_id] = order;
        
        // Update price level aggregation with FIFO ordering
        if (side == OrderSide::BID) {
            if (bid_levels.find(price) == bid_levels.end()) {
                bid_levels[price] = PriceLevel(price);
            }
            bid_levels[price].add_order(order_id, size, timestamp);
        } else if (side == OrderSide::ASK) {
            if (ask_levels.find(price) == ask_levels.end()) {
                ask_levels[price] = PriceLevel(price);
            }
            ask_levels[price].add_order(order_id, size, timestamp);
        }
        
        return true;
    }
    
    // O(1) modify order by order_id
    bool modify_order(const std::string& order_id, uint32_t new_size) {
        auto order_it = orders_by_id.find(order_id);
        if (order_it == orders_by_id.end()) {
            return false;  // Order not found
        }
        
        Order& order = order_it->second;
        order.size = new_size;
        
        // Update price level aggregation
        if (order.side == OrderSide::BID) {
            auto level_it = bid_levels.find(order.price);
            if (level_it != bid_levels.end()) {
                level_it->second.modify_order(order_id, new_size);
            }
        } else if (order.side == OrderSide::ASK) {
            auto level_it = ask_levels.find(order.price);
            if (level_it != ask_levels.end()) {
                level_it->second.modify_order(order_id, new_size);
            }
        }
        
        return true;
    }
    
    // O(1) cancel order by order_id
    bool cancel_order(const std::string& order_id) {
        auto order_it = orders_by_id.find(order_id);
        if (order_it == orders_by_id.end()) {
            return false;  // Order not found
        }
        
        const Order& order = order_it->second;
        
        // Update price level aggregation
        if (order.side == OrderSide::BID) {
            auto level_it = bid_levels.find(order.price);
            if (level_it != bid_levels.end()) {
                level_it->second.remove_order(order_id);
                // Remove empty price levels
                if (level_it->second.empty()) {
                    bid_levels.erase(level_it);
                }
            }
        } else if (order.side == OrderSide::ASK) {
            auto level_it = ask_levels.find(order.price);
            if (level_it != ask_levels.end()) {
                level_it->second.remove_order(order_id);
                // Remove empty price levels
                if (level_it->second.empty()) {
                    ask_levels.erase(level_it);
                }
            }
        }
        
        // Remove order from lookup
        orders_by_id.erase(order_it);
        
        return true;
    }
    
    
    // O(1) get best bid using price level aggregation
    std::pair<double, uint32_t> get_best_bid() const {
        if (bid_levels.empty()) return {0.0, 0};
        auto it = bid_levels.begin();
        return {it->second.price, it->second.total_size};
    }
    
    // O(1) get best ask using price level aggregation
    std::pair<double, uint32_t> get_best_ask() const {
        if (ask_levels.empty()) return {0.0, 0};
        auto it = ask_levels.begin();
        return {it->second.price, it->second.total_size};
    }
    
    double get_spread() const {
        auto bid = get_best_bid();
        auto ask = get_best_ask();
        if (bid.first > 0 && ask.first > 0) {
            return ask.first - bid.first;
        }
        return 0.0;
    }
    
    // O(log n) get total size at a specific price level
    uint32_t get_size_at_price(OrderSide side, double price) const {
        if (side == OrderSide::BID) {
            auto it = bid_levels.find(price);
            return (it != bid_levels.end()) ? it->second.total_size : 0;
        } else if (side == OrderSide::ASK) {
            auto it = ask_levels.find(price);
            return (it != ask_levels.end()) ? it->second.total_size : 0;
        }
        return 0;
    }
    
    // Get number of price levels on each side
    size_t get_bid_levels() const {
        return bid_levels.size();
    }
    
    size_t get_ask_levels() const {
        return ask_levels.size();
    }
    
    // Get number of individual orders
    size_t get_total_orders() const {
        return orders_by_id.size();
    }
    
    // O(1) check if order exists by order_id
    bool has_order(const std::string& order_id) const {
        return orders_by_id.find(order_id) != orders_by_id.end();
    }
    
    // O(1) get order by order_id
    const Order* get_order(const std::string& order_id) const {
        auto it = orders_by_id.find(order_id);
        return (it != orders_by_id.end()) ? &it->second : nullptr;
    }
    
    // Check if order book is empty
    bool is_empty() const {
        return bid_levels.empty() && ask_levels.empty();
    }
    
    // Clear the order book
    void clear() {
        orders_by_id.clear();
        bid_levels.clear();
        ask_levels.clear();
    }
    
    // Get all orders at a price level in FIFO order (for debugging/analysis)
    std::vector<std::string> get_orders_at_price(OrderSide side, double price) const {
        std::vector<std::string> order_ids;
        if (side == OrderSide::BID) {
            auto it = bid_levels.find(price);
            if (it != bid_levels.end()) {
                auto orders_fifo = it->second.get_orders_fifo();
                for (const auto& entry : orders_fifo) {
                    order_ids.push_back(entry.order_id);
                }
            }
        } else if (side == OrderSide::ASK) {
            auto it = ask_levels.find(price);
            if (it != ask_levels.end()) {
                auto orders_fifo = it->second.get_orders_fifo();
                for (const auto& entry : orders_fifo) {
                    order_ids.push_back(entry.order_id);
                }
            }
        }
        return order_ids;
    }
    
    // Get next order to execute at a price level (FIFO front)
    const OrderEntry* get_next_order_at_price(OrderSide side, double price) const {
        if (side == OrderSide::BID) {
            auto it = bid_levels.find(price);
            if (it != bid_levels.end()) {
                return it->second.get_next_order();
            }
        } else if (side == OrderSide::ASK) {
            auto it = ask_levels.find(price);
            if (it != ask_levels.end()) {
                return it->second.get_next_order();
            }
        }
        return nullptr;
    }
};

#endif // ORDERBOOK_HPP
