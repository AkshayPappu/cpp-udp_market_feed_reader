# UDP Quote Printer - Low-Latency Market Data System

A high-performance C++ system for receiving, processing, and analyzing real-time market data quotes via UDP with sub-microsecond latency measurement capabilities.

## 🚀 Features

### Core Functionality
- **UDP Market Data Reception**: Listens on configurable port for incoming market data quotes
- **Lock-Free SPSC Ring Buffer**: Single Producer Single Consumer ring buffer for zero-copy inter-thread communication
- **Producer-Consumer Architecture**: Decoupled data ingestion and processing for optimal performance
- **Real-Time Latency Measurement**: End-to-end latency tracking from exchange to strategy execution

### Performance Optimizations
- **Cache-Aligned Memory**: 64-byte aligned allocations to prevent false sharing
- **Lock-Free Operations**: Atomic operations with relaxed memory ordering for minimal contention
- **Power-of-2 Buffer Sizes**: Efficient modulo operations using bitwise AND
- **Single Writer Rule**: Only consumer thread prints/logs to avoid output contention
- **Monotonic Clock Pipeline**: Consistent timing using `std::chrono::steady_clock` throughout

### Latency Breakdown
The system measures and reports detailed latency metrics:
- **Exchange → UDP Receive**: Network transmission time
- **UDP Receive → Queue**: Time to parse and enqueue quote
- **Queue → Strategy**: Time from queue to consumer processing
- **Total End-to-End**: Complete latency from exchange to strategy execution

### Market Data Support
- **JSON Quote Format**: Parses standard market data fields (symbol, bid/ask prices & sizes, exchange, timestamp)
- **Flexible Exchange Support**: Configurable for different market data feeds
- **Quote Validation**: Robust parsing with whitespace tolerance

## 🏗️ Architecture

### Components

#### UDPListener (`listener.hpp`)
- **Header-only implementation** for easy integration
- **Non-blocking socket operations** with `MSG_DONTWAIT`
- **Configurable port binding** with socket reuse
- **Callback-based quote processing** for flexible integration

#### SPSCRingBuffer (`queue.hpp`)
- **Template-based design** supporting any data type
- **Lock-free operations** using atomic sequence numbers
- **Cache-aligned memory layout** for optimal performance
- **Power-of-2 capacity** for efficient modulo operations

#### Main Application (`main.cpp`)
- **Producer thread** (`ingress_producer`): Receives UDP data and enqueues quotes
- **Consumer thread** (`print_consumer`): Processes quotes and displays latency metrics
- **Graceful shutdown** handling with signal management
- **Performance statistics** aggregation and reporting

### Threading Model
```
[UDP Socket] → [Producer Thread] → [SPSC Ring Buffer] → [Consumer Thread] → [Output]
     ↓              ↓                      ↓                ↓
  Network      Parse & Enqueue        Lock-free         Process &
  Receive      (No Printing)         Communication     Print (Only)
```

## 📊 Performance Characteristics

### Latency Targets
- **UDP → Queue**: < 100 nanoseconds typical
- **Queue → Strategy**: < 50 microseconds typical  
- **Total End-to-End**: < 200 microseconds typical

### Throughput
- **Designed for**: High-frequency market data (1000+ quotes/second)
- **Buffer capacity**: Configurable (default: power-of-2 sizing)
- **Memory usage**: Minimal overhead with aligned allocations

## 🛠️ Build & Run

### Prerequisites
- **Compiler**: GCC 7+ with C++17 support
- **OS**: Linux (tested on RHEL 9)
- **Python**: 3.7+ (for market data simulator)

### Build
```bash
make                    # Build release version
make debug             # Build with debug symbols
make clean             # Clean build artifacts
```

### Run
```bash
# Terminal 1: Start market data simulator
python3 market_feed_simulator.py

# Terminal 2: Run UDP quote printer
./udp_quote_printer

# Or use the automated test script
./test_market_feed.sh
```

## 🧪 Testing & Simulation

### Market Data Simulator (`market_feed_simulator.py`)
- **Realistic quote generation** with price movements and spreads
- **Configurable symbols** (AAPL, MSFT, GOOGL, etc.)
- **Monotonic timestamps** aligned with C++ timing
- **UDP transmission** to localhost:12345

### Test Script (`test_market_feed.sh`)
- **Automated testing** with compilation and execution
- **Background simulator** management
- **Cleanup handling** for development workflow

## 📈 Monitoring & Metrics

### Real-Time Output
- **Per-quote latency breakdown** with microsecond precision
- **Performance statistics** every 10 quotes
- **Producer metrics** (quotes pushed, dropped, average push latency)

### Performance Statistics
```
📊 PERFORMANCE STATISTICS (Last N quotes):
Avg Exchange→UDP: XXX ns
Avg UDP→Queue: XXX ns  
Avg Queue→Strategy: XXX ns
Avg Total Latency: XXX ns
Producer Stats - Pushed: [tracked], Dropped: [tracked], Avg Push Latency: [tracked]
```

## 🔧 Configuration

### UDP Settings
- **Port**: 12345 (configurable in `UDPListener` constructor)
- **Bind Address**: `INADDR_ANY` (all interfaces)
- **Socket Options**: `SO_REUSEADDR` enabled

### Buffer Configuration
- **Default Capacity**: Power-of-2 sizing for optimal performance
- **Memory Alignment**: 64-byte cache line alignment
- **Overflow Handling**: Drop quotes when buffer is full

## 🎯 Use Cases

### Trading Systems
- **Low-latency market data ingestion**
- **Real-time quote processing**
- **Performance benchmarking**
- **System optimization research**

### Learning & Development
- **C++ low-latency programming patterns**
- **Lock-free data structures**
- **Network programming with UDP**
- **Performance measurement techniques**

## 🚧 Future Enhancements

### Planned Features
- **Percentile latency reporting** (P50, P99, P99.9)
- **Batch processing** for improved throughput
- **Multiple UDP ports** support
- **Configurable buffer sizes** via command line
- **Performance profiling** integration

### Optimization Opportunities
- **CPU pinning** for producer/consumer threads
- **NUMA-aware memory allocation**
- **Kernel bypass** networking (DPDK, Solarflare)
- **SIMD optimizations** for quote parsing

## 📚 Technical Details

### Memory Management
- **Aligned allocations** using `aligned_alloc(64, size)`
- **RAII-compliant** destructors for automatic cleanup
- **No virtual functions** to maintain performance

### Thread Safety
- **Single producer, single consumer** design
- **Atomic operations** with relaxed memory ordering
- **No locks or mutexes** in the data path

### Network Stack
- **UDP sockets** for connectionless communication
- **Non-blocking I/O** to prevent producer blocking
- **Efficient packet parsing** with minimal allocations

## 🤝 Contributing

This project serves as a learning platform for low-latency C++ systems. Feel free to:
- **Experiment** with different optimization strategies
- **Add new features** like additional market data formats
- **Improve performance** through profiling and optimization
- **Share insights** about low-latency programming techniques

## 📄 License

Educational project - feel free to use and modify for learning purposes. 