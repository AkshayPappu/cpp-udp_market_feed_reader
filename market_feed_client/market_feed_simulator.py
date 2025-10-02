#!/usr/bin/env python3
"""
Market Data Feed Simulator - Multicast Version
Simulates a multicast market data feed with Level 2/3 order book events for testing the C++ Multicast Subscriber
"""

import socket
import time
import json
import random
import threading
import struct
from datetime import datetime
import argparse
from enum import Enum

class OrderBookEventType(Enum):
    ADD_ORDER = "ADD_ORDER"
    MODIFY_ORDER = "MODIFY_ORDER"
    CANCEL_ORDER = "CANCEL_ORDER"
    DELETE_ORDER = "DELETE_ORDER"
    TRADE = "TRADE"
    QUOTE_UPDATE = "QUOTE_UPDATE"
    MARKET_STATUS = "MARKET_STATUS"

class OrderSide(Enum):
    BID = "BID"
    ASK = "ASK"

class MarketDataSimulator:
    def __init__(self, multicast_group='224.0.0.1', port=12345, symbols=None, update_rate=100):
        self.multicast_group = multicast_group
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
        
        # Order book state for each symbol
        self.order_books = {}
        for symbol in self.symbols:
            self.order_books[symbol] = {
                'bids': {},  # price -> size
                'asks': {},  # price -> size
                'next_order_id': 1000
            }
        
        # Multicast socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        # Initialize multicast socket
        self._initialize_multicast()
    
    def _initialize_multicast(self):
        """Initialize the multicast socket for broadcasting market data"""
        try:
            # Set multicast TTL (Time To Live) - how many network hops the packet can make
            # TTL = 1 means local network only, TTL = 0 means same host only
            ttl = struct.pack('b', 1)  # TTL = 1 (local network only)
            self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)
            
            print(f"Multicast market feed initialized: {self.multicast_group}:{self.port}")
            
        except Exception as e:
            print(f"Failed to initialize multicast socket: {e}")
            raise
        
    def generate_order_book_event(self, symbol):
        """Generate a realistic order book event for a symbol"""
        book = self.order_books[symbol]
        current_price = self.current_prices[symbol]
        
        # Choose event type based on probability
        event_weights = {
            OrderBookEventType.ADD_ORDER: 0.4,
            OrderBookEventType.MODIFY_ORDER: 0.2,
            OrderBookEventType.CANCEL_ORDER: 0.2,
            OrderBookEventType.TRADE: 0.15,
            OrderBookEventType.QUOTE_UPDATE: 0.05
        }
        
        event_type = random.choices(
            list(event_weights.keys()),
            weights=list(event_weights.values())
        )[0]
        
        # Generate order ID
        order_id = f"{symbol}_{book['next_order_id']}"
        book['next_order_id'] += 1
        
        # Choose side
        side = random.choice([OrderSide.BID, OrderSide.ASK])
        
        # Timestamps
        timestamp = int(time.time() * 1_000_000_000)
        mono_ns = time.monotonic_ns()
        sequence = getattr(self, 'sequence', 0)
        self.sequence = sequence + 1
        
        event = {
            'event_type': event_type.value,
            'symbol': symbol,
            'exchange': 'SIM',
            'order_id': order_id,
            'side': side.value,
            'timestamp': timestamp,
            'sequence_number': sequence,
            'exchange_mono_ns': mono_ns
        }
        
        if event_type == OrderBookEventType.ADD_ORDER:
            # Add new order to book
            price_offset = random.uniform(-0.02, 0.02)  # ±2% from current price
            price = round(current_price * (1 + price_offset), 2)
            size = random.randint(100, 5000)
            
            event.update({
                'price': price,
                'size': size
            })
            
            # Update internal book state
            if side == OrderSide.BID:
                book['bids'][price] = book['bids'].get(price, 0) + size
            else:
                book['asks'][price] = book['asks'].get(price, 0) + size
                
        elif event_type == OrderBookEventType.MODIFY_ORDER:
            # Modify existing order
            if side == OrderSide.BID and book['bids']:
                price = random.choice(list(book['bids'].keys()))
                old_size = book['bids'][price]
                new_size = random.randint(100, old_size + 1000)
                event.update({
                    'price': price,
                    'size': new_size,
                    'remaining_size': old_size
                })
                book['bids'][price] = new_size
            elif side == OrderSide.ASK and book['asks']:
                price = random.choice(list(book['asks'].keys()))
                old_size = book['asks'][price]
                new_size = random.randint(100, old_size + 1000)
                event.update({
                    'price': price,
                    'size': new_size,
                    'remaining_size': old_size
                })
                book['asks'][price] = new_size
            else:
                # Fallback to ADD_ORDER if no orders exist
                return self.generate_order_book_event(symbol)
                
        elif event_type == OrderBookEventType.CANCEL_ORDER:
            # Cancel existing order
            if side == OrderSide.BID and book['bids']:
                price = random.choice(list(book['bids'].keys()))
                size = book['bids'][price]
                event.update({
                    'price': price,
                    'size': size
                })
                del book['bids'][price]
            elif side == OrderSide.ASK and book['asks']:
                price = random.choice(list(book['asks'].keys()))
                size = book['asks'][price]
                event.update({
                    'price': price,
                    'size': size
                })
                del book['asks'][price]
            else:
                # Fallback to ADD_ORDER if no orders exist
                return self.generate_order_book_event(symbol)
                
        elif event_type == OrderBookEventType.TRADE:
            # Simulate a trade
            trade_price = current_price * random.uniform(0.999, 1.001)
            trade_size = random.randint(100, 2000)
            is_aggressor = random.choice([True, False])
            
            event.update({
                'trade_price': round(trade_price, 2),
                'trade_size': trade_size,
                'is_aggressor': is_aggressor
            })
            
            # Update current price based on trade
            self.current_prices[symbol] = trade_price
            
        elif event_type == OrderBookEventType.QUOTE_UPDATE:
            # Generate top-of-book quote
            spread_pct = random.uniform(0.0001, 0.001)
            spread = current_price * spread_pct
            bid_price = round(current_price - spread / 2, 2)
            ask_price = round(current_price + spread / 2, 2)
            bid_size = random.randint(100, 10000)
            ask_size = random.randint(100, 10000)
            
            event.update({
                'bid_price': bid_price,
                'bid_size': bid_size,
                'ask_price': ask_price,
                'ask_size': ask_size
            })
        
        return event
    
    """Old function, no longer being used"""
    def generate_quote(self, symbol):
        """Generate a realistic quote for a symbol (legacy support)"""
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
    
    def send_order_book_event(self, event):
        """Send an order book event via multicast"""
        try:
            # Convert to JSON and encode
            data = json.dumps(event).encode('utf-8')
            
            # Send via multicast to all subscribers
            self.sock.sendto(data, (self.multicast_group, self.port))
            
            # Print what we're sending (for debugging)
            event_type = event['event_type']
            symbol = event['symbol']
            if event_type == 'ADD_ORDER':
                print(f"Sent: {symbol} {event_type} {event['side']} {event['price']}x{event['size']}")
            elif event_type == 'MODIFY_ORDER':
                print(f"Sent: {symbol} {event_type} {event['side']} {event['price']}x{event['size']} (was {event['remaining_size']})")
            elif event_type == 'CANCEL_ORDER':
                print(f"Sent: {symbol} {event_type} {event['side']} {event['price']}x{event['size']}")
            elif event_type == 'TRADE':
                print(f"Sent: {symbol} {event_type} {event['trade_price']}x{event['trade_size']} (aggressor: {event['is_aggressor']})")
            elif event_type == 'QUOTE_UPDATE':
                print(f"Sent: {symbol} {event_type} Bid: {event['bid_price']}x{event['bid_size']} Ask: {event['ask_price']}x{event['ask_size']}")
            else:
                print(f"Sent: {symbol} {event_type}")
                  
        except Exception as e:
            print(f"Error sending order book event: {e}")
    
    def send_quote(self, quote):
        """Send a quote via multicast (legacy support)"""
        try:
            # Convert to JSON and encode
            data = json.dumps(quote).encode('utf-8')
            
            # Send via multicast to all subscribers
            self.sock.sendto(data, (self.multicast_group, self.port))
            
            # Print what we're sending (for debugging)
            print(f"Sent: {quote['symbol']} Bid: {quote['bid_price']}x{quote['bid_size']} "
                  f"Ask: {quote['ask_price']}x{quote['ask_size']}")
                  
        except Exception as e:
            print(f"Error sending quote: {e}")
    
    def run_simulation(self):
        """Main simulation loop"""
        print(f"Starting multicast market data simulation on {self.multicast_group}:{self.port}")
        print(f"Symbols: {', '.join(self.symbols)}")
        print(f"Update rate: {self.update_rate} quotes/second")
        print("Broadcasting market data via multicast to all subscribers")
        print("Press Ctrl+C to stop")
        print("-" * 60)
        
        self.running = True
        update_interval = 1.0 / self.update_rate
        
        try:
            while self.running:
                start_time = time.time()
                
                # Generate and send order book events for all symbols
                for symbol in self.symbols:
                    event = self.generate_order_book_event(symbol)
                    self.send_order_book_event(event)
                
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
    parser = argparse.ArgumentParser(description='Multicast Market Data Feed Simulator')
    parser.add_argument('--multicast-group', default='224.0.0.1', help='Multicast group (default: 224.0.0.1)')
    parser.add_argument('--port', type=int, default=12345, help='Multicast port (default: 12345)')
    parser.add_argument('--rate', type=int, default=100, help='Order book events per second (default: 100)')
    parser.add_argument('--symbols', nargs='+', help='Custom symbols to simulate')
    
    args = parser.parse_args()
    
    # Create and start simulator
    simulator = MarketDataSimulator(
        multicast_group=args.multicast_group,
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