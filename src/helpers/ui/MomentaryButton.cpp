#include "MomentaryButton.h"

#define MULTI_CLICK_WINDOW_MS  280

#ifdef BUTTON_USE_INTERRUPTS
#define ISR_DEBOUNCE_MS  5

MomentaryButton* MomentaryButton::_isr_table[MomentaryButton::MAX_ISR_BUTTONS] = { nullptr };

#define DEFINE_ISR_TRAMPOLINE(n) \
  void MomentaryButton::isrTrampoline##n() { if (_isr_table[n]) _isr_table[n]->isrHandler(); }
DEFINE_ISR_TRAMPOLINE(0)
DEFINE_ISR_TRAMPOLINE(1)
DEFINE_ISR_TRAMPOLINE(2)
DEFINE_ISR_TRAMPOLINE(3)
DEFINE_ISR_TRAMPOLINE(4)
DEFINE_ISR_TRAMPOLINE(5)
DEFINE_ISR_TRAMPOLINE(6)
DEFINE_ISR_TRAMPOLINE(7)
#undef DEFINE_ISR_TRAMPOLINE

// attachInterrupt() takes a plain void(*)() with no user-data slot on the
// Adafruit nRF52 and ESP32 Arduino cores, so each button needs its own
// trampoline to know which instance to dispatch to.
static void (*const ISR_TRAMPOLINES[MomentaryButton::MAX_ISR_BUTTONS])() = {
  MomentaryButton::isrTrampoline0, MomentaryButton::isrTrampoline1,
  MomentaryButton::isrTrampoline2, MomentaryButton::isrTrampoline3,
  MomentaryButton::isrTrampoline4, MomentaryButton::isrTrampoline5,
  MomentaryButton::isrTrampoline6, MomentaryButton::isrTrampoline7,
};

void MomentaryButton::pushEdge(uint8_t level, uint32_t at) {
  uint8_t next = (_edge_head + 1) % EDGE_BUF_SIZE;
  if (next == _edge_tail) return;  // full: drop rather than clobber older, unprocessed edges
  _edge_level[_edge_head] = level;
  _edge_time[_edge_head] = at;
  _edge_head = next;
}

bool MomentaryButton::popEdge(uint8_t &level, uint32_t &at) {
  if (_edge_tail == _edge_head) return false;
  level = _edge_level[_edge_tail];
  at = _edge_time[_edge_tail];
  _edge_tail = (_edge_tail + 1) % EDGE_BUF_SIZE;
  return true;
}

void MomentaryButton::isrHandler() {
  uint32_t now = millis();
  if (now - _last_isr_edge < ISR_DEBOUNCE_MS) return;  // mechanical contact-bounce guard
  _last_isr_edge = now;
  pushEdge(digitalRead(_pin), now);
}
#endif

MomentaryButton::MomentaryButton(int8_t pin, int long_press_millis, bool reverse, bool pulldownup, bool multiclick) { 
  _pin = pin;
  _reverse = reverse;
  _pull = pulldownup;
  down_at = 0; 
  prev = _reverse ? HIGH : LOW;
  cancel = 0;
  _long_millis = long_press_millis;
  _threshold = 0;
  _click_count = 0;
  _last_click_time = 0;
  _multi_click_window = multiclick ? MULTI_CLICK_WINDOW_MS : 0;
  _pending_click = false;
}

MomentaryButton::MomentaryButton(int8_t pin, int long_press_millis, int analog_threshold) {
  _pin = pin;
  _reverse = false;
  _pull = false;
  down_at = 0;
  prev = LOW;
  cancel = 0;
  _long_millis = long_press_millis;
  _threshold = analog_threshold;
  _click_count = 0;
  _last_click_time = 0;
  _multi_click_window = MULTI_CLICK_WINDOW_MS;
  _pending_click = false;
}

