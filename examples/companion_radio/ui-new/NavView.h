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

// own_valid : is there a usable GPS fix for the device right now
// cog_valid : is there a stable course-over-ground (UITask::currentCourse)
inline void draw(DisplayDriver& d,
                 bool own_valid, int32_t own_lat, int32_t own_lon,
                 int32_t tgt_lat, int32_t tgt_lon,
                 const char* label,
                 bool cog_valid, int cog_deg,
                 bool imperial) {
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
}

}  // namespace navview
