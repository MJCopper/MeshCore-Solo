#pragma once

namespace solo {

// Separates accepting a notification into device state from presenting it to
// the user. Quiet Time suppresses sound only; Child Mode may reject an event
// entirely when its sender/channel is outside the allow-list.
struct NotificationDecision {
  bool record_unread;
  bool show_visual;
  bool play_sound;
  bool vibrate;

  bool present() const { return show_visual || play_sound || vibrate; }
};

class NotificationPolicy {
public:
  static NotificationDecision decide(bool eligible, bool quiet_active,
                                     bool quiet_affected) {
    bool sound = eligible && !(quiet_active && quiet_affected);
    NotificationDecision result = { eligible, eligible, sound, eligible };
    return result;
  }
};

} // namespace solo
