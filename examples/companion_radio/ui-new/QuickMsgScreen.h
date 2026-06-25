#pragma once
// Custom screen — not part of upstream UITask.cpp
// Included by UITask.cpp after SettingsScreen.h is defined.

#include "NavView.h"   // navigate to a location shared inside a message
#include "icons.h"     // scalable mini-icons (delivery markers)

class QuickMsgScreen : public UIScreen {
  UITask* _task;

  enum Phase { MODE_SELECT, CONTACT_PICK, DM_HIST, MSG_PICK, CHANNEL_PICK, CHANNEL_HIST, KEYBOARD };
  Phase _phase;

  // MODE_SELECT
  int _mode_sel;  // 0=Direct, 1=Channel, 2=Room Servers

  // CONTACT_PICK
  int _contact_sel, _contact_scroll;
  int _num_contacts;
  uint16_t _sorted[MAX_CONTACTS];
  ContactInfo _sel_contact;
  bool _room_mode;  // true = picking a room server, false = picking a DM contact

  // CHANNEL_PICK
  int _channel_sel, _channel_scroll;
  int _num_channels;
  uint8_t _channel_indices[MAX_GROUP_CHANNELS];
  uint8_t _ch_unread[MAX_GROUP_CHANNELS];
  int _sel_channel_idx;
  bool _sending_to_channel;

  // Carries the just-sent DM's ACK tag + deadline + send timestamp from
  // sendText() to afterSend().
  uint32_t _last_ack_tag = 0;
  uint32_t _last_ack_deadline_ms = 0;
  uint32_t _last_send_ts = 0;

  // MSG_PICK (shared)
  int _msg_sel, _msg_scroll;
  int _active_msgs[QUICK_MSGS_MAX];
  int _active_msg_count;

  // CHANNEL_HIST
  int _hist_sel, _hist_scroll;
  FullscreenMsgView _fs;
  int  _unread_at_entry;    // _ch_unread value when entering CHANNEL_HIST
  int  _viewing_max_seen;  // highest _hist_sel reached in current session
  // Shared ring for all channels combined. addChannelMsg() decrements the
  // matching _ch_unread when an entry is evicted so the badge can't claim
  // unread messages that no longer exist in the ring. Each slot carries a full
  // MSG_TEXT_BUF, so this ring dominates the screen's RAM footprint — kept
  // modest to leave heap headroom (see DM_HIST_MAX).
  static const int CH_HIST_MAX = 48;

  // KEYBOARD
  KeyboardWidget* _kb;

  // Context menu (opened by KEY_CONTEXT_MENU in CHANNEL_PICK / CONTACT_PICK / histories)
  PopupMenu _ctx_menu;
  bool      _ctx_dirty;
  char      _ctx_notif_item[22];
  char      _ctx_melody_item[20];
  char      _ctx_pin_item[28];   // "Pin to dial" or "Unpin (slot N)"
  char      _ctx_ch_fav_item[12]; // "Fav" or "Unfav"
  char      _pin_slot_labels[NodePrefs::FAVOURITES_COUNT][22];  // per-slot picker labels
  bool      _pin_picker_active;  // true while the slot-picker submenu is open
  bool      _dm_direct_entry;    // entered DM_HIST via Favourites shortcut; CANCEL returns home
  char      _reply_prefix[36];   // "@[nick] " built when reply is triggered
  bool      _reply_mode;         // true while composing a reply (prefix is prepended)

  // Fullscreen-message context menu actions. Built per-message: Reply (when
  // applicable) plus Navigate / Save waypoint when the message carries a
  // location (a {loc} string or a [WAY] share). _fs_act maps each visible row
  // back to an action so the index math survives the conditional layout.
  enum FsAct : uint8_t { FS_REPLY, FS_NAV, FS_SAVE };
  uint8_t   _fs_act[3];
  int       _fs_act_n = 0;
  // Inline navigate-to-location view layered over the fullscreen message.
  bool      _nav_active = false;
  int32_t   _nav_lat = 0, _nav_lon = 0;
  char      _nav_label[24] = "";

  // Share-compose mode: launched from elsewhere (e.g. a waypoint) with a
  // prepared message; the user picks a recipient and the text lands prefilled
  // in the keyboard to confirm/edit before sending.
  bool      _share_mode = false;
  char      _share_text[160] = "";
  // Live-share target picker: reuse the recipient chooser to set the persistent
  // auto-share target (channel / DM) instead of composing a message.
  bool      _pick_target = false;

  // History text holds a full received message. Channel messages carry the
  // sender embedded as "Name: body" in the payload, so a message can be up to
  // the over-the-air maximum (MAX_TEXT_LEN). Size the buffers to that + NUL,
  // otherwise long messages (and Polish text, where each accented char is two
  // UTF-8 bytes) get their tail clipped.
  static const int MSG_TEXT_BUF = MAX_TEXT_LEN + 1;

  // Outgoing-message delivery state. DM: a real end-to-end ACK (✓ delivered to
  // the recipient). Channel: only a "relayed into mesh" echo from a repeater (no
  // recipient ACK exists for floods), so a missing echo is NOT shown as failure.
  enum AckState : uint8_t { ACK_NONE = 0, ACK_PENDING, ACK_OK, ACK_FAIL };

  struct ChHistEntry {
    uint8_t  ch_idx;
    char     text[MSG_TEXT_BUF];
    uint32_t timestamp;
    uint8_t  relay_status;   // AckState; only PENDING/OK used (no failure for floods)
    uint32_t relay_seq;      // MyMesh relay seq to match against onChannelRelayed()
  };
  ChHistEntry _hist[CH_HIST_MAX];
  int _hist_head, _hist_count;

  // DM_HIST
  struct DmHistEntry {
    uint8_t  prefix[4];
    uint8_t  outgoing;
    char     text[MSG_TEXT_BUF];
    uint32_t timestamp;
    uint8_t  ack_status;       // AckState; meaningful only when outgoing
    uint32_t ack_tag;          // expected_ack CRC to match against onMsgAck()
    uint32_t ack_deadline_ms;  // millis() by which a pending ACK must arrive
    // Sender-perspective message timestamp: the send timestamp for outgoing
    // (reused verbatim on resend so the recipient treats it as a retry), or the
    // sender_timestamp for incoming (used to dedup retried copies). 0 = unknown.
    uint32_t msg_ts;
    uint8_t  attempt;          // last attempt number sent (outgoing); next resend = attempt+1
    uint8_t  resends_left;     // remaining auto-resends before the marker shows ✗
  };
  // Each slot carries a full MSG_TEXT_BUF; kept modest to bound heap use.
  static const int DM_HIST_MAX = 32;
  DmHistEntry _dm_hist[DM_HIST_MAX];
  int _dm_hist_head, _dm_hist_count;
  int _dm_hist_sel, _dm_hist_scroll;
  FullscreenMsgView _dm_fs;

  int _hist_visible = 2;  // updated in render(); for history list scroll clamping

  void expandMsg(const char* tmpl, char* out, int out_len) const {
    double lat = 0, lon = 0;
    bool gps_valid = false;
#if ENV_INCLUDE_GPS == 1
    LocationProvider* loc = sensors.getLocationProvider();
    if (loc && loc->isValid()) {
      lat = loc->getLatitude() / 1000000.0;
      lon = loc->getLongitude() / 1000000.0;
      gps_valid = true;
    }
#endif
    NodePrefs* np = _task->getNodePrefs();
    float batt = (float)board.getBattMilliVolts() / 1000.0f;
    ::expandMsg(tmpl, out, out_len, lat, lon, gps_valid,
                rtc_clock.getCurrentTime(),
                np ? np->tz_offset_hours : 0,
                &sensors, batt);
  }

  // Scrollbar metrics for a history list. The track is pinned to the full list
  // area (top_y..cby) so it never resizes with content; only the thumb sizes and
  // moves. `need` also drives the gutter reserve so wide message boxes don't
  // reflow as messages arrive.
  struct HistScroll { bool need; int reserve; long total_px, scroll_px; int view_px; };

  // `getBody(idx)` returns the body text for list item idx (the part that wraps),
  // or nullptr to fall back to a fixed 2-line box.
  template <class GetBody>
  HistScroll computeHistScroll(DisplayDriver& display, bool portrait, int count, int scroll,
                               int hist_start_y, int cby, int lh, GetBody getBody) {
    HistScroll r{};
    const int fixed_bh = 2 * lh + 1;
    const int top_y    = hist_start_y + 1;
    r.view_px = cby - top_y;                  // fixed track height = full list area
    if (r.view_px < 1) r.view_px = 1;
    const int col = scrollIndicatorColWidth(display);

    if (!portrait) {                          // uniform 2-line boxes → exact pixel math
      const int box = fixed_bh + 1;
      r.total_px  = (long)count * box;
      r.scroll_px = (long)scroll * box;
      r.need      = r.total_px > r.view_px;
      r.reserve   = r.need ? col : 0;
      if (!r.need) r.scroll_px = 0;
      return r;
    }

    const int sp = 2;                         // portrait inter-box spacing
    auto boxH = [&](int idx, int rsv) -> int {
      const char* body = getBody(idx);
      if (!body) return fixed_bh;
      display.translateUTF8ToBlocks(s_wrap_trans, skipReplyPrefix(body), sizeof(s_wrap_trans));
      int nl = FullscreenMsgView::wrapLines(display, s_wrap_trans, display.width() - 6 - rsv, s_wrap_lines, 8);
      return (1 + (nl > 0 ? nl : 1)) * lh + 1;
    };
    // Scrollbar-needed test at the WIDEST layout (reserve 0 → fewest wrap lines →
    // shortest total). If even this overflows the list area the gutter is truly
    // needed, so it stays put and can't flicker in/out as messages arrive.
    int cur = hist_start_y, fit = 0;
    for (int i = 0; i < count; i++) { int bh = boxH(i, 0); if (cur + bh > cby) break; fit++; cur += bh + sp; }
    r.need    = fit < count;
    r.reserve = r.need ? col : 0;
    if (r.need) {                             // sum real box heights at the final width
      for (int i = 0; i < count; i++) {
        long ext = boxH(i, r.reserve) + sp;
        r.total_px += ext;
        if (i < scroll) r.scroll_px += ext;
      }
    }
    return r;
  }

  // Strip "@[nick] " reply prefix from a message body for compact list display.
  static const char* skipReplyPrefix(const char* text) {
    if (text[0] == '@' && text[1] == '[') {
      const char* close = strchr(text + 2, ']');
      if (close && close[1] == ' ' && close[2]) text = close + 2;
    }
    while (*text == '\n' || *text == '\r' || *text == ' ') text++;
    return text;
  }

