#include "../engine/order_book/OrderBook.h"
#include <cassert>
#include <iomanip>
#include <iostream>


using namespace lob;

// Helper function to print order queue
void print_orders(const std::deque<Order> &orders, const std::string &label) {
  std::cout << label << ": [";
  for (size_t i = 0; i < orders.size(); ++i) {
    std::cout << orders[i].quantity;
    if (i < orders.size() - 1)
      std::cout << ", ";
  }
  std::cout << "]" << std::endl;
}

// Test Case 1: Volume Increase (New Order)
void test_case_1() {
  std::cout << "\n=== Test Case 1: Volume Increase (New Order) ==="
            << std::endl;
  OrderBook book("BTCUSDT");

  // Initial: Price 100, Vol 0 -> Update: Price 100, Vol 50
  book.update_order(100.0, 50.0, Side::BID, 1000);

  auto orders = book.get_orders_at_price(100.0, Side::BID);
  print_orders(orders, "Orders at 100");

  assert(orders.size() == 1);
  assert(orders[0].quantity == 50.0);
  assert(book.get_bid_volume(100.0) == 50.0);

  std::cout << " PASSED: One order with size 50, Total Vol 50" << std::endl;
}

// Test Case 2: Volume Increase (Another Order)
void test_case_2() {
  std::cout << "\n=== Test Case 2: Volume Increase (Another Order) ==="
            << std::endl;
  OrderBook book("BTCUSDT");

  book.update_order(100.0, 50.0, Side::BID, 1000);
  book.update_order(100.0, 80.0, Side::BID, 1001); // Delta +30

  auto orders = book.get_orders_at_price(100.0, Side::BID);
  print_orders(orders, "Orders at 100");

  assert(orders.size() == 2);
  assert(orders[0].quantity == 50.0);
  assert(orders[1].quantity == 30.0);
  assert(book.get_bid_volume(100.0) == 80.0);

  std::cout << " PASSED: Two orders [50, 30], Total Vol 80" << std::endl;
}

// Test Case 3: Volume Decrease (Partial Fill - FIFO)
void test_case_3() {
  std::cout << "\n=== Test Case 3: Volume Decrease (Partial Fill - FIFO) ==="
            << std::endl;
  OrderBook book("BTCUSDT");

  book.update_order(100.0, 50.0, Side::BID, 1000);
  book.update_order(100.0, 80.0, Side::BID, 1001); // [50, 30]
  book.update_order(100.0, 60.0, Side::BID, 1002); // Delta -20

  auto orders = book.get_orders_at_price(100.0, Side::BID);
  print_orders(orders, "Orders at 100");

  assert(orders.size() == 2);
  assert(orders[0].quantity == 30.0); // First order reduced by 20
  assert(orders[1].quantity == 30.0);
  assert(book.get_bid_volume(100.0) == 60.0);

  std::cout << " PASSED: Orders [30, 30], Total Vol 60" << std::endl;
}

// Test Case 4: Volume Decrease (Full Fill + Partial)
void test_case_4() {
  std::cout << "\n=== Test Case 4: Volume Decrease (Full Fill + Partial) ==="
            << std::endl;
  OrderBook book("BTCUSDT");

  book.update_order(100.0, 50.0, Side::BID, 1000);
  book.update_order(100.0, 80.0, Side::BID, 1001); // [50, 30]
  book.update_order(100.0, 60.0, Side::BID, 1002); // [30, 30]
  book.update_order(100.0, 10.0, Side::BID, 1003); // Delta -50

  auto orders = book.get_orders_at_price(100.0, Side::BID);
  print_orders(orders, "Orders at 100");

  assert(orders.size() == 1);
  assert(orders[0].quantity ==
         10.0); // Order 1 (30) removed, Order 2 reduced by 20
  assert(book.get_bid_volume(100.0) == 10.0);

  std::cout << " PASSED: One order [10], Total Vol 10" << std::endl;
}

// Test Case 5: Over-reduction (Safety)
void test_case_5() {
  std::cout << "\n=== Test Case 5: Over-reduction (Safety) ===" << std::endl;
  OrderBook book("BTCUSDT");

  book.update_order(100.0, 50.0, Side::BID, 1000);
  book.update_order(100.0, 0.0, Side::BID, 1001); // Clear level

  auto orders = book.get_orders_at_price(100.0, Side::BID);
  print_orders(orders, "Orders at 100");

  assert(orders.empty());
  assert(book.get_bid_volume(100.0) == 0.0);

  std::cout << " PASSED: Price level cleared, deque empty, total vol 0"
            << std::endl;
}

