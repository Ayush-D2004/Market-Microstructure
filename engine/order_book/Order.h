#pragma once

#include <cstdint>
#include <string>

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

// Price level (Limit) structure
struct Limit {
  double price;
  double total_volume;
  uint32_t order_count;

  // Default constructor (needed for std::map::operator[])
  Limit() : price(0.0), total_volume(0.0), order_count(0) {}

  Limit(double p) : price(p), total_volume(0.0), order_count(0) {}

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
