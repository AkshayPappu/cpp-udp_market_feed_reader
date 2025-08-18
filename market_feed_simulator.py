#!/usr/bin/env python3
"""
Market Data Feed Simulator
Simulates a UDP market data feed for testing the C++ UDP Quote Printer
"""

import socket
import time
import json
import random
import threading
from datetime import datetime
import argparse

class MarketDataSimulator:
    def __init__(self, host='127.0.0.1', port=12345, symbols=None, update_rate=100):
        self.host = host
        self.port = port
        self.update_rate = update_rate  # updates per second
        self.running = False
        
        # Default symbols to simulate
        self.symbols = symbols or [
            'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'TSLA', 
            'NVDA', 'META', 'NFLX', 'AMD', 'INTC'
        ]
        
        # Base prices for each symbol
        self.base_prices = {
            'AAPL': 150.0, 'MSFT': 300.0, 'GOOGL': 2800.0, 'AMZN': 3200.0, 'TSLA': 800.0,
            'NVDA': 400.0, 'META': 200.0, 'NFLX': 500.0, 'AMD': 100.0, 'INTC': 50.0
        }
        
        # Current prices (will be updated)
        self.current_prices = self.base_prices.copy()
        
        # UDP socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
    def generate_quote(self, symbol):
        """Generate a realistic quote for a symbol"""
        base_price = self.base_prices[symbol]
        
        # Simulate price movement (random walk with mean reversion)
        price_change = random.gauss(0, base_price * 0.001)  # 0.1% volatility
        self.current_prices[symbol] += price_change
        
        # Mean reversion - pull price back toward base
        reversion = (base_price - self.current_prices[symbol]) * 0.01
        self.current_prices[symbol] += reversion
        
        current_price = self.current_prices[symbol]
        
        # Generate bid/ask spread (typically 0.01% to 0.1%)
        spread_pct = random.uniform(0.0001, 0.001)
        spread = current_price * spread_pct
        
        bid_price = current_price - spread / 2
        ask_price = current_price + spread / 2
        
        # Generate realistic sizes (100 to 10000 shares)
        bid_size = random.randint(100, 10000)
        ask_size = random.randint(100, 10000)
        
        # Timestamp in nanoseconds
        timestamp = int(time.time() * 1_000_000_000)
        
        # Monotonic timestamp (ns) to align with C++ steady_clock for latency
        mono_ns = time.monotonic_ns()

        quote = {
            'symbol': symbol,
            'bid_price': round(bid_price, 4),
            'bid_size': bid_size,
            'ask_price': round(ask_price, 4),
            'ask_size': ask_size,
            'timestamp': timestamp,   # wall clock (for display only)
            'exchange': 'SIM',
            'sequence': getattr(self, 'sequence', 0),
            'exchange_mono_ns': mono_ns
        }
        
        self.sequence = getattr(self, 'sequence', 0) + 1
        return quote
    
    def send_quote(self, quote):
        """Send a quote as UDP packet"""
        try:
            # Convert to JSON and encode
            data = json.dumps(quote).encode('utf-8')
            
            # Send to C++ application
            self.sock.sendto(data, (self.host, self.port))
            
            # Print what we're sending (for debugging)
            print(f"Sent: {quote['symbol']} Bid: {quote['bid_price']}x{quote['bid_size']} "
                  f"Ask: {quote['ask_price']}x{quote['ask_size']}")
                  
        except Exception as e:
            print(f"Error sending quote: {e}")
    
    def run_simulation(self):
        """Main simulation loop"""
        print(f"Starting market data simulation on {self.host}:{self.port}")
        print(f"Symbols: {', '.join(self.symbols)}")
        print(f"Update rate: {self.update_rate} quotes/second")
        print("Press Ctrl+C to stop")
        print("-" * 60)
        
        self.running = True
        update_interval = 1.0 / self.update_rate
        
        try:
            while self.running:
                start_time = time.time()
                
                # Generate and send quotes for all symbols
                for symbol in self.symbols:
                    quote = self.generate_quote(symbol)
                    self.send_quote(quote)
                
                # Calculate sleep time to maintain update rate
                elapsed = time.time() - start_time
                sleep_time = max(0, update_interval - elapsed)
                
                if sleep_time > 0:
                    time.sleep(sleep_time)
                    
        except KeyboardInterrupt:
            print("\nShutting down simulation...")
            self.running = False
    
    def start(self):
        """Start the simulation in a separate thread"""
        self.simulation_thread = threading.Thread(target=self.run_simulation)
        self.simulation_thread.daemon = False  # Changed from True to False
        self.simulation_thread.start()
    
    def stop(self):
        """Stop the simulation"""
        self.running = False
        if hasattr(self, 'simulation_thread'):
            self.simulation_thread.join(timeout=1)
        self.sock.close()

def main():
    parser = argparse.ArgumentParser(description='Market Data Feed Simulator')
    parser.add_argument('--host', default='127.0.0.1', help='UDP host (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=12345, help='UDP port (default: 12345)')
    parser.add_argument('--rate', type=int, default=100, help='Quotes per second (default: 100)')
    parser.add_argument('--symbols', nargs='+', help='Custom symbols to simulate')
    
    args = parser.parse_args()
    
    # Create and start simulator
    simulator = MarketDataSimulator(
        host=args.host,
        port=args.port,
        symbols=args.symbols,
        update_rate=args.rate
    )
    
    try:
        simulator.start()
        
        # Keep main thread alive
        while simulator.running:
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\nShutting down...")
        simulator.stop()

if __name__ == '__main__':
    main() 