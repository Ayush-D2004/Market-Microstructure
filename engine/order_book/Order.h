#pragma once

#include <cstdint>
#include <string>
#include <deque>
#include <cmath>
#include <cassert>

namespace lob {

// Order side enumeration
enum class Side { BID, ASK };

// Individual order structure
struct Order {
  uint64_t order_id;
  double price;
  double quantity;
  Side side;
  uint64_t timestamp;

  Order(uint64_t id, double p, double q, Side s, uint64_t ts)
      : order_id(id), price(p), quantity(q), side(s), timestamp(ts) {}
};

// Price level (Limit) structure with L3 simulation
struct Limit {
  double price;
  double total_volume;
  uint32_t order_count;
  
  // L3 simulation: deque of individual orders (FIFO semantics)
  std::deque<Order> orders;

  // Default constructor (needed for std::map::operator[])
  Limit() : price(0.0), total_volume(0.0), order_count(0) {}

  Limit(double p) : price(p), total_volume(0.0), order_count(0) {}

  // Add a synthetic order to the back of the queue
  void add_synthetic_order(uint64_t order_id, double qty, Side side, uint64_t timestamp) {
    orders.emplace_back(order_id, price, qty, side, timestamp);
    total_volume += qty;
    order_count = static_cast<uint32_t>(orders.size());
  }

  // Reduce volume from front of queue (FIFO)
  // Returns the amount actually removed
  double reduce_volume_fifo(double qty_to_remove) {
    double removed = 0.0;
    
    while (qty_to_remove > 1e-8 && !orders.empty()) {
      Order& front = orders.front();
      
      if (front.quantity <= qty_to_remove) {
        // Remove entire order
        removed += front.quantity;
        qty_to_remove -= front.quantity;
        orders.pop_front();
      } else {
        // Partial reduction
        front.quantity -= qty_to_remove;
        removed += qty_to_remove;
        qty_to_remove = 0.0;
      }
    }
    
    total_volume -= removed;
    order_count = static_cast<uint32_t>(orders.size());
    return removed;
  }

  // Clear all orders at this level
  void clear() {
    orders.clear();
    total_volume = 0.0;
    order_count = 0;
  }

  // Validate invariants (debug builds)
  void validate_invariants() const {
    #ifndef NDEBUG
    // Invariant 1: Volume consistency
    double sum = 0.0;
    for (const auto& order : orders) {
      sum += order.quantity;
      // Invariant 2: Non-negative quantities
      assert(order.quantity >= 0.0);
    }
    assert(std::abs(sum - total_volume) < 1e-6);
    
    // Invariant 3: Empty state
    assert((orders.empty() && total_volume < 1e-8) || (!orders.empty() && total_volume >= 1e-8));
    
    // Invariant 4: Order count matches
    assert(order_count == orders.size());
    #endif
  }

  // Legacy methods (kept for compatibility)
  void add_volume(double qty) {
    total_volume += qty;
    order_count++;
  }

  void remove_volume(double qty) {
    total_volume -= qty;
    if (order_count > 0)
      order_count--;
  }

  void update_volume(double new_qty) { total_volume = new_qty; }
};

} // namespace lob
