#pragma once
// Tools › Repeater — consolidates the repeater toggle, its flood "politeness"
// filters, an optional dedicated radio profile, and live forwarding stats on one
// screen. A dedicated screen (vs. the old Settings › Radio sub-items) gives
// full-width rows, so the longer labels no longer collide with the value column,
// and keeps the relaying controls next to the numbers that show them working.
//
// Network modes:
//  - Current  — repeat on the companion's current frequency. Opt-in only:
//               relaying on whatever network the operator is chatting on
//               isn't the MeshCore community norm.
//  - Custom   — enabling the repeater switches the radio to a dedicated profile
//               (preset or manual), and disabling restores the companion params.
//               Default for a never-configured device, seeded with a frequency
//               in the same band as the companion's own network (so the default
//               can't land outside what's legal for the operator's region).
// Included by UITask.cpp.

#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include "icons.h"
#include "PopupMenu.h"
#include "DigitEditor.h"
#include "../RadioPresets.h"
#include "../MyMesh.h"

extern MyMesh the_mesh;

class RepeaterScreen : public UIScreen {
  UITask* _task;
  bool    _dirty;
  int     _sel;       // index into the interactive items currently shown
  int     _scroll;    // first visible row (render keeps _sel in view)

  enum Item {
    IT_REPEATER, IT_NETWORK,
    IT_RPRESET, IT_RFREQ, IT_RSF, IT_RBW, IT_RCR,   // dedicated profile (Custom network)
    IT_SKIP, IT_HOPS, IT_YIELD, IT_SNR, IT_SUPPRESS
  };
  uint8_t _items[12];
  int     _item_count;

  PopupMenu  _preset_menu;
  DigitEditor _freq_editor;
  uint8_t    _preset_user_slot[NodePrefs::USER_RADIO_PRESET_MAX];
  int        _preset_user_count = 0;

  // Nearest entry in LORA_BW_OPTS to the repeater profile's bw.
  int rptBwIndex(NodePrefs* p) const {
    int best = 0;
    float best_diff = 1e9f;
    for (int i = 0; i < LORA_BW_OPT_COUNT; i++) {
      float diff = fabsf(p->repeater_bw - LORA_BW_OPTS[i]);
      if (diff < best_diff) { best_diff = diff; best = i; }
    }
    return best;
  }

  void buildItems(NodePrefs* p) {
    _item_count = 0;
    _items[_item_count++] = IT_REPEATER;
    if (p && p->client_repeat) {
      _items[_item_count++] = IT_NETWORK;
      if (p->repeater_use_profile) {
        _items[_item_count++] = IT_RPRESET;
        _items[_item_count++] = IT_RFREQ;
        _items[_item_count++] = IT_RSF;
        _items[_item_count++] = IT_RBW;
        _items[_item_count++] = IT_RCR;
      }
      _items[_item_count++] = IT_SKIP;
      _items[_item_count++] = IT_HOPS;
      _items[_item_count++] = IT_YIELD;
      _items[_item_count++] = IT_SNR;
      _items[_item_count++] = IT_SUPPRESS;
    }
    if (_sel >= _item_count) _sel = _item_count - 1;
    if (_sel < 0) _sel = 0;
  }

  static const char* itemLabel(int item) {
    switch (item) {
      case IT_REPEATER: return "Repeater";
      case IT_NETWORK:  return "Network";
      case IT_RPRESET:  return "Rpt preset";
      case IT_RFREQ:    return "Rpt freq";
      case IT_RSF:      return "Rpt SF";
      case IT_RBW:      return "Rpt BW";
      case IT_RCR:      return "Rpt CR";
      case IT_SKIP:     return "Skip advert";
      case IT_HOPS:     return "Max hops";
      case IT_YIELD:    return "Yield";
      case IT_SNR:      return "Min SNR";
      case IT_SUPPRESS: return "Suppress dup";
    }
    return "";
  }

  // Name of the preset matching the repeater profile, or "Custom".
  const char* rptPresetName(NodePrefs* p) const {
    for (int i = 0; i < RADIO_PRESET_COUNT; i++) {
      const RadioPreset& r = RADIO_PRESETS[i];
      if (radioParamsMatchPreset(p->repeater_freq, p->repeater_bw, p->repeater_sf, p->repeater_cr, r.freq, r.bw, r.sf, r.cr))
        return r.name;
    }
    for (int i = 0; i < NodePrefs::USER_RADIO_PRESET_MAX; i++) {
      const NodePrefs::UserRadioPreset& u = p->user_radio_presets[i];
      if (!u.name[0]) continue;
      if (radioParamsMatchPreset(p->repeater_freq, p->repeater_bw, p->repeater_sf, p->repeater_cr, u.freq, u.bw, u.sf, u.cr))
        return u.name;
    }
    return "Custom";
  }

