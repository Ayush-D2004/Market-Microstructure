#include "Metrics.h"
#include <filesystem>
#include <iostream>

namespace lob {

MetricsLogger::MetricsLogger(const std::string &asset,
                             const std::string &output_dir)
    : asset_(asset), total_events_(0), total_trades_(0) {

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

  // Open log files
  trades_log_.open(output_dir_ + "/trades.log", std::ios::app);
  latency_log_.open(output_dir_ + "/latency.log", std::ios::app);
  inventory_log_.open(output_dir_ + "/inventory.log", std::ios::app);
  pnl_log_.open(output_dir_ + "/pnl.log", std::ios::app);
  orderbook_log_.open(output_dir_ + "/orderbook.log", std::ios::app);
  summary_log_.open(output_dir_ + "/summary.log", std::ios::app);

  // Write headers with EXPLICIT UNITS
  if (trades_log_.is_open()) {
    trades_log_ << "Time,Price_USD,Quantity_BTC,Side\n";
  }
  if (latency_log_.is_open()) {
    // EXPLICIT UNITS: All timestamps in milliseconds, latencies in microseconds
    latency_log_ << "Time,ExchangeTS_ms,LocalTS_ms,ProcessingTS_ms,"
                 << "Ingest_Latency_us,Processing_Latency_us\n";
  }
  if (inventory_log_.is_open()) {
    inventory_log_ << "Time,Position_BTC,PnL_USD\n";
  }
  if (pnl_log_.is_open()) {
    pnl_log_ << "Time,GrossPnL_USD,NetPnL_USD,Fees_USD\n";
  }
  if (orderbook_log_.is_open()) {
    orderbook_log_
        << "Time,BestBid_USD,BestAsk_USD,MidPrice_USD,Spread_USD,Imbalance\n";
  }

  // Reserve space for latency vectors (estimate ~10k events)
  ingest_latencies_us_.reserve(10000);
  processing_latencies_us_.reserve(10000);

  std::cout << "[INFO] Metrics logger initialized for " << asset_ << std::endl;
  std::cout << "[INFO] Log files created in: " << output_dir_ << std::endl;
}

MetricsLogger::~MetricsLogger() {
  // Generate summary before closing
  generate_summary();

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
  if (summary_log_.is_open())
    summary_log_.close();
}

void MetricsLogger::log_trade(uint64_t timestamp, double price, double quantity,
                              const std::string &side) {
  if (trades_log_.is_open()) {
    trades_log_ << format_time(timestamp) << "," << price << "," << quantity
                << "," << side << "\n";
    total_trades_++;
  }
}

void MetricsLogger::log_latency(uint64_t exchange_ts, uint64_t local_ts,
                                uint64_t processing_ts) {
  if (latency_log_.is_open()) {
    // SEPARATION OF CONCERNS:
    // 1. Ingest Latency: How long data took to arrive (Exchange -> Local)
    // 2. Processing Latency: How long engine took to process (Local ->
    // Processing)

    int64_t ingest_latency_us =
        static_cast<int64_t>(local_ts) - static_cast<int64_t>(exchange_ts);
    int64_t processing_latency_us =
        static_cast<int64_t>(processing_ts) - static_cast<int64_t>(local_ts);

    // Store for percentile calculation
    ingest_latencies_us_.push_back(ingest_latency_us);
    processing_latencies_us_.push_back(processing_latency_us);

    latency_log_ << format_time(processing_ts) << "," << exchange_ts << ","
                 << local_ts << "," << processing_ts << "," << ingest_latency_us
                 << "," << processing_latency_us << "\n";

    total_events_++;
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
  if (summary_log_.is_open())
    summary_log_.flush();
}

void MetricsLogger::generate_summary() {
  if (!summary_log_.is_open())
    return;

  summary_log_ << "=== PERFORMANCE SUMMARY ===" << "\n";
  summary_log_ << "Asset: " << asset_ << "\n";
  summary_log_ << "Total Events: " << total_events_ << "\n";
  summary_log_ << "Total Trades: " << total_trades_ << "\n\n";

  // Ingest Latency Statistics (Exchange -> Local)
  if (!ingest_latencies_us_.empty()) {
    auto ingest_copy = ingest_latencies_us_; // Copy for sorting
    std::sort(ingest_copy.begin(), ingest_copy.end());

    int64_t ingest_min = ingest_copy.front();
    int64_t ingest_max = ingest_copy.back();
    int64_t ingest_p50 = calculate_percentile(ingest_copy, 0.50);
    int64_t ingest_p95 = calculate_percentile(ingest_copy, 0.95);
    int64_t ingest_p99 = calculate_percentile(ingest_copy, 0.99);

    int64_t ingest_sum = 0;
    for (auto lat : ingest_latencies_us_)
      ingest_sum += lat;
    double ingest_avg =
        static_cast<double>(ingest_sum) / ingest_latencies_us_.size();

    summary_log_ << "--- Ingest Latency (Exchange -> Local) ---" << "\n";
    summary_log_ << "  Min:  " << ingest_min << " us" << "\n";
    summary_log_ << "  Avg:  " << ingest_avg << " us" << "\n";
    summary_log_ << "  P50:  " << ingest_p50 << " us" << "\n";
    summary_log_ << "  P95:  " << ingest_p95 << " us" << "\n";
    summary_log_ << "  P99:  " << ingest_p99 << " us" << "\n";
    summary_log_ << "  Max:  " << ingest_max << " us" << "\n\n";
  }

  // Processing Latency Statistics (Local -> Processing)
  if (!processing_latencies_us_.empty()) {
    auto proc_copy = processing_latencies_us_; // Copy for sorting
    std::sort(proc_copy.begin(), proc_copy.end());

    int64_t proc_min = proc_copy.front();
    int64_t proc_max = proc_copy.back();
    int64_t proc_p50 = calculate_percentile(proc_copy, 0.50);
    int64_t proc_p95 = calculate_percentile(proc_copy, 0.95);
    int64_t proc_p99 = calculate_percentile(proc_copy, 0.99);

    int64_t proc_sum = 0;
    for (auto lat : processing_latencies_us_)
      proc_sum += lat;
    double proc_avg =
        static_cast<double>(proc_sum) / processing_latencies_us_.size();

    summary_log_ << "--- Processing Latency (Local -> Processing) ---" << "\n";
    summary_log_ << "  Min:  " << proc_min << " us" << "\n";
    summary_log_ << "  Avg:  " << proc_avg << " us" << "\n";
    summary_log_ << "  P50:  " << proc_p50 << " us" << "\n";
    summary_log_ << "  P95:  " << proc_p95 << " us" << "\n";
    summary_log_ << "  P99:  " << proc_p99 << " us" << "\n";
    summary_log_ << "  Max:  " << proc_max << " us" << "\n\n";
  }

  summary_log_ << "=== END SUMMARY ===" << "\n";

  std::cout << "[INFO] Performance summary written to: " << output_dir_
            << "/summary.log" << std::endl;
}

int64_t MetricsLogger::calculate_percentile(std::vector<int64_t> &data,
                                            double percentile) {
  if (data.empty())
    return 0;

  // Assumes data is already sorted
  size_t index = static_cast<size_t>(percentile * (data.size() - 1));
  return data[index];
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
