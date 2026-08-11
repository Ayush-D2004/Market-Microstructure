#include "io/EventReader.h"
#include "metrics/Metrics.h"
#include "order_book/OrderBook.h"
#include "strategy/RollingMath.h"
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

  // Initialize strategy (Research-Grade Regime Aware Imbalance)
  std::unique_ptr<Strategy> strategy =
      std::make_unique<RegimeAwareImbalanceStrategy>(1.5, -1.5, 100);
  // Params: Z-Score Buy > 1.5, Sell < -1.5, Window = 100 events

  // Initialize Quant Feature tracker
  RollingVariance realized_vol_tracker(100);

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
        auto mid_price_opt = order_book.get_mid_price();
        if (mid_price_opt) {
          double intended_price = *mid_price_opt;
          double trade_quantity = std::abs(signal * 0.01); // Trade 0.01 BTC

          // Calculate slippage by sweeping the order book
          double actual_fill_price = 0.0;
          double remaining_qty = trade_quantity;

          if (signal > 0) { // BUY -> cross spread to MATCH Asks
            auto asks = order_book.get_ask_depth(20);
            double total_cost = 0;
            for (const auto &level : asks) {
              double px = level.first;
              double vol = level.second;
              if (remaining_qty <= vol) {
                total_cost += remaining_qty * px;
                remaining_qty = 0;
                break;
              } else {
                total_cost += vol * px;
                remaining_qty -= vol;
              }
            }
            if (remaining_qty == 0) {
              actual_fill_price = total_cost / trade_quantity;
            } else {
              actual_fill_price = intended_price;
            }
          } else { // SELL -> cross spread to MATCH Bids
            auto bids = order_book.get_bid_depth(20);
            double total_revenue = 0;
            for (const auto &level : bids) {
              double px = level.first;
              double vol = level.second;
              if (remaining_qty <= vol) {
                total_revenue += remaining_qty * px;
                remaining_qty = 0;
                break;
              } else {
                total_revenue += vol * px;
                remaining_qty -= vol;
              }
            }
            if (remaining_qty == 0) {
              actual_fill_price = total_revenue / trade_quantity;
            } else {
              actual_fill_price = intended_price;
            }
          }

          strategy->update_position(signal * trade_quantity, actual_fill_price);

          // Calculate slippage in basis points
          double slippage_bps = 0.0;
          if (signal > 0) {
            slippage_bps =
                (actual_fill_price - intended_price) / intended_price * 10000;
          } else {
            slippage_bps =
                (intended_price - actual_fill_price) / intended_price * 10000;
          }

          // Log trade
          std::string side = (signal > 0) ? "BUY" : "SELL";
          metrics.log_trade(event.local_ts, actual_fill_price, intended_price,
                            trade_quantity, side, slippage_bps);

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
    uint64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              processing_end - processing_start)
                              .count();

    total_latency_us += latency_us;

    // --- Quant Features Generation (Shared State Bridge) ---
    auto mid_price_opt = order_book.get_mid_price();
    if (mid_price_opt) {
      double mid_price = *mid_price_opt;
      realized_vol_tracker.add(mid_price);

      QuantFeatures features;
      features.timestamp = event.local_ts;
      features.mid_price = mid_price;
      features.spread = order_book.get_spread().value_or(0.0);
      features.imbalance = order_book.calculate_imbalance(5);
      features.depth_slope = order_book.calculate_depth_slope(5);
      features.realized_vol = realized_vol_tracker.get_std_dev();
      features.system_latency = latency_us;

      metrics.log_quant_features(features);
    }

    // Log latency every 1000 events
    if (events_processed % 1000 == 0) {
      metrics.log_latency(event.exchange_ts, event.local_ts,
                          event.local_ts + (latency_us / 1000));
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