  void itemValue(int item, NodePrefs* p, char* buf, size_t n) const {
    if (!p) { strncpy(buf, "OFF", n); buf[n-1]=0; return; }
    switch (item) {
      case IT_REPEATER: strncpy(buf, p->client_repeat ? "ON" : "OFF", n); break;
      case IT_NETWORK:  strncpy(buf, p->repeater_use_profile ? "Custom" : "Current", n); break;
      case IT_RPRESET:  strncpy(buf, rptPresetName(p), n); break;
      case IT_RFREQ:    snprintf(buf, n, "%.3f", p->repeater_freq); break;
      case IT_RSF:      snprintf(buf, n, "%d", (int)p->repeater_sf); break;
      case IT_RBW:      snprintf(buf, n, "%.1f", p->repeater_bw); break;
      case IT_RCR:      snprintf(buf, n, "%d", (int)p->repeater_cr); break;
      case IT_SKIP:     strncpy(buf, p->repeat_skip_adverts ? "ON" : "OFF", n); break;
      case IT_HOPS:
        if (p->repeat_max_hops > 0) snprintf(buf, n, "%d", (int)p->repeat_max_hops);
        else strncpy(buf, "OFF", n);
        break;
      case IT_YIELD:
        if (p->repeat_delay_boost > 0) snprintf(buf, n, "x%d", (int)p->repeat_delay_boost + 1);
        else strncpy(buf, "OFF", n);
        break;
      case IT_SNR:
        if (p->repeat_min_snr != NodePrefs::REPEAT_SNR_DISABLED) snprintf(buf, n, "%ddB", (int)p->repeat_min_snr);
        else strncpy(buf, "OFF", n);
        break;
      case IT_SUPPRESS: strncpy(buf, p->repeat_suppress_dup ? "ON" : "OFF", n); break;
      default: strncpy(buf, "", n); break;
    }
    buf[n - 1] = '\0';
  }

  // Seed the profile from the companion's current params (used when first
  // switching to Custom, so the starting point is a valid, familiar config).
  void seedProfileFromCompanion(NodePrefs* p) {
    p->repeater_freq = p->freq; p->repeater_bw = p->bw;
    p->repeater_sf = p->sf;     p->repeater_cr = p->cr;
  }

  void openPresetMenu(NodePrefs* p) {
    _preset_menu.begin("Repeater Preset", 6);
    for (int i = 0; i < RADIO_PRESET_COUNT; i++) _preset_menu.addItem(RADIO_PRESETS[i].name);
    _preset_user_count = 0;
    for (int i = 0; i < NodePrefs::USER_RADIO_PRESET_MAX; i++) {
      if (!p->user_radio_presets[i].name[0]) continue;
      _preset_menu.addItem(p->user_radio_presets[i].name);
      _preset_user_slot[_preset_user_count++] = (uint8_t)i;
    }
  }

public:
  RepeaterScreen(UITask* task) : _task(task), _dirty(false), _sel(0), _scroll(0), _item_count(1) {}

  void enter() { _dirty = false; _sel = 0; _scroll = 0; _preset_menu.active = false; _freq_editor.active = false; }

  int render(DisplayDriver& display) override {
    NodePrefs* p = _task->getNodePrefs();
    buildItems(p);
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawCenteredHeader("REPEATER");

    const int item_h  = display.lineStep();
    const int start_y = display.listStart();
    int visible = display.listVisible(item_h);
    if (visible < 1) visible = 1;

    // Config only — live forwarding stats live on Tools › Diagnostics.
    int total = _item_count;
    if (_sel < _scroll)            _scroll = _sel;
    if (_sel >= _scroll + visible) _scroll = _sel - visible + 1;
    int max_scroll = total - visible;
    if (max_scroll < 0) max_scroll = 0;
    if (_scroll > max_scroll) _scroll = max_scroll;
    if (_scroll < 0) _scroll = 0;

    const int reserve = scrollIndicatorReserve(display, total, visible);
    for (int i = 0; i < visible && (_scroll + i) < total; i++) {
      int row = _scroll + i;
      int y = start_y + i * item_h;
      char val[16];
      bool sel = (row == _sel);
      int item = _items[row];
      display.drawSelectionRow(0, y - 1, display.width() - reserve, item_h - 1, sel);
      display.setCursor(2, y);
      display.print(itemLabel(item));
      if (item == IT_RFREQ && sel && _freq_editor.active) {
        _freq_editor.render(display, display.valCol(), y);
      } else {
        itemValue(item, p, val, sizeof(val));
        display.drawTextRightAlign(display.width() - reserve - 2, y, val);
      }
      display.setColor(DisplayDriver::LIGHT);
    }
    drawScrollIndicator(display, start_y, visible * item_h, total, visible, _scroll);
    display.setColor(DisplayDriver::LIGHT);
    if (_preset_menu.active) _preset_menu.render(display);
    return (_preset_menu.active || _freq_editor.active) ? 50 : 500;
  }

