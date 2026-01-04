#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace lob {

class MetricsLogger {
public:
  MetricsLogger(const std::string &asset,
                const std::string &output_dir = "./logs");
  ~MetricsLogger();

  // Log different types of metrics
  void log_trade(uint64_t timestamp, double price, double quantity,
                 const std::string &side);
  void log_latency(uint64_t exchange_ts, uint64_t local_ts,
                   uint64_t processing_ts);
  void log_inventory(uint64_t timestamp, double position, double pnl);
  void log_pnl(uint64_t timestamp, double gross_pnl, double net_pnl,
               double fees);
  void log_order_book_state(uint64_t timestamp, double best_bid,
                            double best_ask, double mid_price, double spread,
                            double imbalance);

  // Flush all buffers
  void flush();

private:
  std::string asset_;
  std::string output_dir_;

  // File handles with custom naming: asset-timestamp.log
  std::ofstream trades_log_;
  std::ofstream latency_log_;
  std::ofstream inventory_log_;
  std::ofstream pnl_log_;
  std::ofstream orderbook_log_;

  // Helper to format timestamp as HH:MM:SS
  std::string format_time(uint64_t timestamp_ms);
};

} // namespace lob
