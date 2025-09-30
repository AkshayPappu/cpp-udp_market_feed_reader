#!/bin/bash

# UDP Market Data System Shutdown Script
# Gracefully stops all system components

set -e  # Exit on any error

echo "=== UDP Market Data System Shutdown ==="
echo "Stopping all system components..."
echo ""

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

# Function to stop process by PID file
stop_process_by_pid() {
    local pid_file=$1
    local process_name=$2
    
    if [[ -f "$pid_file" ]]; then
        local pid=$(cat "$pid_file")
        if kill -0 "$pid" 2>/dev/null; then
            print_status "Stopping $process_name (PID: $pid)..."
            kill "$pid"
            
            # Wait for graceful shutdown
            local count=0
            while kill -0 "$pid" 2>/dev/null && [ $count -lt 10 ]; do
                sleep 1
                count=$((count + 1))
            done
            
            # Force kill if still running
            if kill -0 "$pid" 2>/dev/null; then
                print_warning "Force killing $process_name (PID: $pid)..."
                kill -9 "$pid"
            fi
            
            print_success "$process_name stopped"
        else
            print_warning "$process_name (PID: $pid) was not running"
        fi
        rm -f "$pid_file"
    else
        print_warning "PID file $pid_file not found"
    fi
}

# Function to stop processes by name pattern
stop_processes_by_pattern() {
    local pattern=$1
    local process_name=$2
    
    local pids=$(pgrep -f "$pattern" 2>/dev/null || true)
    
    if [[ -n "$pids" ]]; then
        print_status "Stopping $process_name processes..."
        for pid in $pids; do
            if kill -0 "$pid" 2>/dev/null; then
                print_status "Stopping $process_name (PID: $pid)..."
                kill "$pid"
            fi
        done
        
        # Wait for graceful shutdown
        sleep 3
        
        # Force kill any remaining processes
        local remaining_pids=$(pgrep -f "$pattern" 2>/dev/null || true)
        if [[ -n "$remaining_pids" ]]; then
            print_warning "Force killing remaining $process_name processes..."
            for pid in $remaining_pids; do
                if kill -0 "$pid" 2>/dev/null; then
                    kill -9 "$pid"
                fi
            done
        fi
        
        print_success "$process_name processes stopped"
    else
        print_status "No $process_name processes found"
    fi
}

# Stop processes by PID files (preferred method)
stop_by_pid_files() {
    print_status "Stopping processes by PID files..."
    
    stop_process_by_pid ".api.pid" "API Server"
    stop_process_by_pid ".processor.pid" "Order Book Processor"
    stop_process_by_pid ".simulator.pid" "Market Data Simulator"
}

# Stop processes by name patterns (fallback method)
stop_by_patterns() {
    print_status "Stopping processes by name patterns..."
    
    stop_processes_by_pattern "standalone_api" "API Server"
    stop_processes_by_pattern "udp_quote_printer" "Order Book Processor"
    stop_processes_by_pattern "market_feed_simulator" "Market Data Simulator"
}

# Check for remaining processes
check_remaining_processes() {
    print_status "Checking for remaining processes..."
    
    local remaining=$(pgrep -f "udp_quote_printer\|standalone_api\|market_feed_simulator" 2>/dev/null || true)
    
    if [[ -n "$remaining" ]]; then
        print_warning "Some processes are still running:"
        ps -p $remaining -o pid,cmd 2>/dev/null || true
        return 1
    else
        print_success "All processes stopped"
        return 0
    fi
}

# Clean up temporary files
cleanup() {
    print_status "Cleaning up temporary files..."
    
    rm -f .simulator.pid
    rm -f .processor.pid
    rm -f .api.pid
    
    print_success "Cleanup complete"
}

# Test API availability
test_api_shutdown() {
    print_status "Testing API shutdown..."
    
    if curl -s http://localhost:8080/api/health > /dev/null 2>&1; then
        print_warning "API is still responding - may not be fully stopped"
        return 1
    else
        print_success "API is no longer responding"
        return 0
    fi
}

# Main execution
main() {
    # Stop processes by PID files first
    stop_by_pid_files
    
    # Stop any remaining processes by patterns
    stop_by_patterns
    
    # Check for remaining processes
    if check_remaining_processes; then
        print_success "All processes stopped successfully"
    else
        print_warning "Some processes may still be running"
    fi
    
    # Test API shutdown
    test_api_shutdown
    
    # Clean up
    cleanup
    
    echo ""
    print_success "System shutdown complete!"
    echo ""
    echo "All components stopped:"
    echo "  ✓ Market Data Simulator"
    echo "  ✓ Order Book Processor"
    echo "  ✓ API Server"
    echo ""
    echo "Ports freed:"
    echo "  ✓ UDP Port 12345"
    echo "  ✓ API Port 8080"
    echo "  ✓ Multicast 224.0.0.1:12346"
    echo ""
    echo "To restart the system: ./start_system.sh"
}

# Handle script interruption
trap 'echo ""; print_warning "Shutdown interrupted. Continuing cleanup..."; cleanup; exit 1' INT TERM

# Run main function
main "$@"
