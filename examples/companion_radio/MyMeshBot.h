#pragma once
// Bot logic — not part of upstream MyMesh.cpp.
// Included at the bottom of MyMesh.cpp after all class definitions.

// Minimum gap between two auto-replies to the same contact (DM), on the bot
// channel, or on the bot room. Guards against floods and, with
// botTriggerMatches()'s loop check, against bot↔bot ping-pong on a shared
// channel/room.
static const unsigned long BOT_REPLY_COOLDOWN_MS = 10000UL;
// Scratch buffer size for trigger matching / reply expansion. Covers the full
// incoming message (MAX_TEXT_LEN ~160) plus the 140-byte reply templates.
static const int BOT_SCRATCH = 200;

// Case-insensitive (ASCII) substring test: does `body` contain `trigger`? A lone
// "*" trigger means "match everything" (away / reply-to-all mode); honoured only
// where allow_wildcard is set. DM, channel and room each pass their own trigger string.
bool MyMesh::botTriggerMatches(const char* trigger, const char* body, bool allow_wildcard) const {
  if (!trigger[0]) return false;             // empty trigger would match everything
  if (trigger[0] == '*' && trigger[1] == '\0')
    return allow_wildcard;

  char low[BOT_SCRATCH];
  size_t n = strlen(body);
  if (n >= sizeof(low)) n = sizeof(low) - 1;
  for (size_t i = 0; i < n; i++) low[i] = (char)tolower((uint8_t)body[i]);
  low[n] = '\0';

  char low_trigger[64];
  size_t tlen = strnlen(trigger, sizeof(low_trigger) - 1);
  for (size_t i = 0; i < tlen; i++) low_trigger[i] = (char)tolower((uint8_t)trigger[i]);
  low_trigger[tlen] = '\0';

  return strstr(low, low_trigger) != NULL;
}

// DM allow-list gate: when bot_dm_scope is favourites-only, ignore triggers/
// commands from any contact that isn't starred (ContactInfo::flags bit 0 —
// the same "favourite" bit the Messages screen's DM list filter reads), so a
// bot left on can't be spammed by strangers while still answering people the
// user has starred. Channel and room bots have no equivalent — they're
// inherently public to whoever's already on that channel/room.
bool MyMesh::botDmSenderAllowed(const ContactInfo& from) const {
  if (_prefs.bot_dm_scope == 0) return true;   // all (default)
  return (from.flags & 0x01) != 0;             // favourites only
}

// Resolves a room post's signed author (a 4-byte pubkey prefix carried
// alongside the post — `from` is the room server itself, not the poster) to a
// display name for the {name} placeholder. Falls back to a generic label when
// the poster isn't a known contact (unverified beyond the 4-byte prefix).
void MyMesh::botRoomSenderName(const uint8_t* sender_prefix, char* out, int out_len) {
  ContactInfo* sc = sender_prefix ? lookupContactByPubKey(sender_prefix, 4) : nullptr;
  if (sc && sc->name[0]) snprintf(out, out_len, "%s", sc->name);
  else                   snprintf(out, out_len, "someone");
}

// Per-contact DM throttle: true if enough time has passed (or we've never
// replied) to this contact. Only the first 4 key bytes are compared — ample to
// tell local contacts apart.
bool MyMesh::botDmAllowed(const uint8_t* pubkey) {
  unsigned long now = millis();
  for (int i = 0; i < BOT_DM_LOG_SIZE; i++) {
    if (_bot_dm_log[i].used && memcmp(_bot_dm_log[i].key, pubkey, 4) == 0)
      return now - _bot_dm_log[i].t_ms > BOT_REPLY_COOLDOWN_MS;
  }
  return true;
}

