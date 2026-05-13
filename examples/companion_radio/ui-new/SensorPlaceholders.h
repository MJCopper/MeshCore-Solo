#pragma once
// Populates a KeyboardWidget's placeholder list with sensor types currently
// detected by SensorManager. Always adds {loc} and {time} via begin(); this
// helper appends the sensor-specific entries on top.

#include "KeyboardWidget.h"
#include <helpers/SensorManager.h>
#include <helpers/sensors/LPPDataHelpers.h>

inline void kbAddSensorPlaceholders(KeyboardWidget& kb, SensorManager* sm) {
  if (!sm) return;
  uint8_t types[16];
  int tc = sm->getAvailableLPPTypes(types, 16);

  static const struct { uint8_t t; const char* ph; } MAP[] = {
    { LPP_TEMPERATURE,         "{temp}" },
    { LPP_RELATIVE_HUMIDITY,   "{hum}"  },
    { LPP_BAROMETRIC_PRESSURE, "{pres}" },
    { LPP_VOLTAGE,             "{batt}" },
    { LPP_ALTITUDE,            "{alt}"  },
    { LPP_LUMINOSITY,          "{lux}"  },
    { LPP_DISTANCE,            "{dist}" },
    { LPP_CONCENTRATION,       "{co2}"  },
  };

  for (const auto& m : MAP) {
    for (int i = 0; i < tc; i++) {
      if (types[i] == m.t) { kb.addPlaceholder(m.ph); break; }
    }
  }
}
