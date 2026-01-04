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
  // New format:
  // [exchange_seq]|[exchange_event_ts]|[local_ingest_ts]|[event_type]|[price]|[qty]|[side]
  std::istringstream ss(line);
  std::string token;
  Event event;

  int field_idx = 0;
  while (std::getline(ss, token, '|')) {
    try {
      switch (field_idx) {
      case 0: // exchange_seq (sequence number)
        event.exchange_seq = std::stoull(token);
        break;
      case 1: // exchange_event_ts (event timestamp from exchange)
        event.exchange_ts = std::stoull(token);
        break;
      case 2: // local_ingest_ts (local ingestion timestamp)
        event.local_ts = std::stoull(token);
        break;
      case 3: // event_type
        event.event_type = token;
        break;
      case 4: // price
        event.price = std::stod(token);
        break;
      case 5: // quantity
        event.quantity = std::stod(token);
        break;
      case 6: // side
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

  if (field_idx != 7) {
    std::cerr << "[ERROR] Invalid event format (expected 7 fields, got "
              << field_idx << "): " << line << std::endl;
    return std::nullopt;
  }

  return event;
}

} // namespace lob
