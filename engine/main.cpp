#include "io/EventReader.h"
#include "metrics/Metrics.h"
#include "order_book/OrderBook.h"
#include "strategy/Strategy.h"
#include <chrono>
#include <iostream>
#include <memory>

using namespace lob;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <event_file>" << std::endl;
    return 1;
  }

  std::string event_file = argv[1];
  std::string asset = "BTCUSDT"; // Can be extracted from filename

  std::cout << "=== Market Microstructure Engine ===" << std::endl;
  std::cout << "[INFO] Processing events from: " << event_file << std::endl;

  // Initialize components
  OrderBook order_book(asset);
  EventReader reader(event_file);
  MetricsLogger metrics(asset, "../../logs");

  // Initialize strategy (choose one)
  std::unique_ptr<Strategy> strategy =
      std::make_unique<ImbalanceStrategy>(0.3, 5);
  // Or use: std::make_unique<MarketMakingStrategy>(0.1, 10.0);

  std::cout << "[INFO] Using strategy: " << strategy->get_name() << std::endl;

  // Performance counters
  uint64_t events_processed = 0;
  uint64_t total_latency_us = 0;

  // Event processing loop
  while (reader.has_more()) {
    auto event_opt = reader.read_next();
    if (!event_opt)
      continue;

    Event event = *event_opt;

    // Start processing timer
    auto processing_start = std::chrono::high_resolution_clock::now();

    // Update order book
    order_book.update_order(event.price, event.quantity, event.side,
                            event.exchange_ts);

    // Evaluate strategy every N events (to reduce noise)
    if (events_processed % 10 == 0) {
      int signal = strategy->evaluate(order_book, event.local_ts);

      // Execute trade based on signal
      if (signal != 0) {
        auto mid_price = order_book.get_mid_price();
        if (mid_price) {
          double trade_quantity = signal * 0.01; // Trade 0.01 BTC
          strategy->update_position(trade_quantity, *mid_price);

          // Log trade
          std::string side = (signal > 0) ? "BUY" : "SELL";
          metrics.log_trade(event.local_ts, *mid_price,
                            std::abs(trade_quantity), side);

          // Log inventory and PnL
          metrics.log_inventory(event.local_ts, strategy->get_position(),
                                strategy->get_pnl());
          metrics.log_pnl(event.local_ts, strategy->get_pnl(),
                          strategy->get_pnl(), 0.0);
        }
      }
    }

    // Log order book state periodically
    if (events_processed % 100 == 0) {
      auto best_bid = order_book.get_best_bid();
      auto best_ask = order_book.get_best_ask();
      auto mid_price = order_book.get_mid_price();
      auto spread = order_book.get_spread();
      double imbalance = order_book.calculate_imbalance(5);

      if (best_bid && best_ask && mid_price && spread) {
        metrics.log_order_book_state(event.local_ts, *best_bid, *best_ask,
                                     *mid_price, *spread, imbalance);
      }
    }

    // Calculate processing latency
    auto processing_end = std::chrono::high_resolution_clock::now();
    auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                          processing_end - processing_start)
                          .count();

    total_latency_us += latency_us;

    // Log latency every 1000 events
    if (events_processed % 1000 == 0) {
      metrics.log_latency(event.exchange_ts, event.local_ts,
                          event.local_ts + latency_us);
    }

    events_processed++;

    // Progress indicator
    if (events_processed % 10000 == 0) {
      std::cout << "[INFO] Processed " << events_processed << " events"
                << std::endl;
    }
  }

  // Final statistics
  std::cout << "\n=== Processing Complete ===" << std::endl;
  std::cout << "[STATS] Total events processed: " << events_processed
            << std::endl;

  if (events_processed > 0) {
    double avg_latency =
        static_cast<double>(total_latency_us) / events_processed;
    std::cout << "[STATS] Average processing latency: " << avg_latency << " μs"
              << std::endl;
  }

  std::cout << "[STATS] Final position: " << strategy->get_position()
            << std::endl;
  std::cout << "[STATS] Final PnL: $" << strategy->get_pnl() << std::endl;

  auto best_bid = order_book.get_best_bid();
  auto best_ask = order_book.get_best_ask();
  if (best_bid && best_ask) {
    std::cout << "[STATS] Final best bid: $" << *best_bid << std::endl;
    std::cout << "[STATS] Final best ask: $" << *best_ask << std::endl;
  }

  metrics.flush();
  std::cout << "[INFO] Metrics written to ./logs/" << std::endl;

  return 0;
}
