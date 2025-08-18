#!/bin/bash

echo "=== UDP Market Feed Test ==="
echo "This script will test your C++ UDP Quote Printer with a Python market data simulator"
echo ""

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is required but not installed"
    echo "Please install Python 3 and try again"
    exit 1
fi

# Check if the C++ program compiles
echo "1. Compiling C++ UDP Quote Printer..."
if make; then
    echo "✅ Compilation successful"
else
    echo "❌ Compilation failed"
    exit 1
fi

echo ""
echo "2. Starting Python Market Data Simulator in background..."
echo "   Simulator will send quotes to localhost:12345"
echo "   Press Ctrl+C in the simulator terminal to stop it"
echo ""

# Start Python simulator in background
python3 market_feed_simulator.py --rate 50 &
SIMULATOR_PID=$!

# Wait a moment for simulator to start and verify it's running
sleep 3
if ! kill -0 $SIMULATOR_PID 2>/dev/null; then
    echo "❌ Python simulator failed to start"
    exit 1
fi
echo "✅ Python simulator started successfully (PID: $SIMULATOR_PID)"

echo "3. Starting C++ UDP Quote Printer..."
echo "   This will receive and process the simulated market data"
echo "   Press Ctrl+C to stop the C++ program"
echo ""

# Start C++ program with timeout
timeout 30s ./udp_quote_printer || echo "C++ program stopped or timed out"

echo ""
echo "4. Cleaning up..."

# Stop Python simulator
if kill $SIMULATOR_PID 2>/dev/null; then
    echo "✅ Market data simulator stopped"
else
    echo "⚠️  Market data simulator already stopped"
fi

# Force kill any remaining processes
pkill -f "udp_quote_printer\|market_feed_simulator" 2>/dev/null || true
echo "✅ Cleaned up any remaining processes"

# Clean build artifacts
make clean

echo ""
echo "=== Test Complete ==="
echo "If you saw quotes being printed, your system is working correctly!" 