#pragma once
// Bot logic — not part of upstream MyMesh.cpp.
// Included at the bottom of MyMesh.cpp after all class definitions.

// Minimum gap between two auto-replies to the same contact (DM) or on the bot
// channel. Guards against floods and, with botTriggerMatches()'s loop check,
// against bot↔bot ping-pong on a shared channel.
static const unsigned long BOT_REPLY_COOLDOWN_MS = 10000UL;
// Scratch buffer size for trigger matching / reply expansion. Covers the full
// incoming message (MAX_TEXT_LEN ~160) plus the 140-byte reply templates.
static const int BOT_SCRATCH = 200;

// Case-insensitive (ASCII) substring test: does `body` contain `trigger`? A lone
// "*" trigger means "match everything" (away / reply-to-all mode); honoured only
// where allow_wildcard is set. DM and channel each pass their own trigger string.
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

void MyMesh::tryBotReplyDM(const ContactInfo& from, const char* text) {
  if (from.type != ADV_TYPE_CHAT) return;
  if (!(_prefs.bot_enabled && _prefs.bot_reply_dm[0])) return;
  if (botInQuietHours()) return;
  if (!botTriggerMatches(_prefs.bot_trigger, text, true)) return;  // wildcard "*" allowed
  if (!botDmAllowed(from.id.pub_key)) return;        // already answered this contact recently

  uint32_t ts = getRTCClock()->getCurrentTime();
  char expanded[BOT_SCRATCH];
  expandMsg(_prefs.bot_reply_dm, expanded, sizeof(expanded),
            sensors.node_lat, sensors.node_lon,
            sensors.node_lat != 0.0 || sensors.node_lon != 0.0,
            ts, _prefs.tz_offset_hours,
            &sensors, (float)board.getBattMilliVolts() / 1000.0f);
  uint32_t expected_ack, est_timeout;
  if (sendMessage(from, ts, 0, expanded, expected_ack, est_timeout) != MSG_SEND_FAILED) {
    botDmRecord(from.id.pub_key);
    _bot_reply_count++;
#ifdef DISPLAY_CLASS
    if (_ui) _ui->addDMMsg(from.id.pub_key, true, expanded);
#endif
  }
}

void MyMesh::tryBotReplyChannel(uint8_t channel_idx, const char* text) {
  if (!(_prefs.bot_enabled && _prefs.bot_channel_enabled && _prefs.bot_reply_ch[0] &&
        channel_idx == _prefs.bot_channel_idx &&
        millis() - _bot_last_ch_reply_ms > BOT_REPLY_COOLDOWN_MS))
    return;
  if (botInQuietHours()) return;

  // Skip "sender_name: " prefix so the trigger is only matched against the message body.
  const char* msg = text;
  const char* sep = strstr(text, ": ");
  if (sep) msg = sep + 2;

  if (!botTriggerMatches(_prefs.bot_trigger_ch, msg, true)) return;  // own trigger; "*" = any msg

  ChannelDetails ch;
  if (!getChannel(channel_idx, ch)) return;

  uint32_t ts = getRTCClock()->getCurrentTime();
  char expanded[BOT_SCRATCH];
  expandMsg(_prefs.bot_reply_ch, expanded, sizeof(expanded),
            sensors.node_lat, sensors.node_lon,
            sensors.node_lat != 0.0 || sensors.node_lon != 0.0,
            ts, _prefs.tz_offset_hours,
            &sensors, (float)board.getBattMilliVolts() / 1000.0f);
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

// Builds the reply text for a single command word into out. Returns false for an
// unknown command. !hops is dynamic (per received message); the rest expand a
// static template via expandMsg's placeholders.
bool MyMesh::botCommandReply(const char* cmd, uint8_t hops, uint32_t ts, char* out, int out_len) {
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
            &sensors, (float)board.getBattMilliVolts() / 1000.0f);
  if (strchr(out, '{')) strcpy(out, "n/a");      // requested sensor not present
  return true;
}

// Scans body for any number of space-separated "!word" tokens, building a single
// combined reply (segments joined by " | ") into out. Returns how many commands
// were recognised. Shared by the DM and channel command paths.
int MyMesh::botScanCommands(const char* body, uint8_t hops, uint32_t ts, char* out, int out_len) {
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
    if (!botCommandReply(cmd, hops, ts, seg, sizeof(seg))) continue;  // unknown

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

  uint32_t ts = getRTCClock()->getCurrentTime();
  char out[BOT_SCRATCH];
  if (botScanCommands(text, hops, ts, out, sizeof(out)) == 0) return false;  // no commands
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
  if (!(_prefs.bot_commands_enabled && _prefs.bot_channel_enabled &&
        channel_idx == _prefs.bot_channel_idx))
    return false;

  // Skip "sender_name: " prefix so commands are read from the message body only.
  const char* msg = text;
  const char* sep = strstr(text, ": ");
  if (sep) msg = sep + 2;

  uint32_t ts = getRTCClock()->getCurrentTime();
  char out[BOT_SCRATCH];
  if (botScanCommands(msg, hops, ts, out, sizeof(out)) == 0) return false;  // no commands
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