void MyMesh::botDmRecord(const uint8_t* pubkey) {
  unsigned long now = millis();
  for (int i = 0; i < BOT_DM_LOG_SIZE; i++) {  // refresh existing entry
    if (_bot_dm_log[i].used && memcmp(_bot_dm_log[i].key, pubkey, 4) == 0) {
      _bot_dm_log[i].t_ms = now;
      return;
    }
  }
  int target = -1;                             // else take a free slot...
  for (int i = 0; i < BOT_DM_LOG_SIZE; i++)
    if (!_bot_dm_log[i].used) { target = i; break; }
  if (target < 0) {                            // ...or evict the oldest
    target = 0;
    for (int i = 1; i < BOT_DM_LOG_SIZE; i++)
      if (_bot_dm_log[i].t_ms < _bot_dm_log[target].t_ms) target = i;
  }
  memcpy(_bot_dm_log[target].key, pubkey, 4);
  _bot_dm_log[target].t_ms = now;
  _bot_dm_log[target].used = true;
}

// Quiet hours: when the local hour is inside [start, end) the auto-reply paths
// stay silent (commands are explicitly requested, so they ignore this). A zero-
// width window (start == end) disables the feature; a window where start > end
// wraps past midnight.
bool MyMesh::botInQuietHours() const {
  if (_prefs.bot_quiet_start == _prefs.bot_quiet_end) return false;  // disabled
  uint32_t utc = getRTCClock()->getCurrentTime();
  if (utc < 1000000000UL) return false;             // clock not set — don't suppress
  uint32_t local = utc + (int32_t)_prefs.tz_offset_hours * 3600;
  int h = (int)((local / 3600) % 24);
  int s = _prefs.bot_quiet_start, e = _prefs.bot_quiet_end;
  return (s < e) ? (h >= s && h < e)                // same-day window
                 : (h >= s || h < e);               // overnight window
}

void MyMesh::tryBotReplyDM(const ContactInfo& from, const char* text, uint8_t hops) {
  if (from.type != ADV_TYPE_CHAT) return;
  if (!(_prefs.bot_enabled && _prefs.bot_reply_dm[0])) return;
  if (!botDmSenderAllowed(from)) return;
  if (botInQuietHours()) return;
  if (!botTriggerMatches(_prefs.bot_trigger, text, true)) return;  // wildcard "*" allowed
  if (!botDmAllowed(from.id.pub_key)) return;        // already answered this contact recently

  uint32_t ts = getRTCClock()->getCurrentTime();
  char expanded[BOT_SCRATCH];
  expandMsg(_prefs.bot_reply_dm, expanded, sizeof(expanded),
            sensors.node_lat, sensors.node_lon,
            sensors.node_lat != 0.0 || sensors.node_lon != 0.0,
            ts, _prefs.tz_offset_hours,
            &sensors, (float)board.getBattMilliVolts() / 1000.0f,
            from.name, hops);
  uint32_t expected_ack, est_timeout;
  if (sendMessage(from, ts, 0, expanded, expected_ack, est_timeout) != MSG_SEND_FAILED) {
    botDmRecord(from.id.pub_key);
    _bot_reply_count++;
#ifdef DISPLAY_CLASS
    if (_ui) _ui->addDMMsg(from.id.pub_key, true, expanded);
#endif
  }
}