  bool handleInput(char c) override {
    NodePrefs* p = _task->getNodePrefs();

    // Modal overlays first.
    if (_preset_menu.active) {
      auto res = _preset_menu.handleInput(c);
      if (res == PopupMenu::SELECTED && p) {
        int idx = _preset_menu.selectedIndex();
        if (idx >= 0 && idx < RADIO_PRESET_COUNT) {
          const RadioPreset& r = RADIO_PRESETS[idx];
          p->repeater_freq = r.freq; p->repeater_bw = r.bw; p->repeater_sf = r.sf; p->repeater_cr = r.cr;
        } else {
          int u = idx - RADIO_PRESET_COUNT;
          if (u >= 0 && u < _preset_user_count) {
            const NodePrefs::UserRadioPreset& up = p->user_radio_presets[_preset_user_slot[u]];
            p->repeater_freq = up.freq; p->repeater_bw = up.bw; p->repeater_sf = up.sf; p->repeater_cr = up.cr;
          }
        }
        the_mesh.applyRepeaterRadio();   // live if currently relaying on the profile
        _dirty = true;
      }
      return true;
    }
    if (_freq_editor.active) {
      float before = _freq_editor.value;
      _freq_editor.handleInput(c);
      if (p && _freq_editor.value != before) {
        p->repeater_freq = _freq_editor.value;
        the_mesh.applyRepeaterRadio();
        _dirty = true;
      }
      return true;
    }

    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) {
      if (_dirty) the_mesh.savePrefs();
      _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_UP)   { _sel = (_sel > 0) ? _sel - 1 : _item_count - 1; return true; }
    if (c == KEY_DOWN) { _sel = (_sel < _item_count - 1) ? _sel + 1 : 0; return true; }
    if (!p) return false;

    bool right = keyIsNext(c);
    bool left  = keyIsPrev(c);
    bool enter = (c == KEY_ENTER);
    int item = _items[_sel];

    if (item == IT_REPEATER && (left || right || enter)) {
      p->client_repeat ^= 1;
      the_mesh.applyRepeaterRadio();   // switch to profile on enable / restore companion on disable
      _task->applyPowerSave();         // duty-cycle RX is forced off while repeating
      _task->applyApc();               // pin TX power to the ceiling while repeating (APC suppressed)
      buildItems(p);
      _dirty = true;
      return true;
    }
    if (item == IT_NETWORK && (left || right || enter)) {
      p->repeater_use_profile ^= 1;
      if (p->repeater_use_profile && !the_mesh.repeaterProfileValid())
        seedProfileFromCompanion(p);   // start Custom from a valid, familiar config
      the_mesh.applyRepeaterRadio();
      buildItems(p);
      _dirty = true;
      return true;
    }
    if (item == IT_RPRESET && enter) { openPresetMenu(p); return true; }
    if (item == IT_RFREQ && enter) {
      float lo, hi; radio_driver.getFreqBounds(lo, hi);
      _freq_editor.begin(p->repeater_freq, lo, hi, 3, 3);
      return true;
    }
    if (item == IT_RSF) {
      if (right && p->repeater_sf < 12) { p->repeater_sf++; the_mesh.applyRepeaterRadio(); _dirty = true; return true; }
      if (left  && p->repeater_sf > 5)  { p->repeater_sf--; the_mesh.applyRepeaterRadio(); _dirty = true; return true; }
    }
    if (item == IT_RBW) {
      int idx = rptBwIndex(p);
      if (right && idx < LORA_BW_OPT_COUNT - 1) { p->repeater_bw = LORA_BW_OPTS[idx + 1]; the_mesh.applyRepeaterRadio(); _dirty = true; return true; }
      if (left  && idx > 0)                     { p->repeater_bw = LORA_BW_OPTS[idx - 1]; the_mesh.applyRepeaterRadio(); _dirty = true; return true; }
    }
    if (item == IT_RCR) {
      if (right && p->repeater_cr < 8) { p->repeater_cr++; the_mesh.applyRepeaterRadio(); _dirty = true; return true; }
      if (left  && p->repeater_cr > 5) { p->repeater_cr--; the_mesh.applyRepeaterRadio(); _dirty = true; return true; }
    }
    if (item == IT_SKIP && (left || right || enter)) {
      p->repeat_skip_adverts ^= 1; _dirty = true; return true;
    }
    if (item == IT_HOPS) {
      if (right && p->repeat_max_hops < 8) { p->repeat_max_hops++; _dirty = true; return true; }
      if (left  && p->repeat_max_hops > 0) { p->repeat_max_hops--; _dirty = true; return true; }
    }
    if (item == IT_YIELD) {
      if (right && p->repeat_delay_boost < 8) { p->repeat_delay_boost++; _dirty = true; return true; }
      if (left  && p->repeat_delay_boost > 0) { p->repeat_delay_boost--; _dirty = true; return true; }
    }
    if (item == IT_SNR) {
      int8_t v = p->repeat_min_snr;
      if (right) {
        v = (v == NodePrefs::REPEAT_SNR_DISABLED) ? -20 : (v < 10 ? v + 1 : 10);
        p->repeat_min_snr = v; _dirty = true; return true;
      }
      if (left) {
        if (v != NodePrefs::REPEAT_SNR_DISABLED)
          p->repeat_min_snr = (v <= -20) ? NodePrefs::REPEAT_SNR_DISABLED : (int8_t)(v - 1);
        _dirty = true; return true;
      }
    }
    if (item == IT_SUPPRESS && (left || right || enter)) {
      p->repeat_suppress_dup ^= 1; _dirty = true; return true;
    }
    return false;
  }
};