  // Split a DM-history entry into the name to show as the author and the body to
  // show beneath it. Room servers carry many guests, so incoming room posts are
  // stored "Sender: text" (MyMesh::queueMessage); split that off so each line is
  // attributed to its guest. Outgoing → "Me"; plain DMs (or no separator) keep
  // the contact name and the text unchanged.
  const char* dmDisplayParts(const DmHistEntry& e, bool is_room, const char* contact_name,
                             char* sender_buf, int sender_cap) const {
    const char* body = skipReplyPrefix(e.text);
    if (e.outgoing) {
      strncpy(sender_buf, "Me", sender_cap - 1);
    } else if (is_room) {
      const char* sep = strstr(body, ": ");
      if (sep) {
        int nl = (int)(sep - body);
        if (nl > sender_cap - 1) nl = sender_cap - 1;
        strncpy(sender_buf, body, nl);
        sender_buf[nl] = '\0';
        return sep + 2;   // body after "Sender: "
      }
      strncpy(sender_buf, contact_name, sender_cap - 1);
    } else {
      strncpy(sender_buf, contact_name, sender_cap - 1);
    }
    sender_buf[sender_cap - 1] = '\0';
    return body;
  }

  // Build "@[nick] " prefix from a channel message text ("nick: body") into _reply_prefix.
  // Returns false if sender is "Me" (own message — no reply prefix needed).
  bool buildChannelReplyPrefix(const char* text) {
    const char* sep = strstr(text, ": ");
    if (!sep) return false;
    int slen = (int)(sep - text);
    if (slen == 2 && strncmp(text, "Me", 2) == 0) return false;
    if (slen > 31) slen = 31;
    snprintf(_reply_prefix, sizeof(_reply_prefix), "@[%.*s] ", slen, text);
    return true;
  }

  void startReply(bool to_channel) {
    _sending_to_channel = to_channel;
    _reply_mode = true;
    setupMsgPick();
    _phase = MSG_PICK;
  }

  // Recipient chosen while sharing — open the keyboard with the prepared text.
  void beginShareCompose(bool channel) {
    _sending_to_channel = channel;
    _reply_mode = false;
    _kb->begin(_share_text);
    _phase = KEYBOARD;
  }

  // Build the fullscreen-message options popup: Reply (if allowed) plus
  // Navigate / Save waypoint when `body` carries a location. Opens _ctx_menu
  // only when there's at least one action. Parses the location once here and
  // stashes it for the action handler.
  void buildFsMenu(const char* body, bool reply_allowed) {
    bool has_loc = geo::parseLatLon(body, _nav_lat, _nav_lon, _nav_label, sizeof(_nav_label));
    int n = (reply_allowed ? 1 : 0) + (has_loc ? 2 : 0);
    if (n == 0) return;
    _fs_act_n = 0;
    _ctx_menu.begin("Options", n);
    if (reply_allowed) { _ctx_menu.addItem("Reply");         _fs_act[_fs_act_n++] = FS_REPLY; }
    if (has_loc)       { _ctx_menu.addItem("Navigate");      _fs_act[_fs_act_n++] = FS_NAV;
                         _ctx_menu.addItem("Save waypoint"); _fs_act[_fs_act_n++] = FS_SAVE; }
  }

  // Dispatch the selected fullscreen-options row. `channel` picks which
  // fullscreen view to close when starting a reply.
  void dispatchFsAction(bool channel) {
    int csel = _ctx_menu.selectedIndex();
    FsAct a = (FsAct)_fs_act[(csel >= 0 && csel < _fs_act_n) ? csel : 0];
    _ctx_menu.active = false;
    if (a == FS_REPLY) {
      (channel ? _fs : _dm_fs).active = false;
      startReply(channel);
    } else if (a == FS_NAV) {
      _nav_active = true;            // keep the message view active underneath
    } else {
      saveSharedWaypoint();
    }
  }

  // Save the location parsed from the open message as a waypoint. Uses the
  // [WAY] label when present, else auto-names it like a manually-marked point.
  void saveSharedWaypoint() {
    char label[WAYPOINT_LABEL_LEN];
    if (_nav_label[0]) {
      strncpy(label, _nav_label, sizeof(label) - 1);
      label[sizeof(label) - 1] = '\0';
    } else {
      snprintf(label, sizeof(label), "WP%d", _task->waypoints().count() + 1);
    }
    _task->addWaypoint(_nav_lat, _nav_lon, label);
  }

  void renderNav(DisplayDriver& display) {
    int32_t mylat, mylon; bool have = _task->currentLocation(mylat, mylon);
    int cog; bool cogv = _task->currentCourse(cog);
    NodePrefs* p = _task->getNodePrefs();
    navview::draw(display, have, mylat, mylon, _nav_lat, _nav_lon,
                  _nav_label[0] ? _nav_label : "Msg loc", cogv, cog, p && p->units_imperial);
  }

  void setupMsgPick() {
    _msg_sel = _msg_scroll = 0;
    _active_msg_count = 0;
    NodePrefs* p = _task->getNodePrefs();
    if (p) {
      for (int i = 0; i < QUICK_MSGS_MAX; i++) {
        if (p->custom_msgs[i][0] != '\0')
          _active_msgs[_active_msg_count++] = i;
      }
    }
  }


  static void fmtMsgAge(char* buf, int n, uint32_t timestamp, uint32_t now) {
    if (timestamp == 0 || now < timestamp) { buf[0] = '\0'; return; }
    uint32_t age = now - timestamp;
    if      (age < 60)    snprintf(buf, n, "%us", age);
    else if (age < 3600)  snprintf(buf, n, "%um", age / 60);
    else if (age < 86400) snprintf(buf, n, "%uh", age / 3600);
    else                  snprintf(buf, n, ">1d");
  }

  // count history entries for a specific channel
  int histCountForChannel(int ch_idx) const {
    int n = 0;
    for (int i = 0; i < _hist_count; i++) {
      if (_hist[(_hist_head + i) % CH_HIST_MAX].ch_idx == (uint8_t)ch_idx) n++;
    }
    return n;
  }

  // get ring-buffer position of j-th history entry for channel (newest first)
  int histEntryForChannel(int ch_idx, int j) const {
    int n = 0;
    for (int i = _hist_count - 1; i >= 0; i--) {
      int pos = (_hist_head + i) % CH_HIST_MAX;
      if (_hist[pos].ch_idx == (uint8_t)ch_idx) {
        if (n == j) return pos;
        n++;
      }
    }
    return -1;
  }

  // Delivery marker, drawn with the current ink colour and auto-scaled to the
  // font (see icons.h). Pending = a row of dots, one per send (so it grows with
  // each auto-resend); delivered = ✓; failed = ✗; ACK_NONE = nothing.
  static void drawAckGlyph(DisplayDriver& d, int x, int top_y, AckState s, int sends = 1) {
    switch (s) {
      case ACK_PENDING: miniIconDotRow(d, x, top_y, sends);          break;
      case ACK_OK:      miniIconDraw(d, x, top_y, ICON_CHECK);       break;
      case ACK_FAIL:    miniIconDraw(d, x, top_y, ICON_CROSS);       break;
      default: break;                          // ACK_NONE → nothing
    }
  }

  // ack_tag != 0 marks an outgoing DM as awaiting an end-to-end ACK by
  // ack_deadline_ms; 0 means "sent, no confirmation possible" (no path / incoming).
  // msg_ts = sender-perspective timestamp (send ts for outgoing / sender_timestamp
  // for incoming); resends = remaining auto-resends for an outgoing pending DM.
  void storeDMMsg(const uint8_t* pub_key, bool outgoing, const char* text,
                  uint32_t ack_tag = 0, uint32_t ack_deadline_ms = 0,
                  uint32_t msg_ts = 0, uint8_t resends = 0) {
    int pos;
    if (_dm_hist_count < DM_HIST_MAX) {
      pos = (_dm_hist_head + _dm_hist_count) % DM_HIST_MAX;
      _dm_hist_count++;
    } else {
      pos = _dm_hist_head;
      _dm_hist_head = (_dm_hist_head + 1) % DM_HIST_MAX;
    }
    memcpy(_dm_hist[pos].prefix, pub_key, 4);
    _dm_hist[pos].outgoing = outgoing ? 1 : 0;
    _dm_hist[pos].timestamp = rtc_clock.getCurrentTime();
    strncpy(_dm_hist[pos].text, text, sizeof(DmHistEntry::text) - 1);
    _dm_hist[pos].text[sizeof(DmHistEntry::text) - 1] = '\0';
    _dm_hist[pos].ack_status      = (outgoing && ack_tag) ? ACK_PENDING : ACK_NONE;
    _dm_hist[pos].ack_tag         = ack_tag;
    _dm_hist[pos].ack_deadline_ms = ack_deadline_ms;
    _dm_hist[pos].msg_ts          = msg_ts;
    _dm_hist[pos].attempt         = 0;
    _dm_hist[pos].resends_left    = (outgoing && ack_tag) ? resends : 0;
  }

  // Effective status for display. A pending ACK only reads as failed once its
  // deadline has passed AND no auto-resends remain — while resends_left > 0 the
  // entry stays pending (tickDmResends() retries / finalises it). Safety net for
  // when the tick hasn't run yet; the tick is the authority that writes ACK_FAIL.
  AckState dmEffectiveStatus(const DmHistEntry& e) const {
    if (e.ack_status == ACK_PENDING && e.resends_left == 0 &&
        (int32_t)(millis() - e.ack_deadline_ms) >= 0)
      return ACK_FAIL;
    return (AckState)e.ack_status;
  }

  int dmHistCountForContact(const uint8_t* prefix) const {
    int n = 0;
    for (int i = 0; i < _dm_hist_count; i++)
      if (memcmp(_dm_hist[(_dm_hist_head + i) % DM_HIST_MAX].prefix, prefix, 4) == 0) n++;
    return n;
  }

  int dmHistEntryForContact(const uint8_t* prefix, int j) const { // j=0 = newest
    int n = 0;
    for (int i = _dm_hist_count - 1; i >= 0; i--) {
      int pos = (_dm_hist_head + i) % DM_HIST_MAX;
      if (memcmp(_dm_hist[pos].prefix, prefix, 4) == 0) {
        if (n == j) return pos;
        n++;
      }
    }
    return -1;
  }

  void afterSend(bool ok, const char* msg) {
    _reply_mode = false;
    _share_mode = false;
    if (ok && _sending_to_channel) {
      _hist_sel = 0;
      _hist_scroll = 0;
      _phase = CHANNEL_HIST;  // set before addChannelMsg so viewing=true, no unread bump
      char entry[sizeof(ChHistEntry::text)];
      snprintf(entry, sizeof(entry), "Me: %s", msg);
      int pos = addChannelMsg(_sel_channel_idx, entry);
      // Arm the "relayed into mesh" marker on this exact entry — MyMesh tracked
      // the flood it just originated and reports a heard repeater echo by seq.
      if (pos >= 0) {
        _hist[pos].relay_status = ACK_PENDING;
        _hist[pos].relay_seq    = the_mesh.lastChannelRelaySeq();
      }
      // After inserting sent msg at index 0, the unread index range is stale.
      // User is active in this channel — treat as fully read.
      _ch_unread[_sel_channel_idx] = 0;
      _unread_at_entry = 0;
      _viewing_max_seen = 0;
      _task->showAlert("Sent!", 600);
    } else if (ok) {
      NodePrefs* np = _task->getNodePrefs();
      uint8_t resends = np ? np->dm_resend_count : 0;
      storeDMMsg(_sel_contact.id.pub_key, true, msg, _last_ack_tag,
                 _last_ack_deadline_ms, _last_send_ts, resends);
      _dm_hist_sel = 0;
      _dm_hist_scroll = 0;
      _phase = DM_HIST;
      _task->showAlert("Sent!", 600);
    } else {
      _task->showAlert("Send failed", 1500);
      _task->gotoHomeScreen();
    }
  }

