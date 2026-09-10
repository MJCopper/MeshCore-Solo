#pragma once

#include <stdint.h>
#include <string.h>

namespace solo {

// Owns on-device UI login state for rooms and remotely managed nodes. Packet
// construction and the BLE/USB login path remain in BaseChatMesh/MyMesh.
class RoomLoginCoordinator {
public:
  enum Owner : uint8_t { NONE, MESSAGES, ADMIN, SENSOR };

  struct Attempt {
    Owner owner;
    uint8_t pub_key[4];
    char password[16];
    bool used_saved_password;
  };

private:
  static const uint8_t SESSION_CAPACITY = 16;
  Attempt _attempt{};
  uint32_t _deadline_ms = 0;
  bool _active = false;
  uint8_t _sessions[SESSION_CAPACITY][4]{};
  uint8_t _session_count = 0;

public:
  bool active() const { return _active; }
  bool ownedBy(Owner owner) const { return _active && _attempt.owner == owner; }

  bool begin(Owner owner, const uint8_t* pub_key, const char* password,
             bool used_saved_password, uint32_t deadline_ms) {
    if (_active || owner == NONE || !pub_key || !password) return false;
    _attempt.owner = owner;
    memcpy(_attempt.pub_key, pub_key, sizeof(_attempt.pub_key));
    strncpy(_attempt.password, password, sizeof(_attempt.password) - 1);
    _attempt.password[sizeof(_attempt.password) - 1] = '\0';
    _attempt.used_saved_password = used_saved_password;
    _deadline_ms = deadline_ms;
    _active = true;
    return true;
  }

  bool complete(const uint8_t* pub_key, Attempt& completed) {
    if (!_active || !pub_key || memcmp(_attempt.pub_key, pub_key, 4) != 0) return false;
    completed = _attempt;
    memset(&_attempt, 0, sizeof(_attempt));
    _active = false;
    return true;
  }

  bool cancel(Owner owner, const uint8_t* pub_key, Attempt* cancelled = nullptr) {
    if (!_active || _attempt.owner != owner || !pub_key
        || memcmp(_attempt.pub_key, pub_key, 4) != 0) return false;
    if (cancelled) *cancelled = _attempt;
    memset(&_attempt, 0, sizeof(_attempt));
    _active = false;
    return true;
  }

  bool takeTimeout(uint32_t now, Attempt& timed_out) {
    if (!_active || (int32_t)(now - _deadline_ms) < 0) return false;
    timed_out = _attempt;
    memset(&_attempt, 0, sizeof(_attempt));
    _active = false;
    return true;
  }

  bool isLoggedIn(const uint8_t* pub_key) const {
    if (!pub_key) return false;
    for (uint8_t i = 0; i < _session_count; i++)
      if (memcmp(_sessions[i], pub_key, 4) == 0) return true;
    return false;
  }

  void markLoggedIn(const uint8_t* pub_key) {
    if (!pub_key || isLoggedIn(pub_key)) return;
    if (_session_count < SESSION_CAPACITY) {
      memcpy(_sessions[_session_count++], pub_key, 4);
    } else {
      memmove(_sessions, _sessions + 1, (SESSION_CAPACITY - 1) * 4);
      memcpy(_sessions[SESSION_CAPACITY - 1], pub_key, 4);
    }
  }

  void forgetLoggedIn(const uint8_t* pub_key) {
    if (!pub_key) return;
    for (uint8_t i = 0; i < _session_count; i++) {
      if (memcmp(_sessions[i], pub_key, 4) != 0) continue;
      if (i + 1 < _session_count)
        memmove(_sessions + i, _sessions + i + 1, (_session_count - i - 1) * 4);
      _session_count--;
      return;
    }
  }
};

} // namespace solo
