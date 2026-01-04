#include "EventReader.h"
#include <iostream>
#include <sstream>


namespace lob {

EventReader::EventReader(const std::string &filepath)
    : filepath_(filepath), file_(filepath) {
  if (!file_.is_open()) {
    std::cerr << "[ERROR] Failed to open file: " << filepath << std::endl;
  }
}

EventReader::~EventReader() {
  if (file_.is_open()) {
    file_.close();
  }
}

std::optional<Event> EventReader::read_next() {
  if (!file_.is_open() || file_.eof()) {
    return std::nullopt;
  }

  std::string line;
  if (std::getline(file_, line)) {
    return parse_line(line);
  }

  return std::nullopt;
}

bool EventReader::has_more() const { return file_.is_open() && !file_.eof(); }

void EventReader::reset() {
  file_.clear();
  file_.seekg(0, std::ios::beg);
}

std::optional<Event> EventReader::parse_line(const std::string &line) {
  // Format: [exchange_ts]|[local_ts]|[event_type]|[price]|[qty]|[side]
  std::istringstream ss(line);
  std::string token;
  Event event;

  int field_idx = 0;
  while (std::getline(ss, token, '|')) {
    try {
      switch (field_idx) {
      case 0: // exchange_ts
        event.exchange_ts = std::stoull(token);
        break;
      case 1: // local_ts
        event.local_ts = std::stoull(token);
        break;
      case 2: // event_type
        event.event_type = token;
        break;
      case 3: // price
        event.price = std::stod(token);
        break;
      case 4: // quantity
        event.quantity = std::stod(token);
        break;
      case 5: // side
        event.side = (token == "BID") ? Side::BID : Side::ASK;
        break;
      }
      field_idx++;
    } catch (const std::exception &e) {
      std::cerr << "[ERROR] Failed to parse field " << field_idx
                << " in line: " << line << std::endl;
      return std::nullopt;
    }
  }

  if (field_idx != 6) {
    std::cerr << "[ERROR] Invalid event format (expected 6 fields): " << line
              << std::endl;
    return std::nullopt;
  }

  return event;
}

} // namespace lob
