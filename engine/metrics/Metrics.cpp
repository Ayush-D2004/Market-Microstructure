#include "Metrics.h"
#include <filesystem>
#include <iostream>

namespace lob {

MetricsLogger::MetricsLogger(const std::string &asset,
                             const std::string &output_dir)
    : asset_(asset) {

  // Generate timestamp for the session folder
  auto now = std::chrono::system_clock::now();
  time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm *tm_info = std::localtime(&now_time);

  std::ostringstream time_ss;
  time_ss << (tm_info->tm_year + 1900) << "_" << std::setfill('0')
          << std::setw(2) << (tm_info->tm_mon + 1) << "_" << std::setfill('0')
          << std::setw(2) << tm_info->tm_mday << "_" << std::setfill('0')
          << std::setw(2) << tm_info->tm_hour << "_" << std::setfill('0')
          << std::setw(2) << tm_info->tm_min << "_" << std::setfill('0')
          << std::setw(2) << tm_info->tm_sec;
  std::string timestamp_str = time_ss.str();

  // Create output directory: base_dir/asset_timestamp/
  std::ostringstream dir_ss;
  dir_ss << output_dir << "/" << asset_ << "_" << timestamp_str;
  output_dir_ = dir_ss.str();

  std::filesystem::create_directories(output_dir_);

  // Open log files with simple naming: logtype.log
  // (Since they are already in a unique timestamped folder)
  trades_log_.open(output_dir_ + "/trades.log", std::ios::app);
  latency_log_.open(output_dir_ + "/latency.log", std::ios::app);
  inventory_log_.open(output_dir_ + "/inventory.log", std::ios::app);
  pnl_log_.open(output_dir_ + "/pnl.log", std::ios::app);
  orderbook_log_.open(output_dir_ + "/orderbook.log", std::ios::app);

  // Write headers
  if (trades_log_.is_open()) {
    trades_log_ << "Time,Price,Quantity,Side\n";
  }
  if (latency_log_.is_open()) {
    latency_log_ << "Time,ExchangeTS,LocalTS,ProcessingTS,Engine_Latency_us\n";
  }
  if (inventory_log_.is_open()) {
    inventory_log_ << "Time,Position,PnL\n";
  }
  if (pnl_log_.is_open()) {
    pnl_log_ << "Time,GrossPnL,NetPnL,Fees\n";
  }
  if (orderbook_log_.is_open()) {
    orderbook_log_ << "Time,BestBid,BestAsk,MidPrice,Spread,Imbalance\n";
  }

  std::cout << "[INFO] Metrics logger initialized for " << asset_ << std::endl;
  std::cout << "[INFO] Log files created in: " << output_dir_ << std::endl;
}

MetricsLogger::~MetricsLogger() {
  flush();

  if (trades_log_.is_open())
    trades_log_.close();
  if (latency_log_.is_open())
    latency_log_.close();
  if (inventory_log_.is_open())
    inventory_log_.close();
  if (pnl_log_.is_open())
    pnl_log_.close();
  if (orderbook_log_.is_open())
    orderbook_log_.close();
}

void MetricsLogger::log_trade(uint64_t timestamp, double price, double quantity,
                              const std::string &side) {
  if (trades_log_.is_open()) {
    trades_log_ << format_time(timestamp) << "," << price << "," << quantity
                << "," << side << "\n";
  }
}

void MetricsLogger::log_latency(uint64_t exchange_ts, uint64_t local_ts,
                                uint64_t processing_ts) {
  if (latency_log_.is_open()) {
    int64_t engine_latency =
        static_cast<int64_t>(processing_ts) - static_cast<int64_t>(local_ts);
    latency_log_ << format_time(processing_ts) << "," << exchange_ts << ","
                 << local_ts << "," << processing_ts << "," << engine_latency
                 << "\n";
  }
}

void MetricsLogger::log_inventory(uint64_t timestamp, double position,
                                  double pnl) {
  if (inventory_log_.is_open()) {
    inventory_log_ << format_time(timestamp) << "," << position << "," << pnl
                   << "\n";
  }
}

void MetricsLogger::log_pnl(uint64_t timestamp, double gross_pnl,
                            double net_pnl, double fees) {
  if (pnl_log_.is_open()) {
    pnl_log_ << format_time(timestamp) << "," << gross_pnl << "," << net_pnl
             << "," << fees << "\n";
  }
}

void MetricsLogger::log_order_book_state(uint64_t timestamp, double best_bid,
                                         double best_ask, double mid_price,
                                         double spread, double imbalance) {
  if (orderbook_log_.is_open()) {
    orderbook_log_ << format_time(timestamp) << "," << best_bid << ","
                   << best_ask << "," << mid_price << "," << spread << ","
                   << imbalance << "\n";
  }
}

void MetricsLogger::flush() {
  if (trades_log_.is_open())
    trades_log_.flush();
  if (latency_log_.is_open())
    latency_log_.flush();
  if (inventory_log_.is_open())
    inventory_log_.flush();
  if (pnl_log_.is_open())
    pnl_log_.flush();
  if (orderbook_log_.is_open())
    orderbook_log_.flush();
}

std::string MetricsLogger::format_time(uint64_t timestamp_ms) {
  // Convert milliseconds to seconds
  time_t seconds = timestamp_ms / 1000;
  uint64_t milliseconds = timestamp_ms % 1000;

  std::tm *tm_info = std::localtime(&seconds);

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << tm_info->tm_hour << ":"
      << std::setfill('0') << std::setw(2) << tm_info->tm_min << ":"
      << std::setfill('0') << std::setw(2) << tm_info->tm_sec;

  return oss.str();
}

} // namespace lob