void MyMesh::tryBotReplyChannel(uint8_t channel_idx, const char* text, uint8_t hops) {
  // bot_channel_enabled is this target's own on/off switch — independent of
  // bot_enabled (the DM tab's Enable), matching how the commands paths and
  // the tabbed BotScreen UI already treat DM/channel/room as separate targets.
  if (!(_prefs.bot_channel_enabled && _prefs.bot_reply_ch[0] &&
        channel_idx == _prefs.bot_channel_idx &&
        millis() - _bot_last_ch_reply_ms > BOT_REPLY_COOLDOWN_MS))
    return;
  if (botInQuietHours()) return;

  // Split off the "SenderName: " prefix so the trigger is matched against the
  // message body only, and so the name is available for the {name}
  // placeholder. Unsigned/unverified — channel messages carry no identity
  // beyond this text convention.
  const char* msg = text;
  char sender_name[32] = "someone";
  const char* sep = strstr(text, ": ");
  if (sep) {
    msg = sep + 2;
    int n = (int)(sep - text);
    if (n > 0 && n < (int)sizeof(sender_name)) { memcpy(sender_name, text, n); sender_name[n] = '\0'; }
  }

  if (!botTriggerMatches(_prefs.bot_trigger_ch, msg, true)) return;  // own trigger; "*" = any msg

  ChannelDetails ch;
  if (!getChannel(channel_idx, ch)) return;

  uint32_t ts = getRTCClock()->getCurrentTime();
  char expanded[BOT_SCRATCH];
  expandMsg(_prefs.bot_reply_ch, expanded, sizeof(expanded),
            sensors.node_lat, sensors.node_lon,
            sensors.node_lat != 0.0 || sensors.node_lon != 0.0,
            ts, _prefs.tz_offset_hours,
            &sensors, (float)board.getBattMilliVolts() / 1000.0f,
            sender_name, hops);
  // Anti-loop: don't echo a message identical to our own reply (e.g. another bot
  // on the channel sending the same text), which would ping-pong forever. The
  // per-channel cooldown caps any residual back-and-forth between mismatched bots.
  if (strcmp(msg, expanded) == 0) return;
  int rlen = strlen(expanded);
  if (sendGroupMessage(ts, ch.channel, _prefs.node_name, expanded, rlen)) {
    _bot_last_ch_reply_ms = millis();
    _bot_reply_count++;
#ifdef DISPLAY_CLASS
    if (_ui) {
      char with_sender[240];  // node_name(32) + ": "(2) + expanded(200) + margin
      snprintf(with_sender, sizeof(with_sender), "%s: %s", _prefs.node_name, expanded);
      _ui->addChannelMsg(channel_idx, with_sender);
    }
#endif
  }
}

// Room-server counterpart to tryBotReplyChannel: a room also carries many
// posters, so it shares that function's "public reply, respects quiet hours +
// a single shared cooldown" shape, but the reply is a direct sendMessage() to
// the room server (same call used for a room's normal chat compose), like a
// DM, since the room server relays it to its members itself. Requires the
// device to already have an active login session with this room server (see
// NodePrefs::bot_room_prefix's doc comment) — otherwise sendMessage() still
// "succeeds" locally but the server silently drops the unauthorized post.
void MyMesh::tryBotReplyRoom(const ContactInfo& from, const uint8_t* sender_prefix, const char* text, uint8_t hops) {
  if (from.type != ADV_TYPE_ROOM) return;
  // bot_room_enabled is this target's own on/off switch — independent of
  // bot_enabled, same reasoning as tryBotReplyChannel above.
  if (!(_prefs.bot_room_enabled && _prefs.bot_reply_room[0] &&
        memcmp(from.id.pub_key, _prefs.bot_room_prefix, NodePrefs::FAVOURITE_PREFIX_LEN) == 0 &&
        millis() - _bot_last_room_reply_ms > BOT_REPLY_COOLDOWN_MS))
    return;
  if (botInQuietHours()) return;
  if (!botTriggerMatches(_prefs.bot_trigger_room, text, true)) return;  // "*" = any post

  char sender_name[32];
  botRoomSenderName(sender_prefix, sender_name, sizeof(sender_name));

  uint32_t ts = getRTCClock()->getCurrentTime();
  char expanded[BOT_SCRATCH];
  expandMsg(_prefs.bot_reply_room, expanded, sizeof(expanded),
            sensors.node_lat, sensors.node_lon,
            sensors.node_lat != 0.0 || sensors.node_lon != 0.0,
            ts, _prefs.tz_offset_hours,
            &sensors, (float)board.getBattMilliVolts() / 1000.0f,
            sender_name, hops);
  // Anti-loop, mirrors the channel path: don't echo a message identical to
  // our own reply (e.g. re-synced back from the room server).
  if (strcmp(text, expanded) == 0) return;

  uint32_t expected_ack, est_timeout;
  if (sendMessage(from, ts, 0, expanded, expected_ack, est_timeout) != MSG_SEND_FAILED) {
    _bot_last_room_reply_ms = millis();
    _bot_reply_count++;
#ifdef DISPLAY_CLASS
    if (_ui) _ui->addDMMsg(from.id.pub_key, true, expanded);
#endif
  }
}

