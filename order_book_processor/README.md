# Order Book Processor

This directory contains the C++ UDP order book processor that receives, processes, and reconstructs market data from exchange feeds.

## Files

- `main.cpp` - Main application with producer-consumer architecture
- `listener.hpp` - UDP socket listener with JSON parsing
- `queue.hpp` - Lock-free SPSC ring buffer implementation
- `quote.hpp` - Data structures for order book events
- `orderbook.hpp` - Order book reconstruction logic
- `Makefile` - Build configuration
- `test_market_feed.sh` - Integration test script

## Building

```bash
# Compile the order book processor
make

# Clean build artifacts
make clean
```

## Running

```bash
# Run the order book processor
./udp_quote_printer

# Run with integration test
./test_market_feed.sh
```

## Architecture

### Producer-Consumer Design
- **Producer Thread**: Receives UDP packets, parses JSON, enqueues events
- **Consumer Thread**: Processes events, updates order books, displays metrics

### Key Components

1. **UDP Listener** (`listener.hpp`)
   - Non-blocking UDP socket
   - JSON parsing for order book events
   - Monotonic timestamp capture

2. **Lock-Free Queue** (`queue.hpp`)
   - Single Producer Single Consumer (SPSC) ring buffer
   - Cache-aligned memory for performance
   - Atomic operations with relaxed memory ordering

3. **Order Book** (`orderbook.hpp`)
   - Bid/ask price level tracking
   - Real-time order book reconstruction
   - Best bid/ask and spread calculation

4. **Event Processing** (`main.cpp`)
   - Event type handling (ADD, MODIFY, CANCEL, TRADE)
   - Latency measurement and statistics
   - Performance monitoring

## Performance Features

- **Low Latency**: Lock-free design, cache-aligned memory
- **Real-time Processing**: Monotonic timestamps, nanosecond precision
- **Memory Efficient**: Ring buffer with power-of-2 sizing
- **Thread Safe**: SPSC queue eliminates locking overhead

## Latency Measurement

The system measures end-to-end latency:
- **Exchange → UDP Receive**: Network and socket processing
- **UDP Receive → Queue**: JSON parsing and enqueueing
- **Queue → Strategy**: Event processing and order book updates

## Event Types Supported

- `ADD_ORDER` - New orders added to book
- `MODIFY_ORDER` - Existing orders modified
- `CANCEL_ORDER` - Orders cancelled/removed
- `DELETE_ORDER` - Orders deleted from book
- `TRADE` - Order executions
- `QUOTE_UPDATE` - Top-of-book updates
- `MARKET_STATUS` - Session control messages

## Dependencies

- C++17 compiler (g++ or clang++)
- pthread library
- No external dependencies (manual JSON parsing)

## Testing

The `test_market_feed.sh` script provides automated testing:
- Compiles the C++ processor
- Starts the Python market data simulator
- Validates event processing and performance
- Checks for errors and statistics generation
