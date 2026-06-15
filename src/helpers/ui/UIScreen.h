#pragma once

#include "DisplayDriver.h"

#define KEY_LEFT           0xB4
#define KEY_UP             0xB5
#define KEY_DOWN           0xB6
#define KEY_RIGHT          0xB7
#define KEY_SELECT           10
#define KEY_ENTER            13
#define KEY_CANCEL           27   // Esc
#define KEY_HOME           0xF0
#define KEY_NEXT           0xF1
#define KEY_PREV           0xF2
#define KEY_CONTEXT_MENU   0xF3

// "Previous"/"Next" navigation keys: the directional key and its rotary-encoder
// twin. Decrement / increment a value, or step a selection. (Cancel and the
// context-menu key stay screen-specific — they mean different things per screen.)
static inline bool keyIsPrev(char c) { return c == KEY_LEFT  || c == KEY_PREV; }
static inline bool keyIsNext(char c) { return c == KEY_RIGHT || c == KEY_NEXT; }

#ifndef JOYSTICK_ROTATION
  #define JOYSTICK_ROTATION 0
#endif

// Rotate directional key by rot steps clockwise (0–3).
// Pass NodePrefs::joystick_rotation at runtime; JOYSTICK_ROTATION macro is the compile-time default.
static inline char rotateJoystickKey(char key, uint8_t rot) {
  switch (rot & 3) {
    case 1:
      if ((uint8_t)key == KEY_UP)    return KEY_RIGHT;
      if ((uint8_t)key == KEY_RIGHT) return KEY_DOWN;
      if ((uint8_t)key == KEY_DOWN)  return KEY_LEFT;
      if ((uint8_t)key == KEY_LEFT)  return KEY_UP;
      break;
    case 2:
      if ((uint8_t)key == KEY_UP)    return KEY_DOWN;
      if ((uint8_t)key == KEY_DOWN)  return KEY_UP;
      if ((uint8_t)key == KEY_LEFT)  return KEY_RIGHT;
      if ((uint8_t)key == KEY_RIGHT) return KEY_LEFT;
      break;
    case 3:
      if ((uint8_t)key == KEY_UP)    return KEY_LEFT;
      if ((uint8_t)key == KEY_LEFT)  return KEY_DOWN;
      if ((uint8_t)key == KEY_DOWN)  return KEY_RIGHT;
      if ((uint8_t)key == KEY_RIGHT) return KEY_UP;
      break;
  }
  return key;
}

class UIScreen {
protected:
  UIScreen() { }
public:
  virtual int render(DisplayDriver& display) =0;   // return value is number of millis until next render
  virtual bool handleInput(char c) { return false; }
  virtual void poll() { }
};

