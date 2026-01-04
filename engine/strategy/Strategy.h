#pragma once

#include "../order_book/OrderBook.h"
#include <memory>
#include <string>


namespace lob {

// Base strategy class
class Strategy {
public:
  explicit Strategy(const std::string &name)
      : name_(name), position_(0.0), pnl_(0.0) {}
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

// Order Book Imbalance Strategy
class ImbalanceStrategy : public Strategy {
public:
  ImbalanceStrategy(double threshold = 0.3, size_t depth = 5);

  int evaluate(const OrderBook &book, uint64_t timestamp) override;

private:
  double threshold_; // Imbalance threshold to trigger trade
  size_t depth_;     // Number of levels to consider
  double last_imbalance_;
};

// Simple Market Making Strategy (Simplified Avellaneda-Stoikov)
class MarketMakingStrategy : public Strategy {
public:
  MarketMakingStrategy(double risk_aversion = 0.1,
                       double inventory_limit = 10.0);

  int evaluate(const OrderBook &book, uint64_t timestamp) override;

private:
  double risk_aversion_;
  double inventory_limit_;
  double reservation_price_;

  // Calculate reservation price based on inventory
  double calculate_reservation_price(const OrderBook &book);
};

} // namespace lob