// Test Case 6: Snapshot Rebuild
void test_case_6() {
  std::cout << "\n=== Test Case 6: Snapshot Rebuild ===" << std::endl;
  OrderBook book("BTCUSDT");

  // Simulate existing state
  book.update_order(100.0, 50.0, Side::BID, 1000);
  book.update_order(100.0, 80.0, Side::BID, 1001);

  // Clear and rebuild from snapshot
  book.clear();
  book.update_order(100.0, 100.0, Side::BID, 2000); // Fresh snapshot

  auto orders = book.get_orders_at_price(100.0, Side::BID);
  print_orders(orders, "Orders at 100");

  assert(orders.size() == 1);
  assert(orders[0].quantity == 100.0);
  assert(book.get_bid_volume(100.0) == 100.0);

  std::cout << " PASSED: Queue rebuilt as single synthetic order" << std::endl;
}

// Test Case 7: Resync Reset
void test_case_7() {
  std::cout << "\n=== Test Case 7: Resync Reset ===" << std::endl;
  OrderBook book("BTCUSDT");

  // Build up state
  book.update_order(100.0, 50.0, Side::BID, 1000);
  book.update_order(101.0, 30.0, Side::BID, 1001);
  book.update_order(99.0, 20.0, Side::ASK, 1002);

  // Simulate resync
  book.clear();

  auto orders_100 = book.get_orders_at_price(100.0, Side::BID);
  auto orders_101 = book.get_orders_at_price(101.0, Side::BID);
  auto orders_99 = book.get_orders_at_price(99.0, Side::ASK);

  assert(orders_100.empty());
  assert(orders_101.empty());
  assert(orders_99.empty());
  assert(book.get_bid_volume(100.0) == 0.0);

  std::cout << " PASSED: All queues cleared, no stale orders" << std::endl;
}

// Test Case 8: Multiple Price Levels (Integration)
void test_case_8() {
  std::cout << "\n=== Test Case 8: Multiple Price Levels (Integration) ==="
            << std::endl;
  OrderBook book("BTCUSDT");

  // Build book with multiple levels
  book.update_order(100.0, 50.0, Side::BID, 1000);
  book.update_order(99.0, 30.0, Side::BID, 1001);
  book.update_order(101.0, 40.0, Side::ASK, 1002);
  book.update_order(102.0, 20.0, Side::ASK, 1003);

  // Verify best bid/ask
  auto best_bid = book.get_best_bid();
  auto best_ask = book.get_best_ask();

  assert(best_bid.has_value() && *best_bid == 100.0);
  assert(best_ask.has_value() && *best_ask == 101.0);

  // Update existing level
  book.update_order(100.0, 70.0, Side::BID, 1004); // +20
  auto orders = book.get_orders_at_price(100.0, Side::BID);

  assert(orders.size() == 2);
  assert(orders[0].quantity == 50.0);
  assert(orders[1].quantity == 20.0);

  std::cout << " PASSED: Multiple levels work correctly" << std::endl;
}

// Test Case 9: FIFO Order Preservation
void test_case_9() {
  std::cout << "\n=== Test Case 9: FIFO Order Preservation ===" << std::endl;
  OrderBook book("BTCUSDT");

  // Add multiple increments
  book.update_order(100.0, 10.0, Side::BID, 1000); // Order 1: 10
  book.update_order(100.0, 25.0, Side::BID, 1001); // Order 2: 15
  book.update_order(100.0, 45.0, Side::BID, 1002); // Order 3: 20
  book.update_order(100.0, 70.0, Side::BID, 1003); // Order 4: 25

  auto orders = book.get_orders_at_price(100.0, Side::BID);
  print_orders(orders, "Orders at 100 (before reduction)");

  assert(orders.size() == 4);
  assert(orders[0].quantity == 10.0);
  assert(orders[1].quantity == 15.0);
  assert(orders[2].quantity == 20.0);
  assert(orders[3].quantity == 25.0);

  // Reduce by 30 (should remove first two orders and reduce third)
  book.update_order(100.0, 40.0, Side::BID, 1004); // -30

  orders = book.get_orders_at_price(100.0, Side::BID);
  print_orders(orders, "Orders at 100 (after reduction)");

  assert(orders.size() == 2);
  assert(orders[0].quantity == 15.0); // Third order reduced by 5
  assert(orders[1].quantity == 25.0); // Fourth order unchanged

  std::cout << " PASSED: FIFO order preserved correctly" << std::endl;
}

int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "Hybrid L2/L3 Order Book Test Suite" << std::endl;
  std::cout << "========================================" << std::endl;

  try {
    test_case_1();
    test_case_2();
    test_case_3();
    test_case_4();
    test_case_5();
    test_case_6();
    test_case_7();
    test_case_8();
    test_case_9();

    std::cout << "\n========================================" << std::endl;
    std::cout << " ALL TESTS PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "\n✗ TEST FAILED: " << e.what() << std::endl;
    return 1;
  }
}
