#pragma once

#include <stdint.h>
#include <string.h>

namespace solo {

// Login replies carry server time, not the request tag. Give tagged app
// responses priority and accept only the known login payload shapes.
struct RoomLoginResponse {
  static bool valid(const uint8_t* data, uint8_t len) {
    if (!data || len < 6) return false;
    if (len == 6 && memcmp(data + 4, "OK", 2) == 0) return true;
    return (len == 8 || len == 12 || len == 13) && data[4] == 0;
  }

  static bool busy(uint32_t pending, uint32_t deadline, uint32_t now) {
    return pending && (int32_t)(now - deadline) < 0;
  }
};

} // namespace solo
