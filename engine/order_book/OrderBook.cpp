#include "OrderBook.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace lob {

OrderBook::OrderBook(const std::string &symbol) : symbol_(symbol) {}

void OrderBook::add_order(double price, double quantity, Side side,
                          uint64_t timestamp) {
  // BINANCE L2 SEMANTICS: Replace volume at price level (not incremental add)
  // If quantity > 0: set level to this quantity
  // If quantity == 0: handled by update_order (removes level)

  if (side == Side::BID) {
    auto it = bids_.find(price);
    if (it != bids_.end()) {
      // REPLACE existing volume (Binance semantics)
      it->second.total_volume = quantity;
    } else {
      // Create new level
      Limit limit(price);
      limit.total_volume = quantity;
      limit.order_count = 1; // L2 aggregated level
      bids_[price] = limit;
    }
  } else {
    auto it = asks_.find(price);
    if (it != asks_.end()) {
      // REPLACE existing volume (Binance semantics)
      it->second.total_volume = quantity;
    } else {
      // Create new level
      Limit limit(price);
      limit.total_volume = quantity;
      limit.order_count = 1; // L2 aggregated level
      asks_[price] = limit;
    }
  }
}

void OrderBook::cancel_order(double price, Side side) {
  if (side == Side::BID) {
    bids_.erase(price);
  } else {
    asks_.erase(price);
  }
}

void OrderBook::update_order(double price, double quantity, Side side,
                             uint64_t timestamp) {
  // BINANCE L2 UPDATE SEMANTICS:
  // - quantity == 0: Remove price level immediately
  // - quantity > 0: Replace volume at this price level

  // Zero quantity: remove the level immediately
  if (quantity == 0.0 || std::abs(quantity) < 1e-8) {
    cancel_order(price, side);
    return;
  }

  // Non-zero quantity: replace volume (Binance L2 semantics)
  add_order(price, quantity, side, timestamp);

  // Validate book integrity after update
  validate_book_integrity();
}

std::optional<double> OrderBook::get_best_bid() const {
  if (bids_.empty())
    return std::nullopt;
  return bids_.begin()->first;
}

std::optional<double> OrderBook::get_best_ask() const {
  if (asks_.empty())
    return std::nullopt;
  return asks_.begin()->first;
}

std::optional<double> OrderBook::get_mid_price() const {
  auto best_bid = get_best_bid();
  auto best_ask = get_best_ask();

  if (!best_bid || !best_ask)
    return std::nullopt;

  return (*best_bid + *best_ask) / 2.0;
}

std::optional<double> OrderBook::get_spread() const {
  auto best_bid = get_best_bid();
  auto best_ask = get_best_ask();

  if (!best_bid || !best_ask)
    return std::nullopt;

  return *best_ask - *best_bid;
}

double OrderBook::get_bid_volume(double price) const {
  auto it = bids_.find(price);
  if (it != bids_.end()) {
    return it->second.total_volume;
  }
  return 0.0;
}

double OrderBook::get_ask_volume(double price) const {
  auto it = asks_.find(price);
  if (it != asks_.end()) {
    return it->second.total_volume;
  }
  return 0.0;
}

std::vector<std::pair<double, double>>
OrderBook::get_bid_depth(size_t n) const {
  std::vector<std::pair<double, double>> result;
  result.reserve(n);

  size_t count = 0;
  for (const auto &[price, limit] : bids_) {
    if (count >= n)
      break;
    result.emplace_back(price, limit.total_volume);
    count++;
  }

  return result;
}

std::vector<std::pair<double, double>>
OrderBook::get_ask_depth(size_t n) const {
  std::vector<std::pair<double, double>> result;
  result.reserve(n);

  size_t count = 0;
  for (const auto &[price, limit] : asks_) {
    if (count >= n)
      break;
    result.emplace_back(price, limit.total_volume);
    count++;
  }

  return result;
}

double OrderBook::get_total_bid_volume(size_t depth) const {
  double total = 0.0;
  size_t count = 0;

  for (const auto &[price, limit] : bids_) {
    if (count >= depth)
      break;
    total += limit.total_volume;
    count++;
  }

  return total;
}

double OrderBook::get_total_ask_volume(size_t depth) const {
  double total = 0.0;
  size_t count = 0;

  for (const auto &[price, limit] : asks_) {
    if (count >= depth)
      break;
    total += limit.total_volume;
    count++;
  }

  return total;
}

double OrderBook::calculate_imbalance(size_t depth) const {
  double bid_volume = get_total_bid_volume(depth);
  double ask_volume = get_total_ask_volume(depth);

  double total_volume = bid_volume + ask_volume;

  if (total_volume < 1e-8)
    return 0.0;

  return (bid_volume - ask_volume) / total_volume;
}

void OrderBook::validate_book_integrity() const {
  // CRITICAL: Detect crossed book (data corruption indicator)
  // In a valid order book: best_bid < best_ask
  // If violated: data is corrupted or out of sync

  auto best_bid = get_best_bid();
  auto best_ask = get_best_ask();

  if (best_bid && best_ask) {
    if (*best_bid >= *best_ask) {
      std::cerr << "[CRITICAL] Crossed book detected for " << symbol_
                << ": best_bid=" << *best_bid << " >= best_ask=" << *best_ask
                << std::endl;
      std::cerr << "[CRITICAL] Book is now invalid. Resync required."
                << std::endl;

      // In production, you would:
      // 1. Trigger book resync
      // 2. Alert monitoring system
      // 3. Halt trading on this symbol
      // For now, we log the error
    }
  }
}

void OrderBook::clear() {
  bids_.clear();
  asks_.clear();
}

} // namespace lob
