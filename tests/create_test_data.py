"""
Simple test script to verify the order book implementation.
Creates a mock event file and processes it with the C++ engine.
"""

import os
import time

def create_test_events(filename: str = "./data/test_events.events", num_events: int = 1000):
    """Create a test event file with synthetic order book data."""
    
    os.makedirs("./data", exist_ok=True)
    
    with open(filename, 'w') as f:
        base_price = 42000.0
        timestamp = int(time.time() * 1000)
        
        # Initial snapshot
        for i in range(10):
            bid_price = base_price - (i + 1) * 10
            ask_price = base_price + (i + 1) * 10
            qty = 0.5 + i * 0.1
            
            f.write(f"{timestamp}|{timestamp}|SNAPSHOT|{bid_price}|{qty}|BID\n")
            f.write(f"{timestamp}|{timestamp}|SNAPSHOT|{ask_price}|{qty}|ASK\n")
        
        # Updates
        for i in range(num_events):
            timestamp += 100  # 100ms increments
            
            # Random price movement
            import random
            side = random.choice(['BID', 'ASK'])
            offset = random.randint(1, 5) * 10
            
            if side == 'BID':
                price = base_price - offset
            else:
                price = base_price + offset
            
            qty = random.uniform(0.1, 2.0)
            
            f.write(f"{timestamp}|{timestamp}|UPDATE|{price}|{qty}|{side}\n")
            
            # Occasionally move base price
            if i % 100 == 0:
                base_price += random.uniform(-50, 50)
    
    print(f"[INFO] Created test event file: {filename}")
    print(f"[INFO] Total events: {num_events + 20}")
    return filename


if __name__ == "__main__":
    print("=== Order Book Test Setup ===")
    test_file = create_test_events(num_events=5000)
    print(f"\n[INFO] Test file ready: {test_file}")
    print("\n[NEXT] Build and run the C++ engine:")
    print("  cd engine/build")
    print(f"  ./market_engine ../../{test_file}")
