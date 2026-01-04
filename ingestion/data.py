"""
Binance Data Ingestion Layer
Fetches historical data via REST API and streams real-time data via WebSocket.
Normalizes and writes events to disk in the format:
[exchange_ts]|[local_ts]|[event_type]|[price]|[qty]|[side]
"""

import asyncio
import json
import time
import websockets
import requests
from datetime import datetime
from pathlib import Path
from typing import Optional


class BinanceDataIngestion:
    """
    Handles both historical (REST) and real-time (WebSocket) data ingestion from Binance.
    """
    
    def __init__(self, symbol: str = "BTCUSDT", output_dir: str = "./data"):
        self.symbol = symbol
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Binance endpoints
        self.rest_base_url = "https://api.binance.com"
        self.ws_base_url = "wss://stream.binance.com:9443"
        
        # Output file
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.output_file = self.output_dir / f"{timestamp}-{symbol}.events"
        self.file_handle = None
        
    def fetch_historical_depth(self, limit: int = 1000) -> bool:
        """
        Fetch historical order book snapshot via REST API.
        
        Args:
            limit: Number of price levels to fetch (max 5000)
            
        Returns:
            True if successful, False otherwise
        """
        try:
            url = f"{self.rest_base_url}/api/v3/depth"
            params = {"symbol": self.symbol, "limit": limit}
            
            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()
            data = response.json()
            
            # Open file for writing
            self.file_handle = open(self.output_file, 'a', buffering=1)
            
            exchange_ts = data.get('lastUpdateId', 0)
            local_ts = int(time.time() * 1000)
            
            # Write bids
            for price, qty in data.get('bids', []):
                event_line = f"{exchange_ts}|{local_ts}|SNAPSHOT|{price}|{qty}|BID\n"
                self.file_handle.write(event_line)
            
            # Write asks
            for price, qty in data.get('asks', []):
                event_line = f"{exchange_ts}|{local_ts}|SNAPSHOT|{price}|{qty}|ASK\n"
                self.file_handle.write(event_line)
            
            print(f"[INFO] Historical snapshot written: {len(data.get('bids', []))} bids, {len(data.get('asks', []))} asks")
            return True
            
        except Exception as e:
            print(f"[ERROR] Failed to fetch historical data: {e}")
            return False
    
    async def stream_realtime_depth(self):
        """
        Stream real-time order book updates via WebSocket.
        Uses depth@100ms stream for aggregated updates.
        """
        stream = f"{self.symbol.lower()}@depth@100ms"
        ws_url = f"{self.ws_base_url}/ws/{stream}"
        
        print(f"[INFO] Connecting to WebSocket: {ws_url}")
        
        try:
            async with websockets.connect(ws_url) as websocket:
                print(f"[INFO] WebSocket connected. Streaming {self.symbol} depth updates...")
                
                while True:
                    try:
                        message = await websocket.recv()
                        data = json.loads(message)
                        
                        # Extract timestamps
                        exchange_ts = data.get('E', 0)  # Event time
                        local_ts = int(time.time() * 1000)
                        
                        # Process bids
                        for price, qty in data.get('b', []):
                            event_line = f"{exchange_ts}|{local_ts}|UPDATE|{price}|{qty}|BID\n"
                            self.file_handle.write(event_line)
                        
                        # Process asks
                        for price, qty in data.get('a', []):
                            event_line = f"{exchange_ts}|{local_ts}|UPDATE|{price}|{qty}|ASK\n"
                            self.file_handle.write(event_line)
                        
                    except websockets.exceptions.ConnectionClosed:
                        print("[WARN] WebSocket connection closed. Reconnecting...")
                        break
                    except Exception as e:
                        print(f"[ERROR] Error processing message: {e}")
                        
        except Exception as e:
            print(f"[ERROR] WebSocket connection failed: {e}")
    
    async def run(self):
        """
        Main execution: fetch historical data, then stream real-time updates.
        """
        print(f"[INFO] Starting data ingestion for {self.symbol}")
        print(f"[INFO] Output file: {self.output_file}")
        
        # Step 1: Fetch historical snapshot
        if not self.fetch_historical_depth():
            print("[ERROR] Failed to fetch historical data. Exiting.")
            return
        
        # Step 2: Stream real-time updates
        while True:
            await self.stream_realtime_depth()
            print("[INFO] Reconnecting in 5 seconds...")
            await asyncio.sleep(5)
    
    def close(self):
        """Clean up resources."""
        if self.file_handle:
            self.file_handle.close()
            print(f"[INFO] Closed output file: {self.output_file}")


async def main():
    """Entry point for data ingestion."""
    ingestion = BinanceDataIngestion(symbol="BTCUSDT", output_dir="./data")
    
    try:
        await ingestion.run()
    except KeyboardInterrupt:
        print("\n[INFO] Shutting down...")
    finally:
        ingestion.close()


if __name__ == "__main__":
    asyncio.run(main())
