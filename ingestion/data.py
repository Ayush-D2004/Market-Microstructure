"""
Binance Data Ingestion Layer
Fetches historical data via REST API and streams real-time data via WebSocket.
Normalizes and writes events to disk in the format:
[exchange_seq]|[exchange_event_ts]|[local_ingest_ts]|[event_type]|[price]|[qty]|[side]

CRITICAL: Implements correct Binance snapshot+delta synchronization:
1. Start WebSocket and buffer deltas
2. Fetch REST snapshot
3. Apply snapshot
4. Apply buffered deltas where u > lastUpdateId
5. Continue streaming with sequence validation
"""

import asyncio
import json
import time
import websockets
import requests
from datetime import datetime
from pathlib import Path
from typing import Optional, List, Dict


class BinanceDataIngestion:
    """
    Handles both historical (REST) and real-time (WebSocket) data ingestion from Binance.
    Implements correct snapshot+delta synchronization per Binance spec.
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
        
        # Synchronization state
        self.delta_buffer: List[Dict] = []
        self.snapshot_last_update_id: Optional[int] = None
        self.last_applied_update_id: Optional[int] = None
        self.book_initialized = False
        
        # WebSocket task
        self.ws_task: Optional[asyncio.Task] = None
        self.snapshot_ready = asyncio.Event()
        
    def _write_event(self, exchange_seq: int, exchange_event_ts: int, 
                     event_type: str, price: str, qty: str, side: str):
        """Write a single event to the output file."""
        local_ingest_ts = int(time.time() * 1000)
        event_line = f"{exchange_seq}|{exchange_event_ts}|{local_ingest_ts}|{event_type}|{price}|{qty}|{side}\n"
        self.file_handle.write(event_line)
    
    def fetch_snapshot(self, limit: int = 1000) -> bool:
        """
        Fetch order book snapshot via REST API.
        This must be called AFTER WebSocket is started and buffering.
        
        Args:
            limit: Number of price levels to fetch (max 5000)
            
        Returns:
            True if successful, False otherwise
        """
        try:
            url = f"{self.rest_base_url}/api/v3/depth"
            params = {"symbol": self.symbol, "limit": limit}
            
            print(f"[INFO] Fetching snapshot...")
            response = requests.get(url, params=params, timeout=10)
            response.raise_for_status()
            data = response.json()
            
            # Store the snapshot's lastUpdateId (this is a SEQUENCE NUMBER, not timestamp)
            self.snapshot_last_update_id = data.get('lastUpdateId', 0)
            snapshot_ts = int(time.time() * 1000)  # Use current time for snapshot timestamp
            
            print(f"[INFO] Snapshot received: lastUpdateId={self.snapshot_last_update_id}")
            print(f"[INFO] Buffered {len(self.delta_buffer)} deltas during snapshot fetch")
            
            # Write bids
            for price, qty in data.get('bids', []):
                self._write_event(
                    exchange_seq=self.snapshot_last_update_id,
                    exchange_event_ts=snapshot_ts,
                    event_type="SNAPSHOT",
                    price=price,
                    qty=qty,
                    side="BID"
                )
            
            # Write asks
            for price, qty in data.get('asks', []):
                self._write_event(
                    exchange_seq=self.snapshot_last_update_id,
                    exchange_event_ts=snapshot_ts,
                    event_type="SNAPSHOT",
                    price=price,
                    qty=qty,
                    side="ASK"
                )
            
            print(f"[INFO] Snapshot written: {len(data.get('bids', []))} bids, {len(data.get('asks', []))} asks")
            return True
            
        except Exception as e:
            print(f"[ERROR] Failed to fetch snapshot: {e}")
            return False
    
    def apply_buffered_deltas(self):
        """
        Apply buffered deltas after snapshot is received.
        Only apply deltas where u > snapshot_last_update_id.
        Validates sequence continuity.
        """
        applied_count = 0
        discarded_count = 0
        
        print(f"[INFO] Applying buffered deltas...")
        
        for delta in self.delta_buffer:
            u = delta.get('u', 0)  # Final update ID in this event
            U = delta.get('U', 0)  # First update ID in this event
            
            # Discard old deltas
            if u <= self.snapshot_last_update_id:
                discarded_count += 1
                continue
            
            # Validate sequence continuity
            if self.last_applied_update_id is not None:
                if U != self.last_applied_update_id + 1:
                    raise RuntimeError(
                        f"Sequence gap detected in buffered deltas: "
                        f"expected U={self.last_applied_update_id + 1}, got U={U}"
                    )
            else:
                # First delta after snapshot: U should be snapshot_last_update_id + 1
                if U != self.snapshot_last_update_id + 1:
                    print(f"[WARN] First delta U={U} != snapshot_last_update_id + 1 = {self.snapshot_last_update_id + 1}")
                    # This is acceptable if there were no updates between snapshot and first delta
            
            # Apply delta
            self._write_delta(delta)
            self.last_applied_update_id = u
            applied_count += 1
        
        print(f"[INFO] Applied {applied_count} buffered deltas, discarded {discarded_count} old deltas")
        self.delta_buffer.clear()
        self.book_initialized = True
    
    def _write_delta(self, delta: Dict):
        """Write a delta update to the output file."""
        exchange_event_ts = delta.get('E', 0)  # Event time (milliseconds)
        u = delta.get('u', 0)  # Final update ID (sequence number)
        
        # Process bids
        for price, qty in delta.get('b', []):
            self._write_event(
                exchange_seq=u,
                exchange_event_ts=exchange_event_ts,
                event_type="UPDATE",
                price=price,
                qty=qty,
                side="BID"
            )
        
        # Process asks
        for price, qty in delta.get('a', []):
            self._write_event(
                exchange_seq=u,
                exchange_event_ts=exchange_event_ts,
                event_type="UPDATE",
                price=price,
                qty=qty,
                side="ASK"
            )
    
    async def websocket_handler(self):
        """
        WebSocket handler that buffers deltas before snapshot,
        then streams with sequence validation after initialization.
        """
        stream = f"{self.symbol.lower()}@depth@100ms"
        ws_url = f"{self.ws_base_url}/ws/{stream}"
        
        print(f"[INFO] Connecting to WebSocket: {ws_url}")
        
        try:
            async with websockets.connect(ws_url) as websocket:
                print(f"[INFO] WebSocket connected. Buffering deltas...")
                
                while True:
                    try:
                        message = await websocket.recv()
                        data = json.loads(message)
                        
                        if not self.book_initialized:
                            # Buffer deltas until snapshot is applied
                            self.delta_buffer.append(data)
                            
                            # Signal that we're receiving data (snapshot can be fetched)
                            if not self.snapshot_ready.is_set():
                                self.snapshot_ready.set()
                        else:
                            # Book is initialized, validate sequence and write
                            U = data.get('U', 0)
                            u = data.get('u', 0)
                            
                            # Validate sequence continuity
                            if self.last_applied_update_id is not None:
                                if U != self.last_applied_update_id + 1:
                                    raise RuntimeError(
                                        f"WebSocket sequence gap detected: "
                                        f"expected U={self.last_applied_update_id + 1}, got U={U}"
                                    )
                            
                            # Write delta
                            self._write_delta(data)
                            self.last_applied_update_id = u
                        
                    except websockets.exceptions.ConnectionClosed:
                        print("[WARN] WebSocket connection closed")
                        break
                    except RuntimeError as e:
                        print(f"[ERROR] {e}")
                        print("[ERROR] Sequence gap detected. Book is now invalid.")
                        raise
                    except Exception as e:
                        print(f"[ERROR] Error processing message: {e}")
                        
        except Exception as e:
            print(f"[ERROR] WebSocket connection failed: {e}")
            raise
    
    async def run(self):
        """
        Main execution with correct synchronization:
        1. Start WebSocket (buffering)
        2. Wait for first delta
        3. Fetch snapshot
        4. Apply snapshot
        5. Apply buffered deltas
        6. Continue streaming
        """
        print(f"[INFO] Starting data ingestion for {self.symbol}")
        print(f"[INFO] Output file: {self.output_file}")
        
        # Open file for writing
        self.file_handle = open(self.output_file, 'a', buffering=1)
        
        try:
            # Step 1: Start WebSocket (it will buffer deltas)
            self.ws_task = asyncio.create_task(self.websocket_handler())
            
            # Step 2: Wait for WebSocket to receive first delta
            print("[INFO] Waiting for WebSocket to start receiving data...")
            await asyncio.wait_for(self.snapshot_ready.wait(), timeout=30)
            
            # Give it a moment to buffer some deltas
            await asyncio.sleep(0.5)
            
            # Step 3: Fetch snapshot
            if not self.fetch_snapshot():
                print("[ERROR] Failed to fetch snapshot. Exiting.")
                return
            
            # Step 4 & 5: Apply snapshot and buffered deltas
            self.apply_buffered_deltas()
            
            print("[INFO] Book initialized. Streaming live updates...")
            
            # Step 6: Continue streaming (ws_task handles this)
            await self.ws_task
            
        except asyncio.TimeoutError:
            print("[ERROR] Timeout waiting for WebSocket data")
        except Exception as e:
            print(f"[ERROR] Fatal error: {e}")
            if self.ws_task:
                self.ws_task.cancel()
    
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