  bool sendText(const char* msg) {
    _last_ack_tag = 0;
    _last_ack_deadline_ms = 0;
    _last_send_ts = 0;
    if (_sending_to_channel) {
      ChannelDetails ch;
      if (!the_mesh.getChannel(_sel_channel_idx, ch)) return false;
      return the_mesh.sendGroupMessage(rtc_clock.getCurrentTime(), ch.channel,
                                       the_mesh.getNodeName(), msg, strlen(msg));
    } else {
      uint32_t send_ts = rtc_clock.getCurrentTime();
      uint32_t expected_ack = 0, est_timeout = 0;
      bool ok = the_mesh.sendMessage(_sel_contact, send_ts, 0,
                                     msg, expected_ack, est_timeout) > 0;
      if (ok && expected_ack) {
        _last_send_ts = send_ts;
        _last_ack_tag = expected_ack;
        // Generous margin over the base estimate so a slow multi-hop ACK isn't
        // prematurely shown as failed.
        _last_ack_deadline_ms = millis() + est_timeout + 4000;
      }
      return ok;
    }
  }

  void buildContactList() {
    NodePrefs* p = _task->getNodePrefs();
    ContactInfo c;
    int total = the_mesh.getNumContacts();
    _num_contacts = 0;
    if (_room_mode) {
      bool fav_only = (p && p->room_fav_only);
      for (int i = 0; i < total; i++) {
        if (!the_mesh.getContactByIdx(i, c) || c.type != ADV_TYPE_ROOM) continue;
        if (fav_only && !(c.flags & 0x01)) continue;
        _sorted[_num_contacts++] = i;
      }
    } else {
      bool show_all = (p && p->dm_show_all);
      // Build _sorted and counts[] in one pass — avoids a second getContactByIdx loop.
      int counts[MAX_CONTACTS];
      for (int i = 0; i < total; i++) {
        if (!the_mesh.getContactByIdx(i, c) || c.type != ADV_TYPE_CHAT) continue;
        if (!show_all && !(c.flags & 0x01)) continue;
        counts[_num_contacts] = dmHistCountForContact(c.id.pub_key);
        _sorted[_num_contacts++] = i;
      }
      // Sort by message count descending; contacts with no messages keep original order.
      for (int i = 1; i < _num_contacts; i++) {
        if (counts[i] == 0) continue;
        uint16_t key = _sorted[i]; int kc = counts[i];
        int j = i;
        while (j > 0 && counts[j-1] < kc) {
          _sorted[j] = _sorted[j-1]; counts[j] = counts[j-1]; j--;
        }
        _sorted[j] = key; counts[j] = kc;
      }
    }
  }

  void buildChannelList() {
    NodePrefs* p = _task->getNodePrefs();
    bool fav_only = (p && p->ch_fav_only);
    _num_channels = 0;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails ch;
      if (!the_mesh.getChannel(i, ch) || ch.name[0] == '\0') continue;
      if (fav_only && !(p->ch_fav_bitmask & (1ULL << i))) continue;
      _channel_indices[_num_channels++] = (uint8_t)i;
    }
  }

  // Returns per-channel notification state: 0=follow global, 1=muted, 2=force-on
  uint8_t chNotifState(uint8_t ch_idx) const {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return 0;
    uint64_t mask = 1ULL << ch_idx;
    if (!(p->ch_notif_override & mask)) return 0;
    return (p->ch_notif_muted & mask) ? 1 : 2;
  }

  void setChNotifState(uint8_t ch_idx, uint8_t state) {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return;
    uint64_t mask = 1ULL << ch_idx;
    if (state == 0) {
      p->ch_notif_override &= ~mask;
      p->ch_notif_muted    &= ~mask;
    } else if (state == 1) {
      p->ch_notif_override |= mask;
      p->ch_notif_muted    |= mask;
    } else {
      p->ch_notif_override |= mask;
      p->ch_notif_muted    &= ~mask;
    }
  }

  uint8_t dmNotifState(const uint8_t* pub_key) const {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return 0;
    for (int i = 0; i < NodePrefs::DM_NOTIF_TABLE_MAX; i++)
      if (p->dm_notif[i].state && memcmp(p->dm_notif[i].prefix, pub_key, 4) == 0)
        return p->dm_notif[i].state;
    return 0;
  }

  void setDmNotifState(const uint8_t* pub_key, uint8_t state) {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return;
    for (int i = 0; i < NodePrefs::DM_NOTIF_TABLE_MAX; i++) {
      if (p->dm_notif[i].state && memcmp(p->dm_notif[i].prefix, pub_key, 4) == 0) {
        if (state == 0) { memset(&p->dm_notif[i], 0, sizeof(p->dm_notif[i])); }
        else              p->dm_notif[i].state = state;
        return;
      }
    }
    if (state == 0) return;
    for (int i = 0; i < NodePrefs::DM_NOTIF_TABLE_MAX; i++) {
      if (p->dm_notif[i].state == 0) {
        memcpy(p->dm_notif[i].prefix, pub_key, 4);
        p->dm_notif[i].state = state;
        return;
      }
    }
    // table full — overwrite slot 0
    memcpy(p->dm_notif[0].prefix, pub_key, 4);
    p->dm_notif[0].state = state;
  }

  uint8_t chNotifMelody(uint8_t ch_idx) const {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return 0;
    uint64_t mask = 1ULL << ch_idx;
    if (!(p->ch_notif_melody_set & mask)) return 0;
    return (p->ch_notif_melody_2 & mask) ? 2 : 1;
  }

  void setChNotifMelody(uint8_t ch_idx, uint8_t slot) {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return;
    uint64_t mask = 1ULL << ch_idx;
    if (slot == 0) {
      p->ch_notif_melody_set &= ~mask;
      p->ch_notif_melody_2   &= ~mask;
    } else if (slot == 1) {
      p->ch_notif_melody_set |= mask;
      p->ch_notif_melody_2   &= ~mask;
    } else {
      p->ch_notif_melody_set |= mask;
      p->ch_notif_melody_2   |= mask;
    }
  }

  uint8_t dmMelodySlot(const uint8_t* pub_key) const {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return 0;
    for (int i = 0; i < NodePrefs::DM_MELODY_TABLE_MAX; i++)
      if (p->dm_melody[i].slot && memcmp(p->dm_melody[i].prefix, pub_key, 4) == 0)
        return p->dm_melody[i].slot;
    return 0;
  }

  void setDmMelody(const uint8_t* pub_key, uint8_t slot) {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return;
    for (int i = 0; i < NodePrefs::DM_MELODY_TABLE_MAX; i++) {
      if (p->dm_melody[i].slot && memcmp(p->dm_melody[i].prefix, pub_key, 4) == 0) {
        if (slot == 0) memset(&p->dm_melody[i], 0, sizeof(p->dm_melody[i]));
        else           p->dm_melody[i].slot = slot;
        return;
      }
    }
    if (slot == 0) return;
    for (int i = 0; i < NodePrefs::DM_MELODY_TABLE_MAX; i++) {
      if (p->dm_melody[i].slot == 0) {
        memcpy(p->dm_melody[i].prefix, pub_key, 4);
        p->dm_melody[i].slot = slot;
        return;
      }
    }
    memcpy(p->dm_melody[0].prefix, pub_key, 4);
    p->dm_melody[0].slot = slot;
  }

