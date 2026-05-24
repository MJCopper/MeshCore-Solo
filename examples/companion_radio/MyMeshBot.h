#pragma once
// Bot logic — not part of upstream MyMesh.cpp.
// Included at the bottom of MyMesh.cpp after all class definitions.

void MyMesh::tryBotReplyDM(const ContactInfo& from, const char* text) {
  if (from.type != ADV_TYPE_CHAT) return;
  if (!(_prefs.bot_enabled && _prefs.bot_trigger[0] && _prefs.bot_reply_dm[0] &&
        millis() - _bot_last_dm_reply_ms > 10000UL))
    return;

  // Lower-case the body once, then a single strstr — avoids O(n*m) tolower per offset.
  char low[200];
  size_t n = strlen(text);
  if (n >= sizeof(low)) n = sizeof(low) - 1;
  for (size_t i = 0; i < n; i++) low[i] = (char)tolower((uint8_t)text[i]);
  low[n] = '\0';
  if (!strstr(low, _prefs.bot_trigger)) return;

  uint32_t ts = getRTCClock()->getCurrentTime();
  char expanded[200];
  expandMsg(_prefs.bot_reply_dm, expanded, sizeof(expanded),
            sensors.node_lat, sensors.node_lon,
            sensors.node_lat != 0.0 || sensors.node_lon != 0.0,
            ts, _prefs.tz_offset_hours,
            &sensors, (float)board.getBattMilliVolts() / 1000.0f);
  uint32_t expected_ack, est_timeout;
  if (sendMessage(from, ts, 0, expanded, expected_ack, est_timeout) != MSG_SEND_FAILED) {
    _bot_last_dm_reply_ms = millis();
#ifdef DISPLAY_CLASS
    if (_ui) _ui->addDMMsg(from.id.pub_key, true, expanded);
#endif
  }
}

void MyMesh::tryBotReplyChannel(uint8_t channel_idx, const char* text) {
  if (!(_prefs.bot_enabled && _prefs.bot_channel_enabled && _prefs.bot_trigger[0] && _prefs.bot_reply_ch[0] &&
        channel_idx == _prefs.bot_channel_idx &&
        millis() - _bot_last_ch_reply_ms > 10000UL))
    return;

  // Skip "sender_name: " prefix so the trigger is only matched against the message body.
  const char* msg = text;
  const char* sep = strstr(text, ": ");
  if (sep) msg = sep + 2;

  char low[200];
  size_t n = strlen(msg);
  if (n >= sizeof(low)) n = sizeof(low) - 1;
  for (size_t i = 0; i < n; i++) low[i] = (char)tolower((uint8_t)msg[i]);
  low[n] = '\0';
  if (!strstr(low, _prefs.bot_trigger)) return;

  ChannelDetails ch;
  if (!getChannel(channel_idx, ch)) return;

  uint32_t ts = getRTCClock()->getCurrentTime();
  char expanded[200];
  expandMsg(_prefs.bot_reply_ch, expanded, sizeof(expanded),
            sensors.node_lat, sensors.node_lon,
            sensors.node_lat != 0.0 || sensors.node_lon != 0.0,
            ts, _prefs.tz_offset_hours,
            &sensors, (float)board.getBattMilliVolts() / 1000.0f);
  int rlen = strlen(expanded);
  if (sendGroupMessage(ts, ch.channel, _prefs.node_name, expanded, rlen)) {
    _bot_last_ch_reply_ms = millis();
#ifdef DISPLAY_CLASS
    if (_ui) {
      char with_sender[240];  // node_name(32) + ": "(2) + expanded(200) + margin
      snprintf(with_sender, sizeof(with_sender), "%s: %s", _prefs.node_name, expanded);
      _ui->addChannelMsg(channel_idx, with_sender);
    }
#endif
  }
}
