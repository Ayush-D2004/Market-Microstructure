#include "Strategy.h"
#include <algorithm>
#include <cmath>
#include <iostream>


namespace lob {

// Base Strategy Implementation
void Strategy::update_position(double quantity, double price) {
  // Update PnL before changing position
  if (position_ != 0.0) {
    pnl_ += -quantity * (price - avg_entry_price_);
  }

  // Update position
  double new_position = position_ + quantity;

  // Update average entry price
  if (std::abs(new_position) > 1e-8) {
    avg_entry_price_ =
        ((position_ * avg_entry_price_) + (quantity * price)) / new_position;
  } else {
    avg_entry_price_ = 0.0;
  }

  position_ = new_position;
}

// Regime Aware Imbalance Strategy Implementation
RegimeAwareImbalanceStrategy::RegimeAwareImbalanceStrategy(
    double z_buy_threshold, double z_sell_threshold, size_t window_size)
    : Strategy("RegimeAwareImbalanceStrategy"),
      z_buy_threshold_(z_buy_threshold), z_sell_threshold_(z_sell_threshold),
      window_size_(window_size), alpha_decay_(0.5),
      spread_history_(window_size), imbalance_history_(window_size) {}

int RegimeAwareImbalanceStrategy::evaluate(const OrderBook &book,
                                           uint64_t timestamp) {
  // 1. Get current market data
  auto spread_opt = book.get_spread();
  if (!spread_opt)
    return 0;
  double spread = *spread_opt;

  double imbalance = calculate_weighted_imbalance(book);

  // 2. Update Rolling Statistics (O(log N) and O(1))
  spread_history_.add(spread);
  imbalance_history_.add(imbalance);

  // Need enough history to have valid statistics
  if (spread_history_.size() < window_size_ / 2)
    return 0;

  // 3. Condition 1: Spread Gating
  // Only trade if spread is tight (<= median spread)
  // This filters out high volatility / low liquidity periods
  double median_spread = spread_history_.get_median();

  if (spread > median_spread) {
    return 0; // Filter: Spread too wide, adverse selection risk high
  }

  // 4. Condition 2: Z-Score Normalization
  double mean = imbalance_history_.get_mean();
  double std_dev = imbalance_history_.get_std_dev();

  // Avoid division by zero
  if (std_dev < 1e-6)
    return 0;

  double z_score = (imbalance - mean) / std_dev;

  // 5. Condition 3: Directional Asymmetry Logic
  if (z_score > z_buy_threshold_) {
    return 1; // Buy Signal: Imbalance > Mean + Threshold * Std
  } else if (z_score < z_sell_threshold_) {
    return -1; // Sell Signal: Imbalance < Mean - Threshold * Std
  }

  return 0; // Hold
}

double RegimeAwareImbalanceStrategy::calculate_weighted_imbalance(
    const OrderBook &book) const {
  // Weight levels closer to touch higher: w_i = exp(-alpha * i)
  double bid_weighted_sum = 0.0;
  double ask_weighted_sum = 0.0;

  auto bid_depth = book.get_bid_depth(5);
  auto ask_depth = book.get_ask_depth(5);

  for (size_t i = 0; i < bid_depth.size(); ++i) {
    double weight = std::exp(-alpha_decay_ * i);
    bid_weighted_sum += bid_depth[i].second * weight;
  }

  for (size_t i = 0; i < ask_depth.size(); ++i) {
    double weight = std::exp(-alpha_decay_ * i);
    ask_weighted_sum += ask_depth[i].second * weight;
  }

  double total_weighted_volume = bid_weighted_sum + ask_weighted_sum;
  if (total_weighted_volume < 1e-8)
    return 0.0;

  // Normalized Imbalance [-1, 1]
  return (bid_weighted_sum - ask_weighted_sum) / total_weighted_volume;
}

} // namespace lob
