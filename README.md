# UDP Market Data System - Low-Latency Order Book Processing

A high-performance market data processing system with Python simulator, C++ order book processor, and REST API. Demonstrates real-time Level 2/3 order book reconstruction with sub-microsecond latency measurement.

## 🚀 Quick Start

### Prerequisites
- GCC 7+ with C++17 support
- Python 3.7+
- Linux (tested on RHEL 9)

### Build & Run
```bash
# Build all components
cd order_book_processor && make
cd ../order_book_api && make standalone_api

# Start the complete system
./start_system.sh

# Stop the system
./shutdown_system.sh
```

### Test API
```bash
curl http://localhost:8080/api/health
curl http://localhost:8080/api/symbols
curl http://localhost:8080/api/metrics/AAPL
```

## 📁 Project Structure

```
├── market_feed_client/          # Python market data simulator
├── order_book_processor/        # C++ order book processor (multicast publisher)
├── order_book_api/             # REST API server (multicast subscriber)
├── start_system.sh             # System startup script
└── shutdown_system.sh          # System shutdown script
```

## 🏗️ Architecture

**Decoupled System with UDP Multicast:**
```
[Python Simulator] → [Order Book Processor] → [UDP Multicast] → [API Server]
     ↓                        ↓                      ↓              ↓
  Generate Events        Process & Publish       224.0.0.1:12346  Serve REST API
```

**Components:**
- **Market Feed Client**: Generates realistic Level 2/3 order book events
- **Order Book Processor**: Processes events, maintains order books, publishes via multicast
- **API Server**: Subscribes to multicast, serves REST API with real-time metrics

## 🎯 Features

### Core Capabilities
- **Level 2/3 Order Book Events**: ADD, MODIFY, CANCEL, TRADE, QUOTE_UPDATE
- **Real-Time Order Book Reconstruction**: Live in-memory order books
- **Lock-Free SPSC Ring Buffer**: Zero-copy inter-thread communication
- **UDP Multicast Decoupling**: Scalable publisher-subscriber architecture
- **REST API**: Real-time order book metrics and trade data

### Performance Optimizations
- **Cache-Aligned Memory**: 64-byte aligned allocations
- **Lock-Free Operations**: Atomic operations with relaxed memory ordering
- **Sub-Microsecond Latency**: End-to-end latency measurement
- **High Throughput**: 1000+ events/second processing

### API Endpoints
- `GET /api/health` - System health check
- `GET /api/symbols` - Available symbols
- `GET /api/metrics/{SYMBOL}` - Order book metrics (bid/ask, spread, midprice, etc.)
- `GET /api/depth/{SYMBOL}` - Order book depth snapshot
- `GET /api/trades/{SYMBOL}` - Recent trade information

## 📊 Latency Measurement

The system measures end-to-end latency:
- **Exchange → UDP Receive**: Network transmission time
- **UDP Receive → Queue**: Parse and enqueue time
- **Queue → Strategy**: Processing time
- **Total End-to-End**: Complete pipeline latency

**Typical Performance:**
- UDP → Queue: < 100 nanoseconds
- Queue → Strategy: < 50 microseconds
- Total End-to-End: < 200 microseconds

## 🛠️ Manual Operation

### Build Components
```bash
# Order book processor
cd order_book_processor
make

# API server
cd ../order_book_api
make standalone_api
```

### Run Components
```bash
# Terminal 1: Market data simulator
cd market_feed_client
python3 market_feed_simulator.py --rate 50

# Terminal 2: Order book processor (publisher)
cd ../order_book_processor
./udp_quote_printer

# Terminal 3: API server (subscriber)
cd ../order_book_api
./standalone_api
```

## 🧪 Testing

### Automated Testing
```bash
# Run integration test
cd order_book_processor
./test_market_feed.sh
```

### API Testing
```bash
# Health check
curl http://localhost:8080/api/health

# Get symbols
curl http://localhost:8080/api/symbols

# Get metrics for AAPL
curl http://localhost:8080/api/metrics/AAPL

# Get order book depth
curl http://localhost:8080/api/depth/AAPL

# Get recent trades
curl http://localhost:8080/api/trades/AAPL
```

## 🔧 Configuration

### Default Settings
- **UDP Port**: 12345 (market data)
- **API Port**: 8080 (REST API)
- **Multicast**: 224.0.0.1:12346
- **Event Rate**: 50 events/second
- **Buffer Size**: 1024 (power-of-2)

### Customization
- Modify `start_system.sh` for different rates/ports
- Edit `market_feed_simulator.py` for different symbols
- Adjust buffer sizes in `main.cpp`

## 📈 Monitoring

### Real-Time Output
- Per-event latency breakdown
- Order book state (best bid/ask, spread)
- Performance statistics every 10 events
- API request/response logging

### Performance Statistics
```
📊 PERFORMANCE STATISTICS (Last N events):
Avg Exchange→UDP: XXX ns
Avg UDP→Queue: XXX ns  
Avg Queue→Strategy: XXX ns
Avg Total Latency: XXX ns
```

## 🎯 Use Cases

### Trading Systems
- Low-latency order book event ingestion
- Real-time limit order book reconstruction
- Market depth analysis and liquidity tracking
- Trade execution simulation

### Learning & Development
- C++ low-latency programming patterns
- Lock-free data structures
- Network programming with UDP
- Order book reconstruction algorithms
- Market microstructure concepts

## 📚 Technical Details

### Memory Management
- 64-byte aligned allocations
- RAII-compliant destructors
- No virtual functions for performance

### Thread Safety
- Single producer, single consumer design
- Atomic operations with relaxed memory ordering
- No locks in the data path

### Network Stack
- UDP sockets for connectionless communication
- Non-blocking I/O
- Efficient JSON parsing
- Multicast support for scalability

## 🤝 Contributing

This is an educational project demonstrating low-latency C++ systems and market microstructure. Feel free to experiment with optimizations and enhancements.

## 📄 License

Educational project - use and modify for learning purposes. 