#pragma once

#include <stdint.h>
#include <string.h>

namespace solo {

// Small, RAM-only record of operational failures. Consecutive identical
// failures are folded into one entry so a noisy background task cannot evict
// every other useful diagnostic. Nothing is persisted to flash.
class DiagnosticLog {
public:
  static const uint8_t CAPACITY = 16;
  enum Severity : uint8_t { INFO, WARNING, ERROR };
  struct Entry {
    uint32_t timestamp;
    char operation[12];
    char reason[24];
    uint8_t count;
    Severity severity;
  };

private:
  Entry _entries[CAPACITY]{};
  uint8_t _head = 0;
  uint8_t _size = 0;

public:
  uint8_t size() const { return _size; }
  void clear() { memset(_entries, 0, sizeof(_entries)); _head = _size = 0; }

  void add(uint32_t timestamp, Severity severity, const char* operation,
           const char* reason) {
    if (!operation || !reason) return;
    if (_size) {
      Entry& last = _entries[(_head + CAPACITY - 1) % CAPACITY];
      if (last.severity == severity && !strcmp(last.operation, operation) &&
          !strcmp(last.reason, reason)) {
        last.timestamp = timestamp;
        if (last.count < 255) last.count++;
        return;
      }
    }
    Entry& entry = _entries[_head];
    entry.timestamp = timestamp;
    entry.severity = severity;
    strncpy(entry.operation, operation, sizeof(entry.operation) - 1);
    entry.operation[sizeof(entry.operation) - 1] = 0;
    strncpy(entry.reason, reason, sizeof(entry.reason) - 1);
    entry.reason[sizeof(entry.reason) - 1] = 0;
    entry.count = 1;
    _head = (_head + 1) % CAPACITY;
    if (_size < CAPACITY) _size++;
  }

  void add(uint32_t timestamp, const char* operation, const char* reason) {
    add(timestamp, ERROR, operation, reason);
  }

  // index 0 is the newest event.
  const Entry* newest(uint8_t index) const {
    if (index >= _size) return nullptr;
    return &_entries[(_head + CAPACITY - 1 - index) % CAPACITY];
  }
};

} // namespace solo
