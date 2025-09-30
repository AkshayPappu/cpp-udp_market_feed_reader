# Order Book API

A simple REST API server that provides real-time access to order book metrics and market data while your strategy is running. Built with a lightweight HTTP server implementation for maximum performance.

## Features

### Tier 1: Must-Have (MVP) Metrics
- **Best Bid/Ask**: Price and size of the best bid and ask
- **Spread**: Ask price minus bid price
- **Midprice**: Average of best bid and ask prices
- **Quote Imbalance**: (bid_size - ask_size) / (bid_size + ask_size)
- **Depth Snapshot**: Top N levels of the order book
- **Trade Direction**: Aggressor side for the last trade

## API Endpoints

### Health Check
```bash
GET /api/health
```
Returns server status and basic information.

### Available Symbols
```bash
GET /api/symbols
```
Returns a list of all symbols with available data.

### Market Metrics
```bash
GET /api/metrics/{SYMBOL}
```
Returns all Tier 1 metrics for a specific symbol.

**Example Response:**
```json
{
  "best_bid_price": 150.25,
  "best_bid_size": 1000,
  "best_ask_price": 150.30,
  "best_ask_size": 500,
  "spread": 0.05,
  "midprice": 150.275,
  "quote_imbalance": 0.333333,
  "last_update_timestamp": 1696123456789000000,
  "total_events_processed": 1250
}
```

### Order Book Depth
```bash
GET /api/depth/{SYMBOL}
```
Returns the top N levels of the order book for both bids and asks.

**Example Response:**
```json
{
  "symbol": "AAPL",
  "bid_depth": [
    {"price": 150.25, "size": 1000},
    {"price": 150.24, "size": 500}
  ],
  "ask_depth": [
    {"price": 150.30, "size": 500},
    {"price": 150.31, "size": 750}
  ]
}
```

### Last Trade
```bash
GET /api/trades/{SYMBOL}
```
Returns information about the last trade for a symbol.

**Example Response:**
```json
{
  "price": 150.28,
  "size": 200,
  "aggressor_side": "BID",
  "timestamp": 1696123456789000000
}
```

## Building

```bash
# Build the API server
make

# Build with debug symbols
make debug

# Clean build artifacts
make clean
```

## Running

```bash
# Start the API server
./simple_api

# Or use make
make run
```

The server will start on port 8080 by default.

## Integration with Order Book Processor

To integrate with your existing order book processor, you need to:

1. Include the API header in your main.cpp
2. Create an API instance
3. Update the API with order book data

```cpp
#include "simple_api.hpp"

// In your main function
SimpleOrderBookAPI api(8080);
api.start();

// In your event processing loop
api.update_order_book(symbol, order_book);
api.update_trade(symbol, trade_price, trade_size, aggressor_side, timestamp);
api.increment_event_count(symbol);
```

## Testing

```bash
# Run automated tests
make test

# Manual testing
curl http://localhost:8080/api/health
curl http://localhost:8080/api/symbols
curl http://localhost:8080/api/metrics/AAPL
```

## Dependencies

- C++17 compiler
- pthread library
- curl (for testing)
- jq (for JSON formatting in tests)

## Configuration

- **Port**: Default 8080, configurable in constructor
- **Depth Levels**: Default 5, configurable via `set_depth_levels()`
- **Update Frequency**: Real-time updates as events are processed

## Performance

- **Low Latency**: In-memory data structures with minimal locking
- **Thread Safe**: Mutex-protected data access
- **Efficient**: JSON serialization only when requested
- **Scalable**: Handles multiple symbols concurrently
