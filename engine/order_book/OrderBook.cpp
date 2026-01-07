#include "OrderBook.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace lob {

OrderBook::OrderBook(const std::string &symbol)
    : symbol_(symbol), next_order_id_(1) {}

void OrderBook::add_order(double price, double quantity, Side side,
                          uint64_t timestamp) {
  // HYBRID L2/L3 SEMANTICS:
  // L2 input: absolute volume at price level
  // L3 simulation: maintain deque of synthetic orders
  // Delta calculation: new_qty - old_qty determines add/remove

  if (side == Side::BID) {
    auto it = bids_.find(price);
    if (it != bids_.end()) {
      // Existing level: calculate delta
      double old_volume = it->second.total_volume;
      double delta = quantity - old_volume;

      if (delta > 1e-8) {
        // Volume increase: add synthetic order
        it->second.add_synthetic_order(next_order_id_++, delta, side,
                                       timestamp);
      } else if (delta < -1e-8) {
        // Volume decrease: remove from front (FIFO)
        it->second.reduce_volume_fifo(-delta);
      }
      // else: delta ~= 0, no change

      it->second.validate_invariants();
    } else {
      // New level: create with single synthetic order
      Limit limit(price);
      limit.add_synthetic_order(next_order_id_++, quantity, side, timestamp);
      limit.validate_invariants();
      bids_[price] = limit;
    }
  } else {
    auto it = asks_.find(price);
    if (it != asks_.end()) {
      // Existing level: calculate delta
      double old_volume = it->second.total_volume;
      double delta = quantity - old_volume;

      if (delta > 1e-8) {
        // Volume increase: add synthetic order
        it->second.add_synthetic_order(next_order_id_++, delta, side,
                                       timestamp);
      } else if (delta < -1e-8) {
        // Volume decrease: remove from front (FIFO)
        it->second.reduce_volume_fifo(-delta);
      }
      // else: delta ~= 0, no change

      it->second.validate_invariants();
    } else {
      // New level: create with single synthetic order
      Limit limit(price);
      limit.add_synthetic_order(next_order_id_++, quantity, side, timestamp);
      limit.validate_invariants();
      asks_[price] = limit;
    }
  }
}

void OrderBook::clear_price_level(double price, Side side) {
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
  // - quantity > 0: Replace volume at this price level (delta-based L3
  // simulation)

  // Zero quantity: remove the level immediately
  if (quantity == 0.0 || std::abs(quantity) < 1e-8) {
    clear_price_level(price, side);
    return;
  }

  // Non-zero quantity: use delta-based add_order (Hybrid L2/L3)
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
  // Note: best_bid == best_ask is acceptable during rapid updates

  auto best_bid = get_best_bid();
  auto best_ask = get_best_ask();

  if (best_bid && best_ask) {
    // Only fix if STRICTLY crossed (bid > ask), not equal
    if (*best_bid > *best_ask) {
      std::cerr << "[WARN] Crossed book detected for " << symbol_
                << ": best_bid=" << *best_bid << " > best_ask=" << *best_ask
                << std::endl;
      std::cerr << "[WARN] Auto-fixing by clearing crossed levels..."
                << std::endl;

      // Auto-fix: Remove crossed levels
      OrderBook *mutable_this = const_cast<OrderBook *>(this);

      // Remove all bids STRICTLY > best ask (not >=)
      auto bid_it = mutable_this->bids_.begin();
      while (bid_it != mutable_this->bids_.end() && bid_it->first > *best_ask) {
        std::cerr << "[WARN] Removing crossed bid level: " << bid_it->first
                  << std::endl;
        bid_it = mutable_this->bids_.erase(bid_it);
      }

      // Remove all asks STRICTLY < best bid (not <=)
      auto new_best_bid = mutable_this->get_best_bid();
      if (new_best_bid) {
        auto ask_it = mutable_this->asks_.begin();
        while (ask_it != mutable_this->asks_.end() &&
               ask_it->first < *new_best_bid) {
          std::cerr << "[WARN] Removing crossed ask level: " << ask_it->first
                    << std::endl;
          ask_it = mutable_this->asks_.erase(ask_it);
        }
      }

      std::cerr << "[INFO] Book fixed. Continuing..." << std::endl;
    }
  }
}

void OrderBook::clear() {
  bids_.clear();
  asks_.clear();
  reset_order_ids();
}

const std::deque<Order> &OrderBook::get_orders_at_price(double price,
                                                        Side side) const {
  static const std::deque<Order> empty_deque;

  if (side == Side::BID) {
    auto it = bids_.find(price);
    if (it != bids_.end()) {
      return it->second.orders;
    }
  } else {
    auto it = asks_.find(price);
    if (it != asks_.end()) {
      return it->second.orders;
    }
  }

  return empty_deque;
}

} // namespace lob
