#pragma once

#include <stdint.h>

namespace solo {

// Fixed delivery policy for DMs composed on the device. A known route gets two
// direct tries in total, then three flood tries. An unknown route gets three
// flood tries in total.
struct DmRetryPolicy {
  static constexpr uint8_t DIRECT_RETRIES_AFTER_INITIAL = 1;
  static constexpr uint8_t FALLBACK_FLOOD_TRIES = 3;
  static constexpr uint8_t INITIAL_FLOOD_RETRIES = 2;
};

} // namespace solo
