#pragma once

#include "SoloPolicy.h"

// Tiny, allocation-free owner for Solo session state. Persisted configuration
// stays in NodePrefs; transient parent authorization and lock transitions stay
// here instead of leaking feature-specific flags through the UI task.
namespace solo {

class Runtime {
  bool _parent_unlocked = true;
  bool _was_child_locked = false;

public:
  void begin(const NodePrefs* prefs) {
    _parent_unlocked = !prefs || !prefs->child_mode_enabled;
    _was_child_locked = false;
  }

  void setParentUnlocked(bool unlocked) { _parent_unlocked = unlocked; }
  bool parentUnlocked() const { return _parent_unlocked; }

  bool childLocked(const NodePrefs* prefs) const {
    return Policy::childLocked(prefs, _parent_unlocked);
  }

  bool recordChildLockState(const NodePrefs* prefs) {
    bool locked = childLocked(prefs);
    bool entered = locked && !_was_child_locked;
    _was_child_locked = locked;
    return entered;
  }
};

} // namespace solo
