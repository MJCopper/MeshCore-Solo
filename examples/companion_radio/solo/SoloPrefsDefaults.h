#pragma once

#include "../NodePrefs.h"

// Defaults and validation for preferences introduced by the Solo extension.
// Persistence code only reads/writes bytes; runtime code no longer needs to
// duplicate the valid ranges or migration fallbacks.
namespace solo {

class PrefsDefaults {
public:
  static void apply(NodePrefs& prefs) {
    prefs.child_visible_pages = NodePrefs::HP_FAVOURITES;
    prefs.quiet_time_start_min = 21 * 60;
    prefs.quiet_time_end_min = 7 * 60;
    prefs.bluetooth_enabled = 1;
  }

  static void normalize(NodePrefs& prefs) {
    if (prefs.child_mode_enabled > 1) prefs.child_mode_enabled = 0;
    if (prefs.child_channels_enabled > 1) prefs.child_channels_enabled = 0;
    prefs.child_visible_pages &= NodePrefs::HP_FAVOURITES |
                                 NodePrefs::HP_MAP | NodePrefs::HP_SENSORS |
                                 NodePrefs::HP_SHUTDOWN;
    if (prefs.quiet_time_enabled > 1) prefs.quiet_time_enabled = 0;
    if (prefs.quiet_time_start_min >= 24 * 60) prefs.quiet_time_start_min = 21 * 60;
    if (prefs.quiet_time_end_min >= 24 * 60) prefs.quiet_time_end_min = 7 * 60;
    if (prefs.bluetooth_enabled > 1) prefs.bluetooth_enabled = 1;
  }
};

} // namespace solo
