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

## ✅ What's Implemented

### 1. Python Ingestion Layer (`ingestion/data.py`)
✅ Binance REST API for historical snapshots  
✅ Binance WebSocket for real-time streaming  
✅ Automatic reconnection on failures  
✅ Normalized event format: `timestamp|timestamp|type|price|qty|side`

### 2. C++ Core Engine (`engine/`)

**Order Book** (`order_book/`)
- ✅ Efficient price-level management with `std::map`
- ✅ O(log n) insert/delete, O(1) best bid/ask
- ✅ Market microstructure metrics (imbalance, spread)

**Strategies** (`strategy/`)
- ✅ **Imbalance Strategy**: Trade on bid/ask volume ratio
- ✅ **Market Making**: Inventory-aware reservation pricing

**Metrics** (`metrics/`)
- ✅ Custom log naming: `BTCUSDT-DD_MM_YYYY_HH_MM_SS-type.log`
- ✅ Timestamped entries: `HH:MM:SS` format
- ✅ 5 log types: trades, latency, inventory, PnL, orderbook

**I/O** (`io/`)
- ✅ Event file parser
- ✅ Efficient streaming processing

### 3. Python Analysis Layer (`analysis/analyze_logs.py`)
✅ Statistical analysis (Sharpe, drawdown, hit rate)  
✅ Latency metrics (P50, P95, P99)  
✅ 4 visualization types (PnL, inventory, latency, orderbook)

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

### Installation (3 minutes)

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

**Output Logs** (in `./logs/`):
- `BTCUSDT-03_01_2026_13_08_00-trades.log`
- `BTCUSDT-03_01_2026_13_08_00-latency.log`
- `BTCUSDT-03_01_2026_13_08_00-inventory.log`
- `BTCUSDT-03_01_2026_13_08_00-pnl.log`
- `BTCUSDT-03_01_2026_13_08_00-orderbook.log`

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

---

## 🏗️ Architecture

```
┌─────────────────────────────────────┐
│         Binance API                 │
│  REST (Snapshot) | WebSocket (Live) │
└──────────────┬──────────────────────┘
               ↓
┌──────────────────────────────────────┐
│    Python Ingestion Layer            │
│  - Normalize data                    │
│  - Write to .events file             │
└──────────────┬───────────────────────┘
               ↓
┌──────────────────────────────────────┐
│    Event Files (.events)             │
│  Format: ts|ts|type|price|qty|side   │
└──────────────┬───────────────────────┘
               ↓
┌──────────────────────────────────────┐
│    C++ Market Engine                 │
│  ┌────────────────────────────────┐  │
│  │ Event Reader                   │  │
│  └────────┬───────────────────────┘  │
│           ↓                          │
│  ┌────────────────────────────────┐  │
│  │ Order Book (Bid/Ask)           │  │
│  └────────┬───────────────────────┘  │
│           ↓                          │
│  ┌────────────────────────────────┐  │
│  │ Strategy Engine                │  │
│  │ - Imbalance / Market Making    │  │
│  └────────┬───────────────────────┘  │
│           ↓                          │
│  ┌────────────────────────────────┐  │
│  │ Metrics Logger                 │  │
│  └────────┬───────────────────────┘  │
└───────────┼──────────────────────────┘
            ↓
┌──────────────────────────────────────┐
│    Log Files (5 types)               │
│  - trades, latency, inventory,       │
│    pnl, orderbook                    │
└──────────────┬───────────────────────┘
               ↓
┌──────────────────────────────────────┐
│    Python Analysis Layer             │
│  - Parse logs                        │
│  - Calculate metrics                 │
│  - Generate plots                    │
└──────────────────────────────────────┘
```

---

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

**Hardware:** Modern CPU (Intel i5/i7, AMD Ryzen)

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

## 📚 Learning Resources

**Market Microstructure:**
- Avellaneda & Stoikov (2008): High-frequency trading in a limit order book
- Cont, Stoikov, Talreja (2010): Stochastic model for order book dynamics

**C++ Performance:**
- Data structure selection (map vs unordered_map)
- Memory allocation strategies
- Profiling with `perf` or Visual Studio Profiler

**Quantitative Finance:**
- Order flow toxicity
- Adverse selection
- Inventory risk management

---

