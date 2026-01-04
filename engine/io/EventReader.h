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
struct Event {
  uint64_t exchange_ts;
  uint64_t local_ts;
  std::string event_type; // SNAPSHOT, UPDATE
  double price;
  double quantity;
  Side side;

  Event()
      : exchange_ts(0), local_ts(0), price(0.0), quantity(0.0),
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