void MomentaryButton::begin() {
  if (_pin >= 0 && _threshold == 0) {
    pinMode(_pin, _pull ? (_reverse ? INPUT_PULLUP : INPUT_PULLDOWN) : INPUT);
#ifdef BUTTON_USE_INTERRUPTS
    for (uint8_t i = 0; i < MAX_ISR_BUTTONS; i++) {
      if (_isr_table[i] == nullptr) {
        _isr_table[i] = this;
        _isr_slot = i;
        attachInterrupt(digitalPinToInterrupt(_pin), ISR_TRAMPOLINES[i], CHANGE);
        break;
      }
    }
#endif
  }
}

bool  MomentaryButton::isPressed() const {
  int btn = _threshold > 0 ? (analogRead(_pin) < _threshold) : digitalRead(_pin);
  return isPressed(btn);
}

void MomentaryButton::cancelClick() {
  cancel = 1;
  down_at = 0;
  _click_count = 0;
  _last_click_time = 0;
  _pending_click = false;
}

bool MomentaryButton::isPressed(int level) const {
  if (_threshold > 0) {
    return level;
  }
  if (_reverse) {
    return level == LOW;
  } else {
    return level != LOW;
  }
}

void MomentaryButton::applyTransition(int btn, unsigned long at) {
  if (btn == prev) return;
  int event = BUTTON_EVENT_NONE;  // kept for parity with the original inline logic below
  if (isPressed(btn)) {
    down_at = at;
  } else {
    // button UP
    if (_long_millis > 0) {
      if (down_at > 0 && (unsigned long)(at - down_at) < (unsigned long)_long_millis) {  // only a CLICK if still within the long_press millis
          _click_count++;
          _last_click_time = at;
          _pending_click = true;
      }
    } else {
        _click_count++;
        _last_click_time = at;
        _pending_click = true;
    }
    if (event == BUTTON_EVENT_CLICK && cancel) {
      event = BUTTON_EVENT_NONE;
      _click_count = 0;
      _last_click_time = 0;
      _pending_click = false;
    }
    down_at = 0;
  }
  prev = btn;
}

int MomentaryButton::check(bool repeat_click) {
  if (_pin < 0) return BUTTON_EVENT_NONE;

  int event = BUTTON_EVENT_NONE;
  int btn;
#ifdef BUTTON_USE_INTERRUPTS
  if (_isr_slot >= 0) {
    // Replay edges the ISR caught while the main loop was off doing something
    // blocking (e-ink refresh) — each keeps its own capture timestamp so
    // click-duration / multi-click-window math still lines up correctly.
    uint8_t lvl; uint32_t at;
    while (popEdge(lvl, at)) applyTransition(lvl, at);
    btn = prev;  // edges already caught us up to the live level
  } else
#endif
  {
    btn = _threshold > 0 ? (analogRead(_pin) < _threshold) : digitalRead(_pin);
    applyTransition(btn, millis());
  }
  if (!isPressed(btn) && cancel) {   // always clear the pending 'cancel' once button is back in UP state
    cancel = 0;
  }

  if (_long_millis > 0 && down_at > 0 && (unsigned long)(millis() - down_at) >= _long_millis) {
    if (_pending_click) {
      // long press during multi-click detection - cancel pending clicks
      cancelClick();
    } else {
      event = BUTTON_EVENT_LONG_PRESS;
      down_at = 0;
      _click_count = 0;
      _last_click_time = 0;
      _pending_click = false;
    }
  }
  if (down_at > 0 && repeat_click) {
    unsigned long diff = (unsigned long)(millis() - down_at);
    if (diff >= 700) {
      event = BUTTON_EVENT_CLICK;   // wait 700 millis before repeating the click events
    }
  }

  if (_pending_click && (millis() - _last_click_time) >= _multi_click_window) {
    if (down_at > 0) {
      // still pressed - wait for button release before processing clicks
      return event;
    }
    switch (_click_count) {
      case 1:
        event = BUTTON_EVENT_CLICK;
        break;
      case 2:
        event = BUTTON_EVENT_DOUBLE_CLICK;
        break;
      case 3:
        event = BUTTON_EVENT_TRIPLE_CLICK;
        break;
      default:
        // For 4+ clicks, treat as triple click?
        event = BUTTON_EVENT_TRIPLE_CLICK;
        break;
    }
    _click_count = 0;
    _last_click_time = 0;
    _pending_click = false;
  }

  return event;
}