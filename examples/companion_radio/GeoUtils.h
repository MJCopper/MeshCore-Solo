#pragma once
// Shared geographic helpers used by the navigation features (Nearby Nodes,
// Waypoints, trail course-over-ground). Coordinates are fixed-point degrees
// scaled by 1e6 (the same representation used by the GPS provider and the
// contact/trail records). All functions are pure and header-inline.

#include <Arduino.h>
#include <math.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace geo {

// Great-circle distance between two 1e6-scaled lat/lon points, in kilometres.
static inline float haversineKm(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
  static const float DEG2RAD = (float)M_PI / 180.0f;
  float la1 = lat1 * (1e-6f * DEG2RAD);
  float la2 = lat2 * (1e-6f * DEG2RAD);
  float dla = (lat2 - lat1) * (1e-6f * DEG2RAD);
  float dlo = (lon2 - lon1) * (1e-6f * DEG2RAD);
  float a   = sinf(dla/2)*sinf(dla/2) + cosf(la1)*cosf(la2)*sinf(dlo/2)*sinf(dlo/2);
  return 6371.0f * 2.0f * asinf(sqrtf(a));
}

// Initial bearing from point 1 to point 2, degrees 0..359 (0 = north).
static inline int bearingDeg(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
  static const float DEG2RAD = (float)M_PI / 180.0f;
  float la1 = lat1 * (1e-6f * DEG2RAD);
  float la2 = lat2 * (1e-6f * DEG2RAD);
  float dlo = (lon2 - lon1) * (1e-6f * DEG2RAD);
  float b   = atan2f(sinf(dlo)*cosf(la2),
                     cosf(la1)*sinf(la2) - sinf(la1)*cosf(la2)*cosf(dlo)) * (180.0f / (float)M_PI);
  if (b < 0.0f) b += 360.0f;
  return (int)(b + 0.5f) % 360;
}

// 8-point cardinal label for a bearing in degrees.
static inline const char* bearingCardinal(int deg) {
  static const char* dirs[] = { "N","NE","E","SE","S","SW","W","NW" };
  return dirs[((deg + 22) % 360) / 45];
}

// Human distance string: "850m" / "2.3km" / "140km".
static inline void fmtDist(char* buf, int n, float km) {
  if      (km < 1.0f)   snprintf(buf, n, "%dm",   (int)(km * 1000 + 0.5f));
  else if (km < 100.0f) snprintf(buf, n, "%.1fkm", km);
  else                  snprintf(buf, n, "%dkm",  (int)(km + 0.5f));
}

}  // namespace geo