// Builds the reply text for a single command word into out. Returns false for an
// unknown command. !hops is dynamic (per received message; separate from the
// general {hops} placeholder so plain "!hops" keeps its terse "direct"/"N hops"
// reply without needing a template); the rest expand a static template via
// expandMsg's placeholders, including {name}/{hops} if the sender wrote them
// into a custom reply — not used by any of the built-in templates below today.
bool MyMesh::botCommandReply(const char* cmd, uint8_t hops, uint32_t ts, char* out, int out_len, const char* sender_name) {
  if (!strcmp(cmd, "hops")) {
    if (hops == 0) strncpy(out, "direct", out_len);     // heard directly (0 repeaters)
    else           snprintf(out, out_len, "%u hops", (unsigned)hops);
    out[out_len - 1] = '\0';
    return true;
  }
  const char* tmpl = nullptr;
  if      (!strcmp(cmd, "ping"))   tmpl = "pong";
  else if (!strcmp(cmd, "batt"))   tmpl = "{batt}";
  else if (!strcmp(cmd, "loc"))    tmpl = "{loc}";
  else if (!strcmp(cmd, "time"))   tmpl = "{time}";
  else if (!strcmp(cmd, "temp"))   tmpl = "{temp}";
  else if (!strcmp(cmd, "status")) tmpl = "batt {batt} loc {loc} at {time}";
  else if (!strcmp(cmd, "help"))   tmpl = "cmds: !ping !batt !loc !time !temp !hops !status";
  else return false;                             // unknown — let the trigger bot try

  expandMsg(tmpl, out, out_len,
            sensors.node_lat, sensors.node_lon,
            sensors.node_lat != 0.0 || sensors.node_lon != 0.0,
            ts, _prefs.tz_offset_hours,
            &sensors, (float)board.getBattMilliVolts() / 1000.0f,
            sender_name, hops);
  if (strchr(out, '{')) strcpy(out, "n/a");      // requested sensor not present
  return true;
}

// Scans body for any number of space-separated "!word" tokens, building a single
// combined reply (segments joined by " | ") into out. Returns how many commands
// were recognised. Shared by the DM, channel and room command paths.
int MyMesh::botScanCommands(const char* body, uint8_t hops, uint32_t ts, char* out, int out_len, const char* sender_name) {
  int oi = 0;
  int matched = 0;
  for (const char* p = body; *p; ) {
    while (*p == ' ') p++;                        // skip whitespace
    if (*p != '!') {                              // not a command token
      while (*p && *p != ' ') p++;
      continue;
    }
    p++;                                          // step past '!'
    char cmd[16];
    int ci = 0;
    while (*p && *p != ' ' && ci < (int)sizeof(cmd) - 1)
      cmd[ci++] = (char)tolower((uint8_t)*p++);
    cmd[ci] = '\0';
    while (*p && *p != ' ') p++;                  // consume any overflow of this token
    if (!cmd[0]) continue;

    char seg[64];
    if (!botCommandReply(cmd, hops, ts, seg, sizeof(seg), sender_name)) continue;  // unknown

    if (matched > 0 && oi + 3 < out_len) {        // " | " separator
      out[oi++] = ' '; out[oi++] = '|'; out[oi++] = ' ';
    }
    int sl = strlen(seg);
    if (oi + sl >= out_len) sl = out_len - 1 - oi;
    if (sl > 0) { memcpy(out + oi, seg, sl); oi += sl; }
    matched++;
  }
  out[oi] = '\0';
  return matched;
}

