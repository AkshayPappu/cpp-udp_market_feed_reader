# Market Feed Client

This directory contains the Python market data simulator that generates realistic Level 2/3 order book events.

## Files

- `market_feed_simulator.py` - Python script that simulates market data feeds

## Usage

```bash
# Run with default settings (100 events/second)
python3 market_feed_simulator.py

# Run with custom rate
python3 market_feed_simulator.py --rate 50

# Run with specific symbols
python3 market_feed_simulator.py --symbols AAPL,MSFT,GOOGL

# Run with custom port
python3 market_feed_simulator.py --port 12345
```

## Features

- Generates realistic order book events (ADD_ORDER, MODIFY_ORDER, CANCEL_ORDER, TRADE, QUOTE_UPDATE)
- Maintains internal order book state for each symbol
- Sends events via UDP to the order book processor
- Configurable event rates and symbols
- JSON format compatible with the C++ processor

## Event Types

- **ADD_ORDER**: New orders added to the book
- **MODIFY_ORDER**: Existing orders modified (price/size changes)
- **CANCEL_ORDER**: Orders cancelled/removed
- **TRADE**: Order executions with aggressor information
- **QUOTE_UPDATE**: Top-of-book quote updates

## Dependencies

- Python 3.6+
- No external dependencies (uses only standard library)