public:
  QuickMsgScreen(UITask* task, KeyboardWidget* kb)
    : _task(task), _kb(kb), _phase(MODE_SELECT), _mode_sel(0),
      _contact_sel(0), _contact_scroll(0), _num_contacts(0), _room_mode(false),
      _channel_sel(0), _channel_scroll(0), _num_channels(0),
      _sel_channel_idx(0), _sending_to_channel(false),
      _msg_sel(0), _msg_scroll(0), _active_msg_count(0),
      _hist_sel(0), _hist_scroll(0),
      _unread_at_entry(0), _viewing_max_seen(0),
      _hist_head(0), _hist_count(0),
      _dm_hist_head(0), _dm_hist_count(0),
      _dm_hist_sel(-1), _dm_hist_scroll(0),
      _ctx_dirty(false), _pin_picker_active(false), _dm_direct_entry(false), _reply_mode(false) {
    memset(_ch_unread, 0, sizeof(_ch_unread));
  }

  // Returns the ring position the message was stored at, or -1 if rejected, so
  // the outgoing path can attach a relay seq to that exact entry.
  int addChannelMsg(uint8_t ch_idx, const char* text) {
    // Guard against bogus channel indices (e.g. findChannelIdx() returned -1
    // and was cast to uint8_t → 255). Storing such an entry would burn a ring
    // slot for a message that no visible channel can ever surface.
    if (ch_idx >= MAX_GROUP_CHANNELS) return -1;
    int pos;
    if (_hist_count < CH_HIST_MAX) {
      pos = (_hist_head + _hist_count) % CH_HIST_MAX;
      _hist_count++;
    } else {
      pos = _hist_head;
      // Evicting the oldest entry — drop its share of the unread counter so
      // the badge can't claim a message the ring no longer holds.
      uint8_t evicted = _hist[pos].ch_idx;
      if (evicted < MAX_GROUP_CHANNELS && _ch_unread[evicted] > 0) {
        _ch_unread[evicted]--;
      }
      _hist_head = (_hist_head + 1) % CH_HIST_MAX;
    }
    _hist[pos].ch_idx = ch_idx;
    _hist[pos].timestamp = rtc_clock.getCurrentTime();
    strncpy(_hist[pos].text, text, sizeof(_hist[pos].text) - 1);
    _hist[pos].text[sizeof(_hist[pos].text) - 1] = '\0';
    _hist[pos].relay_status = ACK_NONE;
    _hist[pos].relay_seq = 0;

    bool viewing = (_phase == CHANNEL_HIST && _sel_channel_idx == (int)ch_idx);
    if (!viewing && _ch_unread[ch_idx] < 99) _ch_unread[ch_idx]++;
    return pos;
  }

  // Called when a repeater echo of one of our channel sends is heard.
  void markChannelRelayed(uint32_t seq) {
    if (seq == 0) return;
    for (int i = 0; i < _hist_count; i++) {
      ChHistEntry& e = _hist[(_hist_head + i) % CH_HIST_MAX];
      if (e.relay_status == ACK_PENDING && e.relay_seq == seq) {
        e.relay_status = ACK_OK;
        return;
      }
    }
  }

  void addDMMsg(const uint8_t* pub_key, bool outgoing, const char* text,
                uint32_t sender_timestamp = 0) {
    // Drop retried copies of an incoming DM: a resend reuses the sender's
    // timestamp and text but carries a fresh packet hash, so the mesh dup-filter
    // lets it through. Match on prefix + sender_timestamp + text to suppress it.
    if (!outgoing && sender_timestamp != 0) {
      for (int i = 0; i < _dm_hist_count; i++) {
        const DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
        if (!e.outgoing && e.msg_ts == sender_timestamp &&
            memcmp(e.prefix, pub_key, 4) == 0 && strcmp(e.text, text) == 0)
          return;  // duplicate retry — already in history
      }
    }
    storeDMMsg(pub_key, outgoing, text, 0, 0, outgoing ? 0 : sender_timestamp, 0);
  }

  // Called when an end-to-end ACK arrives (routed from MyMesh::onAckRecv).
  // Marks the matching pending outgoing DM as delivered.
  void markDmDelivered(uint32_t ack_crc) {
    if (ack_crc == 0) return;
    for (int i = 0; i < _dm_hist_count; i++) {
      DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
      if (e.outgoing && e.ack_status == ACK_PENDING && e.ack_tag == ack_crc) {
        e.ack_status = ACK_OK;
        return;
      }
    }
  }

  // Look up a contact by 4-byte pub_key prefix (as stored in DmHistEntry).
  bool contactByPrefix(const uint8_t* prefix, ContactInfo& out) const {
    int total = the_mesh.getNumContacts();
    for (int i = 0; i < total; i++) {
      ContactInfo c;
      if (the_mesh.getContactByIdx(i, c) && memcmp(c.id.pub_key, prefix, 4) == 0) {
        out = c;
        return true;
      }
    }
    return false;
  }

  // Background tick (called every UI loop, regardless of the active screen) that
  // drives auto-resend of on-device DMs. When a pending DM passes its deadline
  // with no ACK: resend with the next attempt# (reusing the original timestamp so
  // the recipient dedups) while resends remain, else mark it failed (✗).
  void tickDmResends() {
    uint32_t now = millis();
    for (int i = 0; i < _dm_hist_count; i++) {
      DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
      if (!e.outgoing || e.ack_status != ACK_PENDING) continue;
      if ((int32_t)(now - e.ack_deadline_ms) < 0) continue;   // still waiting
      if (e.resends_left == 0) { e.ack_status = ACK_FAIL; continue; }
      ContactInfo c;
      if (!contactByPrefix(e.prefix, c)) { e.ack_status = ACK_FAIL; continue; }
      uint32_t expected_ack = 0, est_timeout = 0;
      uint8_t next_attempt = e.attempt + 1;
      if (the_mesh.sendMessage(c, e.msg_ts, next_attempt, e.text,
                               expected_ack, est_timeout) > 0 && expected_ack) {
        e.attempt         = next_attempt;
        e.ack_tag         = expected_ack;   // each attempt has a distinct ACK CRC
        e.ack_deadline_ms = now + est_timeout + 4000;
        e.resends_left--;
      } else {
        e.ack_status = ACK_FAIL;            // couldn't compose/send — give up
      }
    }
  }

  int getDMUnreadTotal() const {
    return _task->getDMUnreadTotal();
  }

  int getTotalChannelUnread() const {
    int total = 0;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) total += _ch_unread[i];
    return total;
  }

  void markReadAlert(int n) {
    char msg[32];
    snprintf(msg, sizeof(msg), "%d marked read", n);
    _task->showAlert(msg, 800);
  }

  void clearAllChannelUnread() {
    memset(_ch_unread, 0, sizeof(_ch_unread));
  }

  void updateChannelUnread() {
    if (_sel_channel_idx < 0 || _sel_channel_idx >= MAX_GROUP_CHANNELS) return;
    // histEntryForChannel is newest-first: index 0 = newest (unread), higher = older.
    // Count everything actually rendered on screen as seen — not just the
    // highlighted row — so a taller screen that fits more boxes at once marks
    // more read up front, instead of requiring a press per row.
    int seen_to = _hist_scroll + _hist_visible - 1;
    if (_hist_sel > seen_to) seen_to = _hist_sel;
    if (seen_to > _viewing_max_seen) _viewing_max_seen = seen_to;
    // Each step down from 0 sees one more message; seen count = max_seen + 1.
    int remaining = _unread_at_entry - (_viewing_max_seen + 1);
    _ch_unread[_sel_channel_idx] = (uint8_t)(remaining > 0 ? remaining : 0);
  }

  void reset() {
    _phase = MODE_SELECT;
    _mode_sel = 0;
    _contact_sel = _contact_scroll = 0;
    _msg_sel = _msg_scroll = 0;
    _channel_sel = _channel_scroll = 0;
    _sending_to_channel = false;

    _room_mode = false;
    buildContactList();
    buildChannelList();

    _ctx_menu.active = false;
    _ctx_dirty = false;
    _nav_active = false;
    _share_mode = false;
    _pick_target = false;
    _pin_picker_active = false;
    _dm_direct_entry = false;
    _unread_at_entry = 0;
    _viewing_max_seen = 0;
  }

  // Recent DM contacts, newest first, deduped. Resolves the 4-byte
  // _dm_hist prefix to a 6-byte pub_key prefix by walking the contact
  // list once per unique sender. Returns the number filled.
  int getRecentDMContacts(uint8_t out[][NodePrefs::FAVOURITE_PREFIX_LEN], int max) const {
    int n = 0;
    for (int i = _dm_hist_count - 1; i >= 0 && n < max; i--) {
      int pos = (_dm_hist_head + i) % DM_HIST_MAX;
      const uint8_t* p4 = _dm_hist[pos].prefix;
      // Skip if already collected.
      bool dup = false;
      for (int j = 0; j < n; j++) if (memcmp(out[j], p4, 4) == 0) { dup = true; break; }
      if (dup) continue;
      // Find a real contact whose pub_key starts with this 4-byte prefix.
      for (int idx = 0; ; idx++) {
        ContactInfo c;
        if (!the_mesh.getContactByIdx(idx, c)) break;
        if (memcmp(c.id.pub_key, p4, 4) == 0) {
          memcpy(out[n], c.id.pub_key, NodePrefs::FAVOURITE_PREFIX_LEN);
          n++;
          break;
        }
      }
    }
    return n;
  }

  // Jump straight into a contact's DM history (used by the Favourites dial).
  // Caller must have already reset() the screen. Marks the entry so KEY_CANCEL
  // from DM_HIST returns to the home screen instead of falling back through
  // CONTACT_PICK → MODE_SELECT.
  // Enter the screen pre-loaded to share `text` (e.g. a "[WAY]lat,lon label"
  // waypoint). The user picks Direct/Channel then a recipient; selecting one
  // jumps straight to the keyboard prefilled with the text (see beginShareCompose).
  void startShare(const char* text) {
    reset();
    _share_mode = true;
    strncpy(_share_text, text, sizeof(_share_text) - 1);
    _share_text[sizeof(_share_text) - 1] = '\0';
    _phase = MODE_SELECT;
  }

  // Open the recipient chooser to set the live-share target (channel/DM). On
  // selection the target is stored in NodePrefs and the Map screen is restored.
  void startPickTarget() {
    reset();
    _pick_target = true;
    _phase = MODE_SELECT;
  }

  void commitPickTargetChannel(int ch_idx) {
    NodePrefs* p = _task->getNodePrefs();
    if (p) {
      p->loc_share_target_type = 0;
      p->loc_share_channel_idx = (uint8_t)ch_idx;
      the_mesh.savePrefs();
    }
    _pick_target = false;
    _task->showAlert("Share target set", 1200);
    _task->gotoLiveShareScreen();
  }

  void commitPickTargetDM(const ContactInfo& ci) {
    // A room server can't be a live-share target — a [LOC] DM to it is never
    // reposted to the room's members. Reject the pick and keep the chooser open.
    if (ci.type == ADV_TYPE_ROOM) {
      _task->showAlert("Rooms not supported", 1400);
      return;
    }
    NodePrefs* p = _task->getNodePrefs();
    if (p) {
      p->loc_share_target_type = 1;
      memcpy(p->loc_share_dm_prefix, ci.id.pub_key, NodePrefs::FAVOURITE_PREFIX_LEN);
      the_mesh.savePrefs();
    }
    _pick_target = false;
    _task->showAlert("Share target set", 1200);
    _task->gotoLiveShareScreen();
  }

  void enterDM(const ContactInfo& ci) {
    _sel_contact = ci;
    _task->clearDMUnread(ci.id.pub_key);
    _dm_hist_sel = -1;
    _dm_hist_scroll = 0;
    _dm_fs.active = false;
    _room_mode = false;
    _phase = DM_HIST;
    _dm_direct_entry = true;
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    // Navigate-to-location view sits over everything else while active.
    if (_nav_active) { renderNav(display); return 1000; }

    int lh      = display.getLineHeight();
    int item_h  = display.lineStep();
    int start_y = display.listStart();
    int cw      = display.getCharWidth();

    if (_phase == MODE_SELECT) {
      display.drawCenteredHeader("MESSAGE");
      const char* opts[] = { "Direct message", "Channels", "Room Servers" };
      int badges[3] = {
        getDMUnreadTotal(),
        _task->getChannelUnreadCount(),
        _task->getRoomUnreadCount()
      };
      for (int i = 0; i < 3; i++) {
        int y = start_y + i * item_h;
        bool sel = (i == _mode_sel);
        display.drawSelectionRow(0, y - 1, display.width(), item_h - 1, sel);
        display.setCursor(2, y);
        display.print(opts[i]);
        if (badges[i] > 0) {
          char badge[5];
          snprintf(badge, sizeof(badge), "%d", badges[i]);
          int bw = display.getTextWidth(badge) + 2;
          display.setCursor(display.width() - bw, y);
          display.print(badge);
        }
      }
      display.setColor(DisplayDriver::LIGHT);
      if (_ctx_menu.active) _ctx_menu.render(display);

    } else if (_phase == CONTACT_PICK) {
      display.drawCenteredHeader(_room_mode ? "SELECT ROOM" : "SELECT CONTACT");

      if (_num_contacts == 0) {
        display.drawTextCentered(display.width()/2, display.height()/2, _room_mode ? "No room servers" : "No favourites");
        return 5000;
      }

      drawList(display, _num_contacts, _contact_sel, _contact_scroll, [&](int list_idx, int y, bool sel, int reserve) {
        int mesh_idx = _sorted[list_idx];

        display.drawSelectionRow(0, y - 1, display.width() - reserve, item_h - 1, sel);

        ContactInfo c;
        if (the_mesh.getContactByIdx(mesh_idx, c)) {
          char filtered[sizeof(c.name)];
          display.translateUTF8ToBlocks(filtered, c.name, sizeof(filtered));
          uint8_t dm_unread = _task->getDMUnread(c.id.pub_key);
          char badge[5] = "";
          int bw = 0;
          if (dm_unread > 0) {
            snprintf(badge, sizeof(badge), "%d", (int)dm_unread);
            bw = display.getTextWidth(badge) + 2;
          }
          display.drawTextEllipsized(2, y, display.width() - 2 - bw - 1 - reserve, filtered);
          if (dm_unread > 0) {
            display.setCursor(display.width() - bw + 1 - reserve, y);
            display.print(badge);
          }
        }
      });

      // Context menu overlay
      if (_ctx_menu.active) _ctx_menu.render(display);

    } else if (_phase == CHANNEL_PICK) {
      display.drawCenteredHeader("SELECT CHANNEL");

      if (_num_channels == 0) {
        display.drawTextCentered(display.width()/2, display.height()/2, "No channels");
        return 5000;
      }

      drawList(display, _num_channels, _channel_sel, _channel_scroll, [&](int list_idx, int y, bool sel, int reserve) {
        display.drawSelectionRow(0, y - 1, display.width() - reserve, item_h - 1, sel);
        ChannelDetails ch;
        if (the_mesh.getChannel(_channel_indices[list_idx], ch)) {
          uint8_t unread = _ch_unread[_channel_indices[list_idx]];
          char badge[5] = "";
          int bw = 0;
          if (unread > 0) {
            snprintf(badge, sizeof(badge), "%d", (int)unread);
            bw = display.getTextWidth(badge) + 2;
          }
          display.drawTextEllipsized(2, y, display.width() - 4 - bw - reserve, ch.name);
          if (unread > 0) {
            display.setCursor(display.width() - bw - reserve, y);
            display.print(badge);
          }
        }
      });

      // Context menu overlay
      if (_ctx_menu.active) _ctx_menu.render(display);

    } else if (_phase == DM_HIST) {
      display.setTextSize(1);
      char filtered_name[sizeof(_sel_contact.name)];
      display.translateUTF8ToBlocks(filtered_name, _sel_contact.name, sizeof(filtered_name));

      if (_dm_fs.active && _dm_hist_sel >= 0) {
        int ring_pos = dmHistEntryForContact(_sel_contact.id.pub_key, _dm_hist_sel);
        if (ring_pos >= 0) {
          const DmHistEntry& e = _dm_hist[ring_pos];
          char sender_buf[33];
          const char* body = dmDisplayParts(e, _sel_contact.type == ADV_TYPE_ROOM,
                                            filtered_name, sender_buf, sizeof(sender_buf));
          const char* sender = sender_buf;
          int dm_count = dmHistCountForContact(_sel_contact.id.pub_key);
          int ret = _dm_fs.render(display, sender, body,
                                  _dm_hist_sel < dm_count - 1,
                                  _dm_hist_sel > 0);
          if (e.outgoing) {  // delivery marker in the (inverted) header bar
            display.setColor(DisplayDriver::DARK);
            drawAckGlyph(display, 2 + display.getTextWidth(sender) + 3, 1,
                         dmEffectiveStatus(e), e.attempt + 1);
            display.setColor(DisplayDriver::LIGHT);
          }
          if (_ctx_menu.active) _ctx_menu.render(display);
          return ret;
        }
        return 500;
      }

      int hist_start_y = display.headerH();
      int cby = display.height() - lh - 2;

      char title[24];
      display.setColor(DisplayDriver::LIGHT);
      snprintf(title, sizeof(title), "%.23s", filtered_name);
      display.drawTextCentered(display.width()/2, 0, title);
      display.fillRect(0, lh + 1, display.width(), display.sepH());

      int dm_count = dmHistCountForContact(_sel_contact.id.pub_key);
      uint32_t now_ts = rtc_clock.getCurrentTime();
      bool is_room = (_sel_contact.type == ADV_TYPE_ROOM);

      // Portrait e-ink (height > width): variable-height boxes that show the full
      // wrapped message text. All other displays/orientations: compact 2-line boxes.
      bool portrait_expand = (display.height() > display.width());
      const int MAX_VIS_BOXES = 8;
      int box_ys[MAX_VIS_BOXES], box_hs[MAX_VIS_BOXES], n_vis = 0;
      // Fixed-track scrollbar metrics + a stable gutter reserve (decided by a
      // whole-list fit test, not last frame's visible count) so message boxes
      // don't reflow their width as messages arrive.
      HistScroll hs = computeHistScroll(display, portrait_expand, dm_count, _dm_hist_scroll,
          hist_start_y, cby, lh,
          [&](int idx) -> const char* {
            int rp = dmHistEntryForContact(_sel_contact.id.pub_key, idx);
            return rp >= 0 ? _dm_hist[rp].text : nullptr;
          });
      int reserve = hs.reserve;
      {
        const int fixed_bh = 2 * lh + 1;
        int cur_y = hist_start_y;
        for (int ii = 0; ii < MAX_VIS_BOXES && (_dm_hist_scroll + ii) < dm_count; ii++) {
          int bh = fixed_bh;
          if (portrait_expand) {
            int rp = dmHistEntryForContact(_sel_contact.id.pub_key, _dm_hist_scroll + ii);
            if (rp >= 0) {
              char hsb[33];
              const char* hbody = dmDisplayParts(_dm_hist[rp], is_room, filtered_name, hsb, sizeof(hsb));
              display.translateUTF8ToBlocks(s_wrap_trans, hbody, sizeof(s_wrap_trans));
              int nl = FullscreenMsgView::wrapLines(display, s_wrap_trans, display.width() - 6 - reserve, s_wrap_lines, 8);
              bh = (1 + (nl > 0 ? nl : 1)) * lh + 1;
            }
          }
          if (cur_y + bh > cby) break;
          box_ys[n_vis] = cur_y; box_hs[n_vis] = bh; n_vis++;
          cur_y += bh + (portrait_expand ? 2 : 1);
        }
      }
      _hist_visible = n_vis;
      for (int i = 0; i < n_vis && (_dm_hist_scroll + i) < dm_count; i++) {
        int item = _dm_hist_scroll + i;
        bool sel = (item == _dm_hist_sel);
        int y = box_ys[i];
        int bh = box_hs[i];

        int ring_pos = dmHistEntryForContact(_sel_contact.id.pub_key, item);
        if (ring_pos < 0) continue;

        const DmHistEntry& e = _dm_hist[ring_pos];
        char sender_buf[33];
        const char* body = dmDisplayParts(e, is_room, filtered_name, sender_buf, sizeof(sender_buf));
        const char* sender = sender_buf;

        char age[6]; fmtMsgAge(age, sizeof(age), e.timestamp, now_ts);
        int age_w = age[0] ? display.getTextWidth(age) + 3 : 0;

        if (sel) {
          display.setColor(DisplayDriver::LIGHT);
          display.fillRect(0, y, display.width() - reserve, bh);
          display.setColor(DisplayDriver::DARK);
        } else {
          display.setColor(DisplayDriver::LIGHT);
          display.drawRect(0, y, display.width() - reserve, bh);
          display.fillRect(1, y + 1, display.width() - 2 - reserve, lh);
          display.setColor(DisplayDriver::DARK);
        }
        display.drawTextEllipsized(3, y + 1, display.width() - cw - 2 - age_w - reserve, sender);
        if (e.outgoing) {                       // delivery marker after "Me"
          int gx = 3 + display.getTextWidth(sender) + 3;
          drawAckGlyph(display, gx, y + 1, dmEffectiveStatus(e), e.attempt + 1);
        }
        if (age[0]) { display.setCursor(display.width() - age_w - reserve, y + 1); display.print(age); }
        if (!sel) display.setColor(DisplayDriver::LIGHT);
        if (portrait_expand) {
          display.translateUTF8ToBlocks(s_wrap_trans, body, sizeof(s_wrap_trans));
          int nl = FullscreenMsgView::wrapLines(display, s_wrap_trans, display.width() - 6 - reserve, s_wrap_lines, 8);
          for (int li = 0; li < nl; li++) { display.setCursor(3, y + (li + 1) * lh + 1); display.print(s_wrap_lines[li]); }
        } else {
          display.drawTextEllipsized(3, y + lh + 1, display.width() - cw - 2, body);
        }
      }

      if (dm_count == 0) {
        display.setColor(DisplayDriver::LIGHT);
        display.drawTextCentered(display.width()/2, display.height()/2, "No messages yet");
      }

      // Scrollbar: track pinned to the full list area; thumb sized/positioned
      // from hs's pixel metrics (stable while scrolling the same list).
      if (hs.need)
        drawScrollIndicatorPx(display, hist_start_y + 1, hs.view_px,
                              hs.total_px, hs.view_px, hs.scroll_px);

      bool compose_sel = (_dm_hist_sel == -1);
      const char* ctxt = "[+ send]";
      int ctw = display.getTextWidth(ctxt);
      int cbx = 1;
      if (compose_sel) {
        display.fillRect(cbx, cby - 1, ctw + 4, lh + 1);
        display.setColor(DisplayDriver::DARK);
      }
      display.setCursor(cbx + 2, cby);
      display.print(ctxt);
      display.setColor(DisplayDriver::LIGHT);
      if (_ctx_menu.active) _ctx_menu.render(display);
      return dm_count > 0 ? 500 : 2000;

    } else if (_phase == CHANNEL_HIST) {
      if (_fs.active && _hist_sel >= 0) {
        int fs_hist_count = histCountForChannel(_sel_channel_idx);
        int ring_pos = histEntryForChannel(_sel_channel_idx, _hist_sel);
        if (ring_pos >= 0) {
          const char* ftext = _hist[ring_pos].text;
          const char* fsep = strstr(ftext, ": ");
          char fsender[33], fmsg[MSG_TEXT_BUF];
          if (fsep) {
            int nl = fsep - ftext; if (nl > (int)sizeof(fsender) - 1) nl = sizeof(fsender) - 1;
            strncpy(fsender, ftext, nl); fsender[nl] = '\0';
            strncpy(fmsg, fsep + 2, sizeof(fmsg) - 1); fmsg[sizeof(fmsg)-1] = '\0';
          } else {
            strcpy(fsender, "?");
            strncpy(fmsg, ftext, sizeof(fmsg) - 1); fmsg[sizeof(fmsg)-1] = '\0';
          }
          int ret = _fs.render(display, fsender, fmsg,
                               _hist_sel < fs_hist_count - 1,
                               _hist_sel > 0);
          // Channels: ✓ only once a repeater echo confirms relay (see list view).
          if (strcmp(fsender, "Me") == 0 && _hist[ring_pos].relay_status == ACK_OK) {
            display.setColor(DisplayDriver::DARK);
            drawAckGlyph(display, 2 + display.getTextWidth(fsender) + 3, 1, ACK_OK);
            display.setColor(DisplayDriver::LIGHT);
          }
          if (_ctx_menu.active) _ctx_menu.render(display);
          return ret;
        }
        return 2000;
      }

      int hist_start_y = display.headerH();
      int cby = display.height() - lh - 2;

      ChannelDetails ch;
      the_mesh.getChannel(_sel_channel_idx, ch);
      char title[24];
      snprintf(title, sizeof(title), "%.23s", ch.name);
      display.drawTextCentered(display.width()/2, 0, title);
      display.fillRect(0, lh + 1, display.width(), display.sepH());

      int ch_hist_count = histCountForChannel(_sel_channel_idx);
      uint32_t now_ts = rtc_clock.getCurrentTime();

      // Portrait e-ink (height > width): variable-height boxes that show the full
      // wrapped message text. All other displays/orientations: compact 2-line boxes.
      bool portrait_expand = (display.height() > display.width());
      const int MAX_VIS_BOXES = 8;
      int box_ys[MAX_VIS_BOXES], box_hs[MAX_VIS_BOXES], n_vis = 0;
      // Fixed-track scrollbar metrics + stable gutter reserve (see DM history above).
      HistScroll hs = computeHistScroll(display, portrait_expand, ch_hist_count, _hist_scroll,
          hist_start_y, cby, lh,
          [&](int idx) -> const char* {
            int rp = histEntryForChannel(_sel_channel_idx, idx);
            if (rp < 0) return nullptr;
            const char* t = _hist[rp].text;
            const char* s = strstr(t, ": ");
            return s ? s + 2 : t;
          });
      int reserve = hs.reserve;
      {
        const int fixed_bh = 2 * lh + 1;
        int cur_y = hist_start_y;
        for (int ii = 0; ii < MAX_VIS_BOXES && (_hist_scroll + ii) < ch_hist_count; ii++) {
          int bh = fixed_bh;
          if (portrait_expand) {
            int rp = histEntryForChannel(_sel_channel_idx, _hist_scroll + ii);
            if (rp >= 0) {
              const char* rtext = _hist[rp].text;
              const char* rsep = strstr(rtext, ": ");
              const char* rbody = rsep ? rsep + 2 : rtext;
              display.translateUTF8ToBlocks(s_wrap_trans, skipReplyPrefix(rbody), sizeof(s_wrap_trans));
              int nl = FullscreenMsgView::wrapLines(display, s_wrap_trans, display.width() - 6 - reserve, s_wrap_lines, 8);
              bh = (1 + (nl > 0 ? nl : 1)) * lh + 1;
            }
          }
          if (cur_y + bh > cby) break;
          box_ys[n_vis] = cur_y; box_hs[n_vis] = bh; n_vis++;
          cur_y += bh + (portrait_expand ? 2 : 1);
        }
      }
      _hist_visible = n_vis;
      updateChannelUnread();  // mark everything in the just-computed visible window as seen

      for (int i = 0; i < n_vis && (_hist_scroll + i) < ch_hist_count; i++) {
        int item = _hist_scroll + i;
        bool sel = (item == _hist_sel);
        int y = box_ys[i];
        int bh = box_hs[i];

        int ring_pos = histEntryForChannel(_sel_channel_idx, item);
        if (ring_pos < 0) continue;

        const char* text = _hist[ring_pos].text;
        const char* sep = strstr(text, ": ");
        char sender[33], msg_part[MSG_TEXT_BUF];
        if (sep) {
          int nl = sep - text; if (nl > (int)sizeof(sender) - 1) nl = sizeof(sender) - 1;
          strncpy(sender, text, nl); sender[nl] = '\0';
          strncpy(msg_part, sep + 2, sizeof(msg_part) - 1);
          msg_part[sizeof(msg_part) - 1] = '\0';
        } else {
          strcpy(sender, "?");
          strncpy(msg_part, text, sizeof(msg_part) - 1);
          msg_part[sizeof(msg_part) - 1] = '\0';
        }

        char age[6]; fmtMsgAge(age, sizeof(age), _hist[ring_pos].timestamp, now_ts);
        int age_w = age[0] ? display.getTextWidth(age) + 3 : 0;

        if (sel) {
          display.setColor(DisplayDriver::LIGHT);
          display.fillRect(0, y, display.width() - reserve, bh);
          display.setColor(DisplayDriver::DARK);
        } else {
          display.setColor(DisplayDriver::LIGHT);
          display.drawRect(0, y, display.width() - reserve, bh);
          display.fillRect(1, y + 1, display.width() - 2 - reserve, lh);
          display.setColor(DisplayDriver::DARK);
        }
        display.drawTextEllipsized(3, y + 1, display.width() - cw - 2 - age_w - reserve, sender);
        // Channels have no recipient ACK — only show ✓ once a repeater echo
        // confirms the send was relayed into the mesh; otherwise no marker
        // (absence is normal, not a failure).
        if (strcmp(sender, "Me") == 0 && _hist[ring_pos].relay_status == ACK_OK) {
          int gx = 3 + display.getTextWidth(sender) + 3;
          drawAckGlyph(display, gx, y + 1, ACK_OK);
        }
        if (age[0]) { display.setCursor(display.width() - age_w - reserve, y + 1); display.print(age); }
        if (!sel) display.setColor(DisplayDriver::LIGHT);
        if (portrait_expand) {
          display.translateUTF8ToBlocks(s_wrap_trans, skipReplyPrefix(msg_part), sizeof(s_wrap_trans));
          int nl = FullscreenMsgView::wrapLines(display, s_wrap_trans, display.width() - 6 - reserve, s_wrap_lines, 8);
          for (int li = 0; li < nl; li++) { display.setCursor(3, y + (li + 1) * lh + 1); display.print(s_wrap_lines[li]); }
        } else {
          display.drawTextEllipsized(3, y + lh + 1, display.width() - cw - 2, skipReplyPrefix(msg_part));
        }
      }

      if (ch_hist_count == 0) {
        display.setColor(DisplayDriver::LIGHT);
        display.drawTextCentered(display.width()/2, display.height()/2, "No messages yet");
      }

      // Scrollbar: track pinned to the full list area; thumb from hs metrics.
      if (hs.need)
        drawScrollIndicatorPx(display, hist_start_y + 1, hs.view_px,
                              hs.total_px, hs.view_px, hs.scroll_px);

      // small compose button (bottom-left, always bordered, inverted when selected)
      bool compose_sel = (_hist_sel == -1);
      const char* ctxt = "[+ send]";
      int ctw = display.getTextWidth(ctxt);
      int cbx = 1;
      if (compose_sel) {
        display.setColor(DisplayDriver::LIGHT);
        display.fillRect(cbx - 1, cby - 1, ctw + 4, lh + 2);
        display.setColor(DisplayDriver::DARK);
      } else {
        display.setColor(DisplayDriver::LIGHT);
        display.drawRect(cbx - 1, cby - 1, ctw + 4, lh + 2);
      }
      display.setCursor(cbx + 1, cby);
      display.print(ctxt);
      display.setColor(DisplayDriver::LIGHT);
      if (_ctx_menu.active) _ctx_menu.render(display);

    } else if (_phase == KEYBOARD) {
      return _kb->render(display);

    } else { // MSG_PICK
      char title[24];
      if (_reply_mode) {
        int rlen = (int)strlen(_reply_prefix) - 4; // exclude "@[" and "] "
        if (rlen < 0) rlen = 0;
        if (rlen > 20) rlen = 20;
        char nick_raw[32], nick_trans[32];
        snprintf(nick_raw, sizeof(nick_raw), "%.*s", rlen, _reply_prefix + 2);
        display.translateUTF8ToBlocks(nick_trans, nick_raw, sizeof(nick_trans));
        snprintf(title, sizeof(title), "RE:%s", nick_trans);
      } else if (_sending_to_channel) {
        ChannelDetails ch;
        the_mesh.getChannel(_sel_channel_idx, ch);
        snprintf(title, sizeof(title), "%.23s", ch.name);
      } else {
        snprintf(title, sizeof(title), "TO:%.14s", _sel_contact.name);
      }
      display.drawCenteredHeader(title);

      int total_msg_items = 1 + _active_msg_count;
      drawList(display, total_msg_items, _msg_sel, _msg_scroll, [&](int idx, int y, bool sel, int reserve) {
        display.drawSelectionRow(0, y - 1, display.width() - reserve, item_h - 1, sel);

        if (idx == 0) {
          display.setCursor(2, y);
          display.print("Custom message...");
        } else {
          NodePrefs* p = _task->getNodePrefs();
          int slot = _active_msgs[idx - 1];
          const char* tmpl = p ? p->custom_msgs[slot] : "";
          display.drawTextEllipsized(2, y, display.width() - 4 - reserve, tmpl);
        }
      });
    }
    return 2000;
  }

  bool handleInput(char c) override {
    // Navigate view: any back key returns to the message it was opened from.
    if (_nav_active) {
      if (c == KEY_CANCEL || c == KEY_ENTER || c == KEY_CONTEXT_MENU) _nav_active = false;
      return true;
    }
    if (_phase == MODE_SELECT) {
      // Context menu (Mark-all-read) takes precedence while active.
      if (_ctx_menu.active) {
        auto res = _ctx_menu.handleInput(c);
        if (res == PopupMenu::SELECTED) {
          int cleared = 0;
          if (_mode_sel == 0) {
            cleared = getDMUnreadTotal();
            _task->clearAllDMUnread();
          } else if (_mode_sel == 1) {
            cleared = _task->getChannelUnreadCount();
            clearAllChannelUnread();
          } else {
            cleared = _task->getRoomUnreadCount();
            _task->clearRoomUnread();
          }
          markReadAlert(cleared);
        }
        return true;
      }
      if (c == KEY_CANCEL) { _task->gotoHomeScreen(); return true; }
      if (c == KEY_UP)   { _mode_sel = (_mode_sel > 0) ? _mode_sel - 1 : 2; return true; }
      if (c == KEY_DOWN) { _mode_sel = (_mode_sel < 2) ? _mode_sel + 1 : 0; return true; }
      if (c == KEY_CONTEXT_MENU) {
        // PopupMenu stores the title pointer verbatim — use static strings.
        static const char* MODE_TITLES[] = { "DM options", "Channel options", "Room options" };
        _ctx_menu.begin(MODE_TITLES[_mode_sel < 3 ? _mode_sel : 0], 1);
        _ctx_menu.addItem("Mark all read");
        return true;
      }
      if (c == KEY_ENTER) {
        if (_mode_sel == 1) {
          _phase = CHANNEL_PICK;
        } else {
          _room_mode = (_mode_sel == 2);
          if (_room_mode) _task->clearRoomUnread();
          buildContactList();
          _contact_sel = _contact_scroll = 0;
          _phase = CONTACT_PICK;
        }
        return true;
      }

    } else if (_phase == CONTACT_PICK) {
      // Context menu consumes all input while open
      if (_ctx_menu.active) {
        // LEFT/RIGHT cycle Notif/Melody in-place (menu stays open).
        if (!_pin_picker_active && _num_contacts > 0) {
          bool left  = (c == KEY_LEFT || c == KEY_PREV);
          bool right = (c == KEY_RIGHT || c == KEY_NEXT);
          if (left || right) {
            static const char* NOTIF_LABELS[] = { "default", "OFF", "ON" };
            static const char* ML[]           = { "global", "M1", "M2" };
            ContactInfo ci;
            if (the_mesh.getContactByIdx(_sorted[_contact_sel], ci)) {
              int sel = _ctx_menu.selectedIndex();
              if (sel == 1) {
                uint8_t v = dmNotifState(ci.id.pub_key);
                v = right ? (v + 1) % 3 : (v + 2) % 3;
                setDmNotifState(ci.id.pub_key, v);
                snprintf(_ctx_notif_item, sizeof(_ctx_notif_item), "Notif: %s", NOTIF_LABELS[v]);
                _ctx_dirty = true;
              } else if (sel == 2) {
                uint8_t v = dmMelodySlot(ci.id.pub_key);
                v = right ? (v + 1) % 3 : (v + 2) % 3;
                setDmMelody(ci.id.pub_key, v);
                snprintf(_ctx_melody_item, sizeof(_ctx_melody_item), "Melody: %s", ML[v]);
                _ctx_dirty = true;
              }
            }
            return true;
          }
        }
        auto res = _ctx_menu.handleInput(c);
        if (_pin_picker_active) {
          // Slot picker sub-menu: index 0..FAVOURITES_COUNT-1 maps directly to slot.
          if (res == PopupMenu::SELECTED && _num_contacts > 0) {
            ContactInfo ci;
            if (the_mesh.getContactByIdx(_sorted[_contact_sel], ci)) {
              int slot = _ctx_menu.selectedIndex();
              _task->setFavouriteSlot(slot, ci.id.pub_key);
              the_mesh.savePrefs();
              char alert[24];
              snprintf(alert, sizeof(alert), "Pinned to slot %d", slot + 1);
              _task->showAlert(alert, 800);
            }
          }
          if (res != PopupMenu::NONE) _pin_picker_active = false;
          return true;
        }
        if (res == PopupMenu::SELECTED && _num_contacts > 0) {
          ContactInfo ci;
          if (the_mesh.getContactByIdx(_sorted[_contact_sel], ci)) {
            int sel = _ctx_menu.selectedIndex();
            if (sel == 0) {
              int cleared = (int)_task->getDMUnread(ci.id.pub_key);
              _task->clearDMUnread(ci.id.pub_key);
              markReadAlert(cleared);
            } else if (sel == 3) {
              // Pin / Unpin
              int pinned_slot = _task->findFavouriteSlot(ci.id.pub_key);
              if (pinned_slot >= 0) {
                _task->clearFavouriteSlot(pinned_slot);
                the_mesh.savePrefs();
                char alert[24];
                snprintf(alert, sizeof(alert), "Unpinned (slot %d)", pinned_slot + 1);
                _task->showAlert(alert, 800);
              } else {
                for (int s = 0; s < NodePrefs::FAVOURITES_COUNT; s++) {
                  if (_task->isFavouriteSlotEmpty(s)) {
                    snprintf(_pin_slot_labels[s], sizeof(_pin_slot_labels[s]), "Slot %d: empty", s + 1);
                  } else {
                    char nm[16] = "?";
                    NodePrefs* p = _task->getNodePrefs();
                    if (p) {
                      const uint8_t* pfx = p->favourite_contacts[s];
                      for (int idx = 0; ; idx++) {
                        ContactInfo c2;
                        if (!the_mesh.getContactByIdx(idx, c2)) break;
                        if (memcmp(c2.id.pub_key, pfx, NodePrefs::FAVOURITE_PREFIX_LEN) == 0) {
                          DisplayDriver::translateUTF8Static(nm, c2.name, sizeof(nm));
                          break;
                        }
                      }
                    }
                    snprintf(_pin_slot_labels[s], sizeof(_pin_slot_labels[s]), "Slot %d: %s", s + 1, nm);
                  }
                }
                _ctx_menu.begin("Pick slot", 3);
                for (int s = 0; s < NodePrefs::FAVOURITES_COUNT; s++) _ctx_menu.addItem(_pin_slot_labels[s]);
                _pin_picker_active = true;
              }
            }
            // sel == 1 (Notif) and sel == 2 (Melody): already cycled via LEFT/RIGHT; ENTER just closes.
          }
        }
        if (res != PopupMenu::NONE && _ctx_dirty) {
          the_mesh.savePrefs(); _ctx_dirty = false;
        }
        return true;
      }
      if (c == KEY_CANCEL) { _room_mode = false; _phase = MODE_SELECT; return true; }
      // drawList() reclamps _contact_scroll from _contact_sel every render.
      if (c == KEY_UP   && _num_contacts > 0) { _contact_sel = (_contact_sel > 0) ? _contact_sel - 1 : _num_contacts - 1; return true; }
      if (c == KEY_DOWN && _num_contacts > 0) { _contact_sel = (_contact_sel < _num_contacts - 1) ? _contact_sel + 1 : 0; return true; }
      if (c == KEY_ENTER && _num_contacts > 0) {
        if (the_mesh.getContactByIdx(_sorted[_contact_sel], _sel_contact)) {
          if (_pick_target) { commitPickTargetDM(_sel_contact); return true; }
          _task->clearDMUnread(_sel_contact.id.pub_key);
          _dm_hist_sel = -1;
          _dm_hist_scroll = 0;
          _dm_fs.active = false;
          _phase = DM_HIST;
          if (_share_mode) beginShareCompose(false);
        }
        return true;
      }
      if (c == KEY_CONTEXT_MENU && _num_contacts > 0 && !_room_mode) {
        static const char* NOTIF_LABELS[] = { "default", "OFF", "ON" };
        ContactInfo ci;
        the_mesh.getContactByIdx(_sorted[_contact_sel], ci);
        snprintf(_ctx_notif_item, sizeof(_ctx_notif_item), "Notif: %s",
                 NOTIF_LABELS[dmNotifState(ci.id.pub_key)]);
        { static const char* ML[] = { "global", "M1", "M2" };
          snprintf(_ctx_melody_item, sizeof(_ctx_melody_item), "Melody: %s",
                   ML[dmMelodySlot(ci.id.pub_key)]); }
        int pinned_slot = _task->findFavouriteSlot(ci.id.pub_key);
        if (pinned_slot >= 0) snprintf(_ctx_pin_item, sizeof(_ctx_pin_item), "Unpin (slot %d)", pinned_slot + 1);
        else                  snprintf(_ctx_pin_item, sizeof(_ctx_pin_item), "Pin to dial");
        _ctx_menu.begin("Contact options", 3);
        _ctx_menu.addItem("Mark as read");
        _ctx_menu.addItem(_ctx_notif_item);
        _ctx_menu.addItem(_ctx_melody_item);
        _ctx_menu.addItem(_ctx_pin_item);
        _ctx_dirty = false;
        return true;
      }

    } else if (_phase == CHANNEL_PICK) {
      // Context menu consumes all input while open
      if (_ctx_menu.active) {
        // LEFT/RIGHT cycle Notif/Melody/Fav in-place (menu stays open).
        if (_num_channels > 0) {
          bool left  = (c == KEY_LEFT || c == KEY_PREV);
          bool right = (c == KEY_RIGHT || c == KEY_NEXT);
          if (left || right) {
            static const char* NOTIF_LABELS[] = { "default", "OFF", "ON" };
            static const char* ML[]           = { "global", "M1", "M2" };
            uint8_t ch_idx = _channel_indices[_channel_sel];
            int sel = _ctx_menu.selectedIndex();
            if (sel == 1) {
              uint8_t v = chNotifState(ch_idx);
              v = right ? (v + 1) % 3 : (v + 2) % 3;
              setChNotifState(ch_idx, v);
              snprintf(_ctx_notif_item, sizeof(_ctx_notif_item), "Notif: %s", NOTIF_LABELS[v]);
              _ctx_dirty = true;
            } else if (sel == 2) {
              uint8_t v = chNotifMelody(ch_idx);
              v = right ? (v + 1) % 3 : (v + 2) % 3;
              setChNotifMelody(ch_idx, v);
              snprintf(_ctx_melody_item, sizeof(_ctx_melody_item), "Melody: %s", ML[v]);
              _ctx_dirty = true;
            } else if (sel == 3) {
              NodePrefs* p2 = _task->getNodePrefs();
              if (p2) {
                p2->ch_fav_bitmask ^= (1ULL << ch_idx);
                bool is_fav = (p2->ch_fav_bitmask & (1ULL << ch_idx));
                snprintf(_ctx_ch_fav_item, sizeof(_ctx_ch_fav_item), is_fav ? "Fav: yes" : "Fav: no");
                _ctx_dirty = true;
                buildChannelList();
                if (_channel_sel >= _num_channels) _channel_sel = _num_channels > 0 ? _num_channels - 1 : 0;
              }
            }
            return true;
          }
        }
        auto res = _ctx_menu.handleInput(c);
        if (res == PopupMenu::SELECTED && _num_channels > 0) {
          uint8_t ch_idx = _channel_indices[_channel_sel];
          int sel = _ctx_menu.selectedIndex();
          if (sel == 0) {
            int cleared = (int)_ch_unread[ch_idx];
            _ch_unread[ch_idx] = 0;
            markReadAlert(cleared);
          }
          // sel 1/2/3 already handled by LEFT/RIGHT; ENTER just closes.
        }
        if (res != PopupMenu::NONE && _ctx_dirty) {
          the_mesh.savePrefs(); _ctx_dirty = false;
        }
        return true;
      }
      if (c == KEY_CANCEL) { _phase = MODE_SELECT; return true; }
      // drawList() reclamps _channel_scroll from _channel_sel every render.
      if (c == KEY_UP   && _num_channels > 0) { _channel_sel = (_channel_sel > 0) ? _channel_sel - 1 : _num_channels - 1; return true; }
      if (c == KEY_DOWN && _num_channels > 0) { _channel_sel = (_channel_sel < _num_channels - 1) ? _channel_sel + 1 : 0; return true; }
      if (c == KEY_ENTER && _num_channels > 0) {
        _sel_channel_idx = _channel_indices[_channel_sel];
        if (_pick_target) { commitPickTargetChannel(_sel_channel_idx); return true; }
        int hc = histCountForChannel(_sel_channel_idx);
        _unread_at_entry = (int)_ch_unread[_sel_channel_idx];
        _hist_scroll = 0;
        _hist_sel = hc > 0 ? 0 : -1;
        _viewing_max_seen = _hist_sel >= 0 ? _hist_sel : 0;
        _phase = CHANNEL_HIST;
        // Not updateChannelUnread() here: _hist_visible is still whatever this
        // channel's first render() hasn't computed yet (stale, shared with
        // DM_HIST) — calling it now could ratchet _viewing_max_seen past what's
        // actually about to be shown. render() calls it once that's fresh.
        if (_share_mode) beginShareCompose(true);
        return true;
      }
      if (c == KEY_CONTEXT_MENU && _num_channels > 0) {
        uint8_t ch_idx = _channel_indices[_channel_sel];
        static const char* NOTIF_LABELS[] = { "default", "OFF", "ON" };
        snprintf(_ctx_notif_item, sizeof(_ctx_notif_item), "Notif: %s",
                 NOTIF_LABELS[chNotifState(ch_idx)]);
        { static const char* ML[] = { "global", "M1", "M2" };
          snprintf(_ctx_melody_item, sizeof(_ctx_melody_item), "Melody: %s",
                   ML[chNotifMelody(ch_idx)]); }
        { NodePrefs* p2 = _task->getNodePrefs();
          bool is_fav = p2 && (p2->ch_fav_bitmask & (1ULL << ch_idx));
          snprintf(_ctx_ch_fav_item, sizeof(_ctx_ch_fav_item), is_fav ? "Fav: yes" : "Fav: no"); }
        _ctx_menu.begin("Channel options", 4);
        _ctx_menu.addItem("Mark all read");
        _ctx_menu.addItem(_ctx_notif_item);
        _ctx_menu.addItem(_ctx_melody_item);
        _ctx_menu.addItem(_ctx_ch_fav_item);
        _ctx_dirty = false;
        return true;
      }

    } else if (_phase == DM_HIST) {
      int dm_count = dmHistCountForContact(_sel_contact.id.pub_key);
      if (_dm_fs.active) {
        if (_ctx_menu.active) {
          auto res = _ctx_menu.handleInput(c);
          if (res == PopupMenu::SELECTED) {
            dispatchFsAction(false);
          } else if (res != PopupMenu::NONE) {
            _ctx_menu.active = false;
          }
          return true;
        }
        auto res = _dm_fs.handleInput(c);
        if (res == FullscreenMsgView::PREV) {
          if (_dm_hist_sel < dm_count - 1) { _dm_hist_sel++; _dm_fs.scroll = 0; }
        } else if (res == FullscreenMsgView::NEXT) {
          if (_dm_hist_sel > 0) { _dm_hist_sel--; _dm_fs.scroll = 0; }
        } else if (res == FullscreenMsgView::CLOSE) {
          _dm_fs.active = false;
        } else if (res == FullscreenMsgView::REPLY) {
          int ring_pos = dmHistEntryForContact(_sel_contact.id.pub_key, _dm_hist_sel);
          if (ring_pos >= 0) {
            bool reply_ok = !_dm_hist[ring_pos].outgoing;
            if (reply_ok) {
              char _tname[32]; DisplayDriver::translateUTF8Static(_tname, _sel_contact.name, sizeof(_tname));
              snprintf(_reply_prefix, sizeof(_reply_prefix), "@[%.31s] ", _tname);
            }
            buildFsMenu(_dm_hist[ring_pos].text, reply_ok);
          }
        }
        return true;
      }
      if (_ctx_menu.active) {
        auto res = _ctx_menu.handleInput(c);
        if (res == PopupMenu::SELECTED) {
          dispatchFsAction(false);
        } else if (res != PopupMenu::NONE) {
          _ctx_menu.active = false;
        }
        return true;
      }
      if (c == KEY_CANCEL) {
        if (_dm_direct_entry) {
          _dm_direct_entry = false;
          _task->gotoHomeScreen();
        } else {
          _phase = CONTACT_PICK;
        }
        return true;
      }
      if (c == KEY_UP) {
        if (_dm_hist_sel > 0) {
          _dm_hist_sel--;
          if (_dm_hist_sel < _dm_hist_scroll) _dm_hist_scroll = _dm_hist_sel;
        } else if (_dm_hist_sel == 0) {
          _dm_hist_sel = -1;
        }
        return true;
      }
      if (c == KEY_DOWN) {
        if (_dm_hist_sel == -1 && dm_count > 0) { _dm_hist_sel = 0; _dm_hist_scroll = 0; }
        else if (_dm_hist_sel >= 0 && _dm_hist_sel < dm_count - 1) {
          _dm_hist_sel++;
          if (_dm_hist_sel >= _dm_hist_scroll + _hist_visible)
            _dm_hist_scroll = _dm_hist_sel - _hist_visible + 1;
        }
        return true;
      }
      if (c == KEY_ENTER) {
        if (_dm_hist_sel >= 0) {
          _dm_fs.begin();
        } else {
          _sending_to_channel = false;
          setupMsgPick();
          _phase = MSG_PICK;
        }
        return true;
      }
      if (c == KEY_CONTEXT_MENU && _dm_hist_sel >= 0) {
        int ring_pos = dmHistEntryForContact(_sel_contact.id.pub_key, _dm_hist_sel);
        if (ring_pos >= 0) {
          bool reply_ok = !_dm_hist[ring_pos].outgoing;
          if (reply_ok) {
            char _tname[32]; DisplayDriver::translateUTF8Static(_tname, _sel_contact.name, sizeof(_tname));
            snprintf(_reply_prefix, sizeof(_reply_prefix), "@[%.31s] ", _tname);
          }
          buildFsMenu(_dm_hist[ring_pos].text, reply_ok);
        }
        return true;
      }

    } else if (_phase == CHANNEL_HIST) {
      int ch_hist_count = histCountForChannel(_sel_channel_idx);
      if (_fs.active) {
        if (_ctx_menu.active) {
          auto res = _ctx_menu.handleInput(c);
          if (res == PopupMenu::SELECTED) {
            dispatchFsAction(true);
          } else if (res != PopupMenu::NONE) {
            _ctx_menu.active = false;
          }
          return true;
        }
        auto res = _fs.handleInput(c);
        if (res == FullscreenMsgView::PREV) {
          if (_hist_sel < ch_hist_count - 1) { _hist_sel++; _fs.scroll = 0; updateChannelUnread(); }
        } else if (res == FullscreenMsgView::NEXT) {
          if (_hist_sel > 0) { _hist_sel--; _fs.scroll = 0; updateChannelUnread(); }
        } else if (res == FullscreenMsgView::CLOSE) {
          _fs.active = false;
        } else if (res == FullscreenMsgView::REPLY) {
          int ring_pos = histEntryForChannel(_sel_channel_idx, _hist_sel);
          if (ring_pos >= 0)
            buildFsMenu(_hist[ring_pos].text, buildChannelReplyPrefix(_hist[ring_pos].text));
        }
        return true;
      }
      if (_ctx_menu.active) {
        auto res = _ctx_menu.handleInput(c);
        if (res == PopupMenu::SELECTED) {
          dispatchFsAction(true);
        } else if (res != PopupMenu::NONE) {
          _ctx_menu.active = false;
        }
        return true;
      }
      if (c == KEY_CANCEL) { _phase = CHANNEL_PICK; return true; }
      if (c == KEY_UP) {
        if (_hist_sel > 0) { _hist_sel--; if (_hist_sel < _hist_scroll) _hist_scroll = _hist_sel; }
        else if (_hist_sel == 0) _hist_sel = -1;
        updateChannelUnread();
        return true;
      }
      if (c == KEY_DOWN) {
        if (_hist_sel == -1 && ch_hist_count > 0) { _hist_sel = 0; _hist_scroll = 0; }
        else if (_hist_sel >= 0 && _hist_sel < ch_hist_count - 1) {
          _hist_sel++;
          if (_hist_sel >= _hist_scroll + _hist_visible) _hist_scroll = _hist_sel - _hist_visible + 1;
        }
        updateChannelUnread();
        return true;
      }
      if (c == KEY_ENTER) {
        if (_hist_sel >= 0) {
          _fs.begin();
          updateChannelUnread();
        } else {
          _sending_to_channel = true;
          setupMsgPick();
          _phase = MSG_PICK;
        }
        return true;
      }
      if (c == KEY_CONTEXT_MENU && _hist_sel >= 0) {
        int ring_pos = histEntryForChannel(_sel_channel_idx, _hist_sel);
        if (ring_pos >= 0)
          buildFsMenu(_hist[ring_pos].text, buildChannelReplyPrefix(_hist[ring_pos].text));
        return true;
      }

    } else if (_phase == KEYBOARD) {
      auto res = _kb->handleInput(c);
      if (res == KeyboardWidget::CANCELLED) {
        if (_share_mode) { _share_mode = false; _task->gotoHomeScreen(); }
        else             { _phase = MSG_PICK; }
      } else if (res == KeyboardWidget::DONE) {
        int prefix_len = _reply_mode ? (int)strlen(_reply_prefix) : 0;
        if (_kb->len > prefix_len) {
          // Expand only the body — prefix "@[nick] " is preserved verbatim, so a nick
          // that happens to contain a placeholder token isn't substituted.
          char expanded[KB_MAX_LEN + 1];
          if (prefix_len > 0) {
            memcpy(expanded, _kb->buf, prefix_len);
            expandMsg(_kb->buf + prefix_len, expanded + prefix_len, sizeof(expanded) - prefix_len);
          } else {
            expandMsg(_kb->buf, expanded, sizeof(expanded));
          }
          bool ok = sendText(expanded);
          afterSend(ok, expanded);
        }
      }
      return true;

    } else { // MSG_PICK
      int total_msg_items = 1 + _active_msg_count;
      if (c == KEY_CANCEL) {
        _reply_mode = false;
        _phase = _sending_to_channel ? CHANNEL_HIST : DM_HIST;
        return true;
      }
      // drawList() reclamps _msg_scroll from _msg_sel every render.
      if (c == KEY_UP)   { _msg_sel = (_msg_sel > 0) ? _msg_sel - 1 : total_msg_items - 1; return true; }
      if (c == KEY_DOWN) { _msg_sel = (_msg_sel < total_msg_items - 1) ? _msg_sel + 1 : 0; return true; }
      if (c == KEY_ENTER) {
        if (_msg_sel == 0) {
          _kb->begin(_reply_mode ? _reply_prefix : "");
          kbAddSensorPlaceholders(*_kb, &sensors);
          _phase = KEYBOARD;
          return true;
        }
        NodePrefs* p = _task->getNodePrefs();
        int slot = _active_msgs[_msg_sel - 1];
        const char* tmpl = p ? p->custom_msgs[slot] : "OK";
        char msg[MSG_TEXT_BUF];
        if (_reply_mode) {
          char body[MSG_TEXT_BUF];
          expandMsg(tmpl, body, sizeof(body));
          snprintf(msg, sizeof(msg), "%s%s", _reply_prefix, body);
        } else {
          expandMsg(tmpl, msg, sizeof(msg));
        }
        bool ok = sendText(msg);
        afterSend(ok, msg);
        return true;
      }
    }
    return false;
  }
};
