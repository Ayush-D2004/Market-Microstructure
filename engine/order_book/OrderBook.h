#pragma once

#include "Order.h"
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace lob {

class OrderBook {
public:
  OrderBook(const std::string &symbol);
  ~OrderBook() = default;

  // Core operations (Binance L2 semantics: replace, not add)
  void add_order(double price, double quantity, Side side, uint64_t timestamp);
  void cancel_order(double price, Side side);
  void update_order(double price, double quantity, Side side,
                    uint64_t timestamp);

  // Query operations
  std::optional<double> get_best_bid() const;
  std::optional<double> get_best_ask() const;
  std::optional<double> get_mid_price() const;
  std::optional<double> get_spread() const;

  // Get volume at price level
  double get_bid_volume(double price) const;
  double get_ask_volume(double price) const;

  // Get top N levels
  std::vector<std::pair<double, double>> get_bid_depth(size_t n) const;
  std::vector<std::pair<double, double>> get_ask_depth(size_t n) const;

  // Market microstructure metrics
  double calculate_imbalance(size_t depth = 5) const;
  double get_total_bid_volume(size_t depth = 10) const;
  double get_total_ask_volume(size_t depth = 10) const;

  // Utility
  void clear();
  std::string get_symbol() const { return symbol_; }

private:
  std::string symbol_;

  // Bid book: sorted descending (highest price first)
  std::map<double, Limit, std::greater<double>> bids_;

  // Ask book: sorted ascending (lowest price first)
  std::map<double, Limit, std::less<double>> asks_;

  // Validation
  void validate_book_integrity() const;
};

} // namespace lob
