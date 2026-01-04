# Market Microstructure LOB - Quick Start Guide

## Step-by-Step Execution

### 1. Install Python Dependencies
```powershell
pip install -r requirements.txt
```

### 2. Option A: Test with Synthetic Data (Recommended First)

Generate test data:
```powershell
cd tests
python create_test_data.py
cd ..
```

Build C++ engine:
```powershell
cd engine
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
cd ../..
```

Run the engine:
```powershell
cd engine/build
./market_engine ../../data/test_events.events
cd ../..
```

Analyze results:
```powershell
cd analysis
python analyze_logs.py
cd ..
```

### 3. Option B: Live Data from Binance

Start data ingestion (let it run for 1-5 minutes):
```powershell
cd ingestion
python data.py
# Press Ctrl+C after collecting data
cd ..
```

Process the collected data:
```powershell
cd engine/build
# Find your event file in ../../data/
./market_engine ../../data/<timestamp>-BTCUSDT.events
cd ../..
```

Analyze results:
```powershell
cd analysis
python analyze_logs.py --log_dir ../logs/<dir_name>
cd ..
```

## Expected Outputs

### Console Output (C++ Engine)
```
=== Market Microstructure Engine ===
[INFO] Processing events from: ...
[INFO] Using strategy: ImbalanceStrategy
[INFO] Processed 10000 events
[INFO] Processed 20000 events
...
=== Processing Complete ===
[STATS] Total events processed: 5020
[STATS] Average processing latency: 2.34 μs
[STATS] Final position: 0.05
[STATS] Final PnL: $12.45
```

### Log Files (in ./logs/)
- `BTCUSDT-03_01_2026_13_08_00-trades.log`
- `BTCUSDT-03_01_2026_13_08_00-latency.log`
- `BTCUSDT-03_01_2026_13_08_00-inventory.log`
- `BTCUSDT-03_01_2026_13_08_00-pnl.log`
- `BTCUSDT-03_01_2026_13_08_00-orderbook.log`

### Analysis Outputs
- `pnl_curve.png`
- `inventory.png`
- `latency_histogram.png`
- `orderbook_depth.png`
- Console statistics report

## Troubleshooting

### CMake not found
Install CMake from https://cmake.org/download/

### Python packages missing
```powershell
pip install websockets requests pandas matplotlib numpy
```

### C++ compiler not found
- Windows: Install Visual Studio with C++ tools
- Linux: `sudo apt install build-essential cmake`
- Mac: `xcode-select --install`

### WebSocket connection fails
- Check internet connection
- Binance may have rate limits
- Try again after a few minutes

## Next Steps

1. **Modify Strategy**: Edit `engine/strategy/Strategy.cpp` to implement your own strategy
2. **Tune Parameters**: Adjust imbalance threshold, depth, risk aversion
3. **Add Metrics**: Extend `Metrics.cpp` to log additional data
4. **Optimize Performance**: Profile with `perf` or Visual Studio Profiler
5. **Backtest**: Process multiple days of historical data

## Performance Benchmarks

Expected performance on modern hardware:
- Event processing: 1-10 μs per event
- Throughput: 100,000+ events/second
- Memory usage: < 100 MB for typical order book

## Questions?

Refer to README.md for detailed documentation.
