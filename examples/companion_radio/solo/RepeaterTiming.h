#pragma once

#include <cmath>
#include <cstdint>

namespace solo {

// Radio timing shared by the companion's optional repeater backend. Defaults
// and limits intentionally match examples/simple_repeater/CommonCLI.
struct RepeaterTiming {
  static constexpr float DEFAULT_RX_DELAY_BASE = 10.0f;
  static constexpr float DEFAULT_FLOOD_TX_FACTOR = 0.5f;
  static constexpr float DEFAULT_DIRECT_TX_FACTOR = 0.3f;
  static constexpr uint8_t DEFAULT_YIELD_BOOST = 1;       // displayed as x2
  static constexpr uint8_t DEFAULT_SUPPRESS_DUP = 1;
  static constexpr float MAX_RX_DELAY_BASE = 20.0f;
  static constexpr float MAX_TX_FACTOR = 2.0f;

  static float validOrDefault(float value, float maximum, float fallback) {
    return std::isfinite(value) && value >= 0.0f && value <= maximum ? value : fallback;
  }

  static uint32_t delayWindow(uint32_t airtime, float factor) {
    return static_cast<uint32_t>(airtime * factor);
  }
};

} // namespace solo
