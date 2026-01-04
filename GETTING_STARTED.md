# 🚀 Market Microstructure - Limit Order Book Project
## Complete Implementation Guide

---

## 📋 Table of Contents
1. [Project Overview](#project-overview)
2. [What's Implemented](#whats-implemented)
3. [Quick Start](#quick-start)
4. [Detailed Usage](#detailed-usage)
5. [Architecture](#architecture)
6. [Customization](#customization)

---

## 🎯 Project Overview

This is a **market microstructure system** implementing:
- Real-time limit order book maintenance
- Trading strategy execution
- Comprehensive performance metrics
- Low-latency event processing (< 10 μs)

**Technology Stack:**
- **Python**: Data ingestion (Binance API) + Post-processing
- **C++17**: High-performance order book + Strategy engine
- **CMake**: Build system

---

## What's Implemented

### 1. Python Ingestion Layer (`ingestion/data.py`)
Binance REST API for historical snapshots  
Binance WebSocket for real-time streaming  
Automatic reconnection on failures  
Normalized event format: `timestamp|timestamp|type|price|qty|side`

### 2. C++ Core Engine (`engine/`)

**Order Book** (`order_book/`)
- Efficient price-level management with `std::map`
- O(log n) insert/delete, O(1) best bid/ask
- Market microstructure metrics (imbalance, spread)

**Strategies** (`strategy/`)
- **Imbalance Strategy**: Trade on bid/ask volume ratio
- **Market Making**: Inventory-aware reservation pricing

**Metrics** (`metrics/`)
- Custom log naming: `BTCUSDT-DD_MM_YYYY_HH_MM_SS-type.log`
- Timestamped entries: `HH:MM:SS` format
- 5 log types: trades, latency, inventory, PnL, orderbook

**I/O** (`io/`)
- Event file parser
- Efficient streaming processing

### 3. Python Analysis Layer (`analysis/analyze_logs.py`)
Statistical analysis (Sharpe, drawdown, hit rate)  
Latency metrics (P50, P95, P99)  
Visualization (PnL, inventory, latency, orderbook)

---

## 🚀 Quick Start

### Prerequisites
```powershell
# Check Python
python --version  # Should be 3.8+

# Check CMake
cmake --version   # Should be 3.15+

# Check C++ compiler
g++ --version     # Or MSVC/Clang
```

### Installation 

**Step 1: Install Python dependencies**
```powershell
pip install -r requirements.txt
```

**Step 2: Build C++ engine**
```powershell
.\build.ps1
```
Or manually:
```powershell
cd engine
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
cd ..\..
```

### First Run (5 minutes)

**Option A: Test with Synthetic Data** (Recommended)
```powershell
# 1. Generate test data
python tests\create_test_data.py

# 2. Run engine
cd engine\build
.\market_engine.exe ..\..\data\test_events.events

# 3. Analyze results
cd ..\..\analysis
python analyze_logs.py
```

**Option B: Live Binance Data**
```powershell
# 1. Collect data (run for 1-5 minutes, then Ctrl+C)
python ingestion\data.py

# 2. Run engine on collected data
cd engine\build
.\market_engine.exe ..\..\data\<your-timestamp>-BTCUSDT.events

# 3. Analyze
cd ..\..\analysis
python analyze_logs.py
```

---

## 📊 Detailed Usage

### Data Ingestion

**Configuration** (edit `ingestion/data.py`):
```python
ingestion = BinanceDataIngestion(
    symbol="BTCUSDT",      # Change asset
    output_dir="./data"    # Output location
)
```

**Run:**
```powershell
cd ingestion
python data.py
```

**Output:** `data/<timestamp>-BTCUSDT.events`

### C++ Engine

**Command:**
```powershell
cd engine\build
.\market_engine.exe <event-file>
```

**Strategy Selection** (edit `engine/main.cpp` line 21):
```cpp
// Option 1: Imbalance Strategy
auto strategy = std::make_unique<ImbalanceStrategy>(0.3, 5);

// Option 2: Market Making
auto strategy = std::make_unique<MarketMakingStrategy>(0.1, 10.0);
```

### Analysis

**Run:**
```powershell
cd analysis
python analyze_logs.py
```

**Outputs:**
- Console: Statistics report
- `pnl_curve.png`: Profit/loss over time
- `inventory.png`: Position management
- `latency_histogram.png`: Performance distribution
- `orderbook_depth.png`: Market metrics


## 🔧 Customization

### Add a New Strategy

1. **Edit `engine/strategy/Strategy.h`:**
```cpp
class MyStrategy : public Strategy {
public:
    MyStrategy(/* params */);
    int evaluate(const OrderBook& book, uint64_t timestamp) override;
private:
    // Your state
};
```

2. **Edit `engine/strategy/Strategy.cpp`:**
```cpp
int MyStrategy::evaluate(const OrderBook& book, uint64_t timestamp) {
    // Your logic
    return 1;  // Buy
    return -1; // Sell
    return 0;  // Hold
}
```

3. **Use in `engine/main.cpp`:**
```cpp
auto strategy = std::make_unique<MyStrategy>(/* params */);
```

### Modify Metrics

**Edit `engine/metrics/Metrics.h` and `.cpp`:**
```cpp
void log_custom_metric(uint64_t timestamp, double value) {
    custom_log_ << format_time(timestamp) << "," << value << "\n";
}
```

### Change Asset

**In `ingestion/data.py`:**
```python
ingestion = BinanceDataIngestion(symbol="ETHUSDT")
```

**In `engine/main.cpp`:**
```cpp
std::string asset = "ETHUSDT";
```

---

## 📈 Expected Performance

| Metric | Expected Value |
|--------|----------------|
| Event Processing Latency | 1-10 μs |
| Throughput | 100,000+ events/sec |
| Memory Usage | < 100 MB |
| Build Time | < 30 seconds |

---

## 🐛 Troubleshooting

### Build Fails
```powershell
# Clean and rebuild
Remove-Item -Recurse -Force engine\build
.\build.ps1
```

### Python Import Errors
```powershell
pip install --upgrade -r requirements.txt
```

### WebSocket Connection Fails
- Check internet connection
- Binance may have rate limits (wait 5 minutes)
- Try different symbol

### No Log Files Generated
- Check `./logs/` directory exists
- Verify engine ran successfully
- Check file permissions

---

