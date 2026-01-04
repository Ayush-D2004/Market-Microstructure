#pragma once

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "../order_book/Order.h"
#include <fstream>
#include <optional>
#include <string>

namespace lob {

// Parsed event structure
// New format:
// exchange_seq|exchange_event_ts|local_ingest_ts|event_type|price|qty|side
struct Event {
  uint64_t exchange_seq;  // Sequence number from exchange
  uint64_t exchange_ts;   // Event timestamp from exchange (ms)
  uint64_t local_ts;      // Local ingestion timestamp (ms)
  std::string event_type; // SNAPSHOT, UPDATE
  double price;
  double quantity;
  Side side;

  Event()
      : exchange_seq(0), exchange_ts(0), local_ts(0), price(0.0), quantity(0.0),
        side(Side::BID) {}
};

class EventReader {
public:
  explicit EventReader(const std::string &filepath);
  ~EventReader();

  // Read next event from file
  std::optional<Event> read_next();

  // Check if more events are available
  bool has_more() const;

  // Reset to beginning of file
  void reset();

private:
  std::string filepath_;
  std::ifstream file_;

  // Parse a line into an Event
  std::optional<Event> parse_line(const std::string &line);
};

} // namespace lob
