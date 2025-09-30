#!/bin/bash

echo "=== UDP Order Book Processor Test ==="
echo "Testing C++ UDP Order Book Processor with Python market data simulator"
echo ""

# Test configuration
TEST_DURATION=10
SIMULATOR_RATE=20
EXPECTED_MIN_EVENTS=50

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: Python 3 is required but not installed"
    exit 1
fi

# Check if required files exist
if [[ ! -f "../market_feed_client/market_feed_simulator.py" ]]; then
    echo "❌ Error: market_feed_simulator.py not found in ../market_feed_client/"
    exit 1
fi

# Clean any existing processes
pkill -f "udp_quote_printer\|market_feed_simulator" 2>/dev/null || true

# Compile the C++ program
echo "1. Compiling C++ UDP Order Book Processor..."
if make clean && make; then
    echo "✅ Compilation successful"
else
    echo "❌ Compilation failed"
    exit 1
fi

# Start Python simulator in background
echo "2. Starting Python Market Data Simulator..."
python3 ../market_feed_client/market_feed_simulator.py --rate $SIMULATOR_RATE > /dev/null 2>&1 &
SIMULATOR_PID=$!

# Wait for simulator to start
sleep 2
if ! kill -0 $SIMULATOR_PID 2>/dev/null; then
    echo "❌ Python simulator failed to start"
    exit 1
fi
echo "✅ Python simulator started (PID: $SIMULATOR_PID)"

# Run the C++ program and capture output
echo "3. Running C++ UDP Order Book Processor for ${TEST_DURATION}s..."
echo "   (Output will be captured and analyzed)"

# Create temporary file for output
TEMP_OUTPUT=$(mktemp)

# Run the C++ program with timeout and capture output
timeout ${TEST_DURATION}s ./udp_quote_printer > "$TEMP_OUTPUT" 2>&1
CPP_EXIT_CODE=$?

# Stop the simulator
kill $SIMULATOR_PID 2>/dev/null || true
sleep 1

# Analyze the output
echo "4. Analyzing results..."

# Count order book events received
EVENT_COUNT=$(grep -c "ORDER BOOK EVENT RECEIVED" "$TEMP_OUTPUT" 2>/dev/null)
EVENT_COUNT=${EVENT_COUNT:-0}

# Check for compilation warnings
WARNING_COUNT=$(grep -c "warning:" "$TEMP_OUTPUT" 2>/dev/null)
WARNING_COUNT=${WARNING_COUNT:-0}

# Check for errors (excluding the expected "Error parsing" messages from the program)
ERROR_COUNT=$(grep -c "Error parsing order book event" "$TEMP_OUTPUT" 2>/dev/null)
ERROR_COUNT=${ERROR_COUNT:-0}

# Check for performance statistics
STATS_FOUND=$(grep -c "PERFORMANCE STATISTICS" "$TEMP_OUTPUT" 2>/dev/null)
STATS_FOUND=${STATS_FOUND:-0}

# Check for critical errors
CRITICAL_ERRORS=$(grep -c "FATAL\|CRITICAL\|SEGFAULT\|Aborted" "$TEMP_OUTPUT" 2>/dev/null)
CRITICAL_ERRORS=${CRITICAL_ERRORS:-0}

# Display results
echo ""
echo "=== Test Results ==="
echo "Events received: $EVENT_COUNT"
echo "Expected minimum: $EXPECTED_MIN_EVENTS"
echo "Compilation warnings: $WARNING_COUNT"
echo "Parsing errors: $ERROR_COUNT"
echo "Critical errors: $CRITICAL_ERRORS"
echo "Performance stats: $STATS_FOUND"

# Determine test success
SUCCESS=true

if [ "$EVENT_COUNT" -lt "$EXPECTED_MIN_EVENTS" ]; then
    echo "❌ FAIL: Too few events received ($EVENT_COUNT < $EXPECTED_MIN_EVENTS)"
    SUCCESS=false
fi

if [ "$CRITICAL_ERRORS" -gt 0 ]; then
    echo "❌ FAIL: Critical errors found in output"
    SUCCESS=false
fi

if [ "$CPP_EXIT_CODE" -ne 124 ] && [ "$CPP_EXIT_CODE" -ne 0 ]; then
    echo "❌ FAIL: C++ program exited with code $CPP_EXIT_CODE"
    SUCCESS=false
fi

if [ "$STATS_FOUND" -eq 0 ]; then
    echo "❌ FAIL: No performance statistics found"
    SUCCESS=false
fi

# Clean up
rm -f "$TEMP_OUTPUT"
pkill -f "udp_quote_printer\|market_feed_simulator" 2>/dev/null || true

echo ""
if [ "$SUCCESS" = true ]; then
    echo "✅ TEST PASSED: UDP Order Book Processor is working correctly!"
    echo "   - Received $EVENT_COUNT order book events"
    echo "   - No errors detected"
    echo "   - Performance statistics generated"
    exit 0
else
    echo "❌ TEST FAILED: Issues detected with the UDP Order Book Processor"
    echo "   Check the output above for details"
    exit 1
fi