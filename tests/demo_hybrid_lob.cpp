#include "../engine/order_book/OrderBook.h"
#include <iomanip>
#include <iostream>


using namespace lob;

void print_separator() { std::cout << std::string(60, '=') << std::endl; }

void print_order_queue(const OrderBook &book, double price, Side side) {
  const auto &orders = book.get_orders_at_price(price, side);
  std::string side_str = (side == Side::BID) ? "BID" : "ASK";

  std::cout << "  Price " << price << " (" << side_str << ") - "
            << orders.size() << " orders:" << std::endl;

  if (orders.empty()) {
    std::cout << "    [Empty]" << std::endl;
    return;
  }

  for (size_t i = 0; i < orders.size(); ++i) {
    const auto &order = orders[i];
    std::cout << "    Order #" << order.order_id << ": qty=" << std::fixed
              << std::setprecision(2) << order.quantity << " @ " << order.price
              << " (ts=" << order.timestamp << ")" << std::endl;
  }

  double total = 0.0;
  for (const auto &order : orders) {
    total += order.quantity;
  }
  std::cout << "    Total Volume: " << total << std::endl;
}

void demo_l3_simulation() {
  std::cout << "\n";
  print_separator();
  std::cout << "Hybrid L2/L3 Order Book - Live Demonstration" << std::endl;
  print_separator();

  OrderBook book("BTCUSDT");

  // Scenario 1: Initial L2 update creates synthetic order
  std::cout << "\n[Step 1] L2 Update: BID @ 50000, Vol=1.5 BTC" << std::endl;
  book.update_order(50000.0, 1.5, Side::BID, 1000);
  print_order_queue(book, 50000.0, Side::BID);

  // Scenario 2: Volume increase adds new synthetic order
  std::cout << "\n[Step 2] L2 Update: BID @ 50000, Vol=2.3 BTC (+0.8)"
            << std::endl;
  book.update_order(50000.0, 2.3, Side::BID, 1001);
  print_order_queue(book, 50000.0, Side::BID);

  // Scenario 3: Another increase
  std::cout << "\n[Step 3] L2 Update: BID @ 50000, Vol=3.5 BTC (+1.2)"
            << std::endl;
  book.update_order(50000.0, 3.5, Side::BID, 1002);
  print_order_queue(book, 50000.0, Side::BID);

  // Scenario 4: Volume decrease (FIFO execution simulation)
  std::cout << "\n[Step 4] L2 Update: BID @ 50000, Vol=2.0 BTC (-1.5)"
            << std::endl;
  std::cout << "  → FIFO: First order (1.5) fully executed" << std::endl;
  book.update_order(50000.0, 2.0, Side::BID, 1003);
  print_order_queue(book, 50000.0, Side::BID);

  // Scenario 5: Partial execution
  std::cout << "\n[Step 5] L2 Update: BID @ 50000, Vol=1.3 BTC (-0.7)"
            << std::endl;
  std::cout << "  → FIFO: Second order (0.8) fully executed, third order "
               "partially (0.1)"
            << std::endl;
  book.update_order(50000.0, 1.3, Side::BID, 1004);
  print_order_queue(book, 50000.0, Side::BID);

  // Scenario 6: Multiple price levels
  std::cout << "\n[Step 6] Building multi-level book" << std::endl;
  book.update_order(50100.0, 0.5, Side::BID, 1005);
  book.update_order(49900.0, 1.0, Side::BID, 1006);
  book.update_order(50200.0, 0.8, Side::ASK, 1007);
  book.update_order(50300.0, 1.2, Side::ASK, 1008);

  std::cout << "\nBID Side:" << std::endl;
  print_order_queue(book, 50100.0, Side::BID);
  print_order_queue(book, 50000.0, Side::BID);
  print_order_queue(book, 49900.0, Side::BID);

  std::cout << "\nASK Side:" << std::endl;
  print_order_queue(book, 50200.0, Side::ASK);
  print_order_queue(book, 50300.0, Side::ASK);

  // Show market data
  std::cout << "\nMarket Data:" << std::endl;
  auto best_bid = book.get_best_bid();
  auto best_ask = book.get_best_ask();
  auto mid = book.get_mid_price();
  auto spread = book.get_spread();

  if (best_bid && best_ask) {
    std::cout << "  Best Bid: $" << *best_bid << std::endl;
    std::cout << "  Best Ask: $" << *best_ask << std::endl;
    std::cout << "  Mid Price: $" << *mid << std::endl;
    std::cout << "  Spread: $" << *spread << std::endl;
  }

  print_separator();
  std::cout << "✓ Demonstration Complete!" << std::endl;
  std::cout << "\nKey Features Demonstrated:" << std::endl;
  std::cout << "  • L2 updates create synthetic L3 orders" << std::endl;
  std::cout << "  • Volume increases add new orders to queue" << std::endl;
  std::cout << "  • Volume decreases execute FIFO from front" << std::endl;
  std::cout << "  • Multiple price levels maintained correctly" << std::endl;
  std::cout << "  • Order IDs auto-increment for traceability" << std::endl;
  print_separator();
}

int main() {
  demo_l3_simulation();
  return 0;
}
