#include "Strategy.h"
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

// Imbalance Strategy Implementation
ImbalanceStrategy::ImbalanceStrategy(double threshold, size_t depth)
    : Strategy("ImbalanceStrategy"), threshold_(threshold), depth_(depth),
      last_imbalance_(0.0) {}

int ImbalanceStrategy::evaluate(const OrderBook &book, uint64_t timestamp) {
  // Calculate order book imbalance
  double imbalance = book.calculate_imbalance(depth_);
  last_imbalance_ = imbalance;

  // Trading logic:
  // If imbalance > threshold: more bids than asks -> expect price to rise ->
  // BUY If imbalance < -threshold: more asks than bids -> expect price to fall
  // -> SELL

  if (imbalance > threshold_) {
    return 1; // Buy signal
  } else if (imbalance < -threshold_) {
    return -1; // Sell signal
  }

  return 0; // Hold
}

// Market Making Strategy Implementation
MarketMakingStrategy::MarketMakingStrategy(double risk_aversion,
                                           double inventory_limit)
    : Strategy("MarketMakingStrategy"), risk_aversion_(risk_aversion),
      inventory_limit_(inventory_limit), reservation_price_(0.0) {}

int MarketMakingStrategy::evaluate(const OrderBook &book, uint64_t timestamp) {
  // Calculate reservation price
  reservation_price_ = calculate_reservation_price(book);

  auto mid_price = book.get_mid_price();
  if (!mid_price)
    return 0;

  // Inventory management logic
  // If we have too much inventory, we want to sell
  // If we have negative inventory (short), we want to buy

  double inventory_ratio = position_ / inventory_limit_;

  // Aggressive inventory reduction
  if (inventory_ratio > 0.7) {
    return -1; // Sell to reduce long position
  } else if (inventory_ratio < -0.7) {
    return 1; // Buy to reduce short position
  }

  // Market making: provide liquidity on both sides
  // This is simplified - in reality, we'd place limit orders
  // Here we just signal when to take liquidity based on reservation price

  if (*mid_price < reservation_price_ - 0.0001) {
    return 1; // Price is below our reservation -> buy
  } else if (*mid_price > reservation_price_ + 0.0001) {
    return -1; // Price is above our reservation -> sell
  }

  return 0; // Hold
}

double
MarketMakingStrategy::calculate_reservation_price(const OrderBook &book) {
  auto mid_price = book.get_mid_price();
  if (!mid_price)
    return 0.0;

  // Simplified Avellaneda-Stoikov reservation price
  // r = mid_price - q * gamma * sigma^2 * (T - t)
  // Where q = inventory, gamma = risk aversion
  // For simplicity, we use: r = mid_price - q * gamma

  double inventory_adjustment = position_ * risk_aversion_;
  return *mid_price - inventory_adjustment;
}

} // namespace lob
