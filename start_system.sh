#!/bin/bash

# UDP Market Data System Startup Script
# Starts the complete decoupled system with UDP multicast

set -e  # Exit on any error

echo "=== UDP Market Data System Startup ==="
echo "Starting decoupled system with UDP multicast..."
echo ""

# Configuration
MARKET_DATA_RATE=50
UDP_PORT=12345
API_PORT=8080
MULTICAST_GROUP="224.0.0.1"
MULTICAST_PORT=12346

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if processes are already running
check_running_processes() {
    if pgrep -f "udp_quote_printer\|standalone_api\|market_feed_simulator" > /dev/null; then
        print_warning "Some processes are already running. Stopping them first..."
        ./shutdown_system.sh
        sleep 2
    fi
}

# Check prerequisites
check_prerequisites() {
    print_status "Checking prerequisites..."
    
    # Check if Python 3 is available
    if ! command -v python3 &> /dev/null; then
        print_error "Python 3 is required but not installed"
        exit 1
    fi
    
    # Check if required files exist
    if [[ ! -f "market_feed_client/market_feed_simulator.py" ]]; then
        print_error "Market data simulator not found"
        exit 1
    fi
    
    if [[ ! -f "order_book_processor/udp_quote_printer" ]]; then
        print_error "Order book processor not found. Please build it first:"
        print_error "  cd order_book_processor && make"
        exit 1
    fi
    
    if [[ ! -f "order_book_api/standalone_api" ]]; then
        print_error "Standalone API server not found. Please build it first:"
        print_error "  cd order_book_api && make standalone_api"
        exit 1
    fi
    
    print_success "All prerequisites met"
}

# Start market data simulator
start_market_data_simulator() {
    print_status "Starting Market Data Simulator..."
    cd market_feed_client
    python3 market_feed_simulator.py --rate $MARKET_DATA_RATE > /dev/null 2>&1 &
    SIMULATOR_PID=$!
    cd ..
    
    # Wait for simulator to start
    sleep 2
    if ! kill -0 $SIMULATOR_PID 2>/dev/null; then
        print_error "Market data simulator failed to start"
        exit 1
    fi
    
    print_success "Market Data Simulator started (PID: $SIMULATOR_PID)"
    echo $SIMULATOR_PID > .simulator.pid
}

# Start order book processor
start_order_book_processor() {
    print_status "Starting Order Book Processor (Publisher)..."
    cd order_book_processor
    ./udp_quote_printer > /dev/null 2>&1 &
    PROCESSOR_PID=$!
    cd ..
    
    # Wait for processor to start
    sleep 3
    if ! kill -0 $PROCESSOR_PID 2>/dev/null; then
        print_error "Order book processor failed to start"
        exit 1
    fi
    
    print_success "Order Book Processor started (PID: $PROCESSOR_PID)"
    echo $PROCESSOR_PID > .processor.pid
}

# Start standalone API server
start_api_server() {
    print_status "Starting Standalone API Server (Subscriber)..."
    cd order_book_api
    ./standalone_api > /dev/null 2>&1 &
    API_PID=$!
    cd ..
    
    # Wait for API server to start
    sleep 3
    if ! kill -0 $API_PID 2>/dev/null; then
        print_error "API server failed to start"
        exit 1
    fi
    
    # Test API health
    sleep 2
    if curl -s http://localhost:$API_PORT/api/health > /dev/null; then
        print_success "API Server started and responding (PID: $API_PID)"
    else
        print_warning "API Server started but not responding yet (PID: $API_PID)"
    fi
    
    echo $API_PID > .api.pid
}

# Test system functionality
test_system() {
    print_status "Testing system functionality..."
    
    # Wait for data to flow
    sleep 5
    
    # Test API health
    if curl -s http://localhost:$API_PORT/api/health > /dev/null; then
        print_success "API health check passed"
    else
        print_warning "API health check failed"
    fi
    
    # Test symbols endpoint
    SYMBOLS=$(curl -s http://localhost:$API_PORT/api/symbols | grep -o '\[.*\]' | wc -c)
    if [ $SYMBOLS -gt 10 ]; then
        print_success "Symbols endpoint working (found symbols)"
    else
        print_warning "Symbols endpoint not returning data yet"
    fi
}

# Main execution
main() {
    echo "Configuration:"
    echo "  Market Data Rate: $MARKET_DATA_RATE events/sec"
    echo "  UDP Port: $UDP_PORT"
    echo "  API Port: $API_PORT"
    echo "  Multicast: $MULTICAST_GROUP:$MULTICAST_PORT"
    echo ""
    
    check_running_processes
    check_prerequisites
    
    start_market_data_simulator
    start_order_book_processor
    start_api_server
    
    test_system
    
    echo ""
    print_success "System startup complete!"
    echo ""
    echo "System Status:"
    echo "  Market Data Simulator: Running (PID: $(cat .simulator.pid 2>/dev/null || echo 'Unknown'))"
    echo "  Order Book Processor: Running (PID: $(cat .processor.pid 2>/dev/null || echo 'Unknown'))"
    echo "  API Server: Running (PID: $(cat .api.pid 2>/dev/null || echo 'Unknown'))"
    echo ""
    echo "API Endpoints:"
    echo "  Health: curl http://localhost:$API_PORT/api/health"
    echo "  Symbols: curl http://localhost:$API_PORT/api/symbols"
    echo "  Metrics: curl http://localhost:$API_PORT/api/metrics/AAPL"
    echo ""
    echo "To stop the system: ./shutdown_system.sh"
    echo "To view logs: tail -f /dev/null (processes run in background)"
}

# Handle script interruption
trap 'echo ""; print_warning "Startup interrupted. Cleaning up..."; ./shutdown_system.sh; exit 1' INT TERM

# Run main function
main "$@"
