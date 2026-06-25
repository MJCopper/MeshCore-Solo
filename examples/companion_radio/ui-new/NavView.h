#pragma once
// Shared "navigate to a point" view. The target is just a (lat, lon, label)
// triple, so the same screen serves Waypoints, trail Backtrack and Nearby
// node navigation. No magnetometer: we show two *absolute* bearings — the
// target's bearing and the user's course over ground — and let the user
// compare them ("target 145°, I'm heading 090° → bear right").
//
// Pure render helper; the caller supplies its own GPS fix + COG (from UITask).

#include <helpers/ui/DisplayDriver.h>
#include "../GeoUtils.h"

namespace navview {

// Closing-speed / ETA estimator. The caller keeps one instance per navigation
// session and passes it to draw(), which feeds it the live target distance each
// frame. Speed is the rate the gap closes (smoothed), so it works for both a
// fixed waypoint and a moving live contact; ETA = remaining distance / closing
// speed, shown only while actually approaching.
struct EtaTracker {
  float    prev_km     = -1.0f;
  uint32_t prev_ms     = 0;
  float    closing_kmh = 0.0f;   // > 0 = gap shrinking (approaching)
  bool     have        = false;
  void reset() { prev_km = -1.0f; have = false; closing_kmh = 0.0f; }
  void update(float dist_km, uint32_t now_ms) {
    if (prev_km >= 0.0f && now_ms > prev_ms) {
      float dt_h = (now_ms - prev_ms) / 3600000.0f;
      if (dt_h > 0.0f) {
        float inst = (prev_km - dist_km) / dt_h;            // +approaching, -receding
        closing_kmh = have ? (closing_kmh * 0.6f + inst * 0.4f) : inst;  // light EMA
        have = true;
      }
    }
    prev_km = dist_km;
    prev_ms = now_ms;
  }
};

// own_valid : is there a usable GPS fix for the device right now
// cog_valid : is there a stable course-over-ground (UITask::currentCourse)
// eta       : optional closing-speed/ETA tracker; when supplied a third line is
//             shown with the approach speed and estimated time of arrival
inline void draw(DisplayDriver& d,
                 bool own_valid, int32_t own_lat, int32_t own_lon,
                 int32_t tgt_lat, int32_t tgt_lon,
                 const char* label,
                 bool cog_valid, int cog_deg,
                 bool imperial,
                 EtaTracker* eta = nullptr) {
  const int cx  = d.width() / 2;
  const int hdr = d.headerH();

  // Title bar: label, inverted (matches the Nearby/discover detail style).
  d.drawInvertedHeader((label && label[0]) ? label : "Navigate");

  if (!own_valid) {
    d.drawTextCentered(cx, hdr + (d.height() - hdr) / 2 - d.getLineHeight(), "No GPS fix");
    return;
  }

  int   to_deg = geo::bearingDeg(own_lat, own_lon, tgt_lat, tgt_lon);
  float dist_km = geo::haversineKm(own_lat, own_lon, tgt_lat, tgt_lon);
  char dist[12];
  geo::fmtDist(dist, sizeof(dist), dist_km, imperial);

  const int step = d.lineStep();
  int y = d.listStart();

  // Distance — emphasised at size 2.
  d.setTextSize(2);
  d.drawTextCentered(cx, y, dist);
  y += d.getLineHeight() + 3;
  d.setTextSize(1);

  char line[20];
  snprintf(line, sizeof(line), "To:  %d %s", to_deg, geo::bearingCardinal(to_deg));
  d.drawTextLeftAlign(2, y, line); y += step;

  if (cog_valid) snprintf(line, sizeof(line), "Hdg: %d %s", cog_deg, geo::bearingCardinal(cog_deg));
  else           snprintf(line, sizeof(line), "Hdg: --");
  d.drawTextLeftAlign(2, y, line); y += step;

  // Optional ETA / closing-speed line. Approaching only — a receding or
  // stationary target shows "ETA: --" rather than a misleading time.
  if (eta) {
    eta->update(dist_km, millis());
    if (eta->have && eta->closing_kmh > 0.3f) {
      int secs = (int)(dist_km / eta->closing_kmh * 3600.0f);
      float spd = imperial ? eta->closing_kmh * 0.621371f : eta->closing_kmh;
      char eta_s[12];
      if (secs < 3600) snprintf(eta_s, sizeof(eta_s), "%dm", (secs + 59) / 60);
      else             snprintf(eta_s, sizeof(eta_s), "%dh%02dm", secs / 3600, (secs % 3600) / 60);
      snprintf(line, sizeof(line), "ETA: %s @%d%s", eta_s, (int)(spd + 0.5f), imperial ? "mph" : "kmh");
    } else {
      snprintf(line, sizeof(line), "ETA: --");
    }
    d.drawTextLeftAlign(2, y, line); y += step;
  }
}

}  // namespace navview
