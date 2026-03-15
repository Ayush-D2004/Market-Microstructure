#pragma once

#include "../order_book/OrderBook.h"
#include "RollingMath.h"
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace lob {

// Base strategy class
class Strategy {
public:
  explicit Strategy(const std::string &name)
      : name_(name), position_(0.0), pnl_(0.0), avg_entry_price_(0.0) {}
  virtual ~Strategy() = default;

  // Main strategy evaluation - returns signal: 1 (buy), -1 (sell), 0 (hold)
  virtual int evaluate(const OrderBook &book, uint64_t timestamp) = 0;

  // Update position and PnL
  void update_position(double quantity, double price);

  // Getters
  std::string get_name() const { return name_; }
  double get_position() const { return position_; }
  double get_pnl() const { return pnl_; }

protected:
  std::string name_;
  double position_;
  double pnl_;
  double avg_entry_price_;
};

// Research-Grade Regime Aware Imbalance Strategy
// Implements:
// 1. Z-Score Normalization (Dynamic Thresholds)
// 2. Spread Conditioning (Gating)
// 3. Queue-Weighted Imbalance (Depth Decay)
// 4. Directional Asymmetry
class RegimeAwareImbalanceStrategy : public Strategy {
public:
  RegimeAwareImbalanceStrategy(double z_buy_threshold = 1.5,
                               double z_sell_threshold = -1.5,
                               size_t window_size = 100);

  int evaluate(const OrderBook &book, uint64_t timestamp) override;

private:
  // Hyperparameters
  double z_buy_threshold_;
  double z_sell_threshold_;
  size_t window_size_;
  double alpha_decay_; // For queue weighting

  // O(1) / O(log N) Rolling Statistics Buffers
  RollingMedian spread_history_;
  RollingVariance imbalance_history_;

  // Helper methods
  double calculate_weighted_imbalance(const OrderBook &book) const;
};

} // namespace lob
