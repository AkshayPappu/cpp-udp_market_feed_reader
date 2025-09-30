#!/usr/bin/env python3
"""
Test script for the Order Book API
Demonstrates how to interact with the API endpoints
"""

import requests
import json
import time
import sys

API_BASE_URL = "http://localhost:8080"

def test_health():
    """Test the health endpoint"""
    print("Testing health endpoint...")
    try:
        response = requests.get(f"{API_BASE_URL}/api/health")
        if response.status_code == 200:
            data = response.json()
            print(f"✅ Health check passed: {data}")
            return True
        else:
            print(f"❌ Health check failed: {response.status_code}")
            return False
    except requests.exceptions.ConnectionError:
        print("❌ Cannot connect to API server. Is it running?")
        return False

def test_symbols():
    """Test the symbols endpoint"""
    print("\nTesting symbols endpoint...")
    try:
        response = requests.get(f"{API_BASE_URL}/api/symbols")
        if response.status_code == 200:
            data = response.json()
            symbols = data.get('symbols', [])
            print(f"✅ Found {len(symbols)} symbols: {symbols}")
            return symbols
        else:
            print(f"❌ Symbols request failed: {response.status_code}")
            return []
    except Exception as e:
        print(f"❌ Error getting symbols: {e}")
        return []

def test_metrics(symbol):
    """Test the metrics endpoint for a specific symbol"""
    print(f"\nTesting metrics endpoint for {symbol}...")
    try:
        response = requests.get(f"{API_BASE_URL}/api/metrics/{symbol}")
        if response.status_code == 200:
            data = response.json()
            print(f"✅ Metrics for {symbol}:")
            print(f"   Best Bid: {data.get('best_bid_price', 0):.2f} x {data.get('best_bid_size', 0)}")
            print(f"   Best Ask: {data.get('best_ask_price', 0):.2f} x {data.get('best_ask_size', 0)}")
            print(f"   Spread: {data.get('spread', 0):.4f}")
            print(f"   Midprice: {data.get('midprice', 0):.2f}")
            print(f"   Quote Imbalance: {data.get('quote_imbalance', 0):.4f}")
            print(f"   Events Processed: {data.get('total_events_processed', 0)}")
            return True
        else:
            print(f"❌ Metrics request failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"❌ Error getting metrics: {e}")
        return False

def test_depth(symbol):
    """Test the depth endpoint for a specific symbol"""
    print(f"\nTesting depth endpoint for {symbol}...")
    try:
        response = requests.get(f"{API_BASE_URL}/api/depth/{symbol}")
        if response.status_code == 200:
            data = response.json()
            print(f"✅ Depth for {symbol}:")
            
            bid_depth = data.get('bid_depth', [])
            ask_depth = data.get('ask_depth', [])
            
            print("   Bid Depth:")
            for level in bid_depth:
                print(f"     {level['price']:.2f} x {level['size']}")
            
            print("   Ask Depth:")
            for level in ask_depth:
                print(f"     {level['price']:.2f} x {level['size']}")
            
            return True
        else:
            print(f"❌ Depth request failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"❌ Error getting depth: {e}")
        return False

def test_trades(symbol):
    """Test the trades endpoint for a specific symbol"""
    print(f"\nTesting trades endpoint for {symbol}...")
    try:
        response = requests.get(f"{API_BASE_URL}/api/trades/{symbol}")
        if response.status_code == 200:
            data = response.json()
            print(f"✅ Last trade for {symbol}:")
            print(f"   Price: {data.get('price', 0):.2f}")
            print(f"   Size: {data.get('size', 0)}")
            print(f"   Aggressor: {data.get('aggressor_side', 'Unknown')}")
            print(f"   Timestamp: {data.get('timestamp', 0)}")
            return True
        else:
            print(f"❌ Trades request failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"❌ Error getting trades: {e}")
        return False

def monitor_metrics(symbol, duration=30):
    """Monitor metrics for a symbol over time"""
    print(f"\nMonitoring metrics for {symbol} for {duration} seconds...")
    print("Press Ctrl+C to stop monitoring")
    
    try:
        start_time = time.time()
        while time.time() - start_time < duration:
            response = requests.get(f"{API_BASE_URL}/api/metrics/{symbol}")
            if response.status_code == 200:
                data = response.json()
                timestamp = time.strftime("%H:%M:%S")
                print(f"[{timestamp}] {symbol}: "
                      f"Bid={data.get('best_bid_price', 0):.2f} "
                      f"Ask={data.get('best_ask_price', 0):.2f} "
                      f"Spread={data.get('spread', 0):.4f} "
                      f"Events={data.get('total_events_processed', 0)}")
            else:
                print(f"❌ Failed to get metrics: {response.status_code}")
            
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nMonitoring stopped by user")

def main():
    """Main test function"""
    print("=== Order Book API Test Suite ===")
    print(f"Testing API at: {API_BASE_URL}")
    print()
    
    # Test health
    if not test_health():
        print("\n❌ API server is not running or not accessible")
        print("Please start the API server first:")
        print("  cd order_book_api")
        print("  make deps && make")
        print("  ./order_book_api")
        sys.exit(1)
    
    # Test symbols
    symbols = test_symbols()
    if not symbols:
        print("\n⚠️  No symbols available. Start the market data simulator and order book processor.")
        print("The API is running but no data is available yet.")
        return
    
    # Test each symbol
    for symbol in symbols[:3]:  # Test first 3 symbols
        test_metrics(symbol)
        test_depth(symbol)
        test_trades(symbol)
    
    # Interactive monitoring
    if len(sys.argv) > 1 and sys.argv[1] == "monitor":
        if symbols:
            monitor_metrics(symbols[0], 60)
        else:
            print("No symbols available for monitoring")
    
    print("\n✅ All tests completed!")

if __name__ == "__main__":
    main()