// DM query commands. The reply is a pull, not a push, so it ignores quiet hours;
// the per-contact throttle still applies. Returns true when at least one command
// was found (replied or throttled), so the caller skips the trigger-reply path.
bool MyMesh::tryBotCommand(const ContactInfo& from, const char* text, uint8_t hops) {
  if (!_prefs.bot_commands_enabled) return false;
  if (from.type != ADV_TYPE_CHAT) return false;
  if (!botDmSenderAllowed(from)) return false;

  uint32_t ts = getRTCClock()->getCurrentTime();
  char out[BOT_SCRATCH];
  if (botScanCommands(text, hops, ts, out, sizeof(out), from.name) == 0) return false;  // no commands
  if (!botDmAllowed(from.id.pub_key)) return true;                           // throttled

  uint32_t expected_ack, est_timeout;
  if (sendMessage(from, ts, 0, out, expected_ack, est_timeout) != MSG_SEND_FAILED) {
    botDmRecord(from.id.pub_key);
    _bot_reply_count++;
#ifdef DISPLAY_CLASS
    if (_ui) _ui->addDMMsg(from.id.pub_key, true, out);
#endif
  }
  return true;
}

// Channel query commands — only on the bot's monitored channel. The reply is
// broadcast to everyone on the channel, so unlike DM commands it respects quiet
// hours and uses the global per-channel cooldown (shared with the trigger reply).
bool MyMesh::tryBotChannelCommand(uint8_t channel_idx, const char* text, uint8_t hops) {
  if (!(_prefs.bot_commands_ch && _prefs.bot_channel_enabled &&
        channel_idx == _prefs.bot_channel_idx))
    return false;

  // Skip "sender_name: " prefix so commands are read from the message body only.
  const char* msg = text;
  char sender_name[32] = "someone";
  const char* sep = strstr(text, ": ");
  if (sep) {
    msg = sep + 2;
    int n = (int)(sep - text);
    if (n > 0 && n < (int)sizeof(sender_name)) { memcpy(sender_name, text, n); sender_name[n] = '\0'; }
  }

  uint32_t ts = getRTCClock()->getCurrentTime();
  char out[BOT_SCRATCH];
  if (botScanCommands(msg, hops, ts, out, sizeof(out), sender_name) == 0) return false;  // no commands
  if (botInQuietHours()) return true;                                       // quiet hours
  if (millis() - _bot_last_ch_reply_ms <= BOT_REPLY_COOLDOWN_MS) return true;  // throttled

  ChannelDetails ch;
  if (!getChannel(channel_idx, ch)) return true;
  int rlen = strlen(out);
  if (sendGroupMessage(ts, ch.channel, _prefs.node_name, out, rlen)) {
    _bot_last_ch_reply_ms = millis();
    _bot_reply_count++;
#ifdef DISPLAY_CLASS
    if (_ui) {
      char with_sender[240];
      snprintf(with_sender, sizeof(with_sender), "%s: %s", _prefs.node_name, out);
      _ui->addChannelMsg(channel_idx, with_sender);
    }
#endif
  }
  return true;
}

// Room query commands — mirrors tryBotChannelCommand: the reply posts to the
// room (visible to every member), so it respects quiet hours and the shared
// per-room cooldown, unlike the DM command path's private-pull exemption.
bool MyMesh::tryBotRoomCommand(const ContactInfo& from, const uint8_t* sender_prefix, const char* text, uint8_t hops) {
  if (from.type != ADV_TYPE_ROOM) return false;
  if (!(_prefs.bot_commands_room && _prefs.bot_room_enabled &&
        memcmp(from.id.pub_key, _prefs.bot_room_prefix, NodePrefs::FAVOURITE_PREFIX_LEN) == 0))
    return false;

  char sender_name[32];
  botRoomSenderName(sender_prefix, sender_name, sizeof(sender_name));

  uint32_t ts = getRTCClock()->getCurrentTime();
  char out[BOT_SCRATCH];
  if (botScanCommands(text, hops, ts, out, sizeof(out), sender_name) == 0) return false;  // no commands
  if (botInQuietHours()) return true;                                          // quiet hours
  if (millis() - _bot_last_room_reply_ms <= BOT_REPLY_COOLDOWN_MS) return true;   // throttled

  uint32_t expected_ack, est_timeout;
  if (sendMessage(from, ts, 0, out, expected_ack, est_timeout) != MSG_SEND_FAILED) {
    _bot_last_room_reply_ms = millis();
    _bot_reply_count++;
#ifdef DISPLAY_CLASS
    if (_ui) _ui->addDMMsg(from.id.pub_key, true, out);
#endif
  }
  return true;
}
