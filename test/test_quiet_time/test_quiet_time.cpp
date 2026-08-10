#include <gtest/gtest.h>

#include "../../examples/companion_radio/ui-new/QuietTimePolicy.h"

TEST(QuietTime, HandlesSameDayIntervalBoundaries) {
  EXPECT_FALSE(quiettime::intervalActive(8 * 60 + 59, 9 * 60, 17 * 60));
  EXPECT_TRUE(quiettime::intervalActive(9 * 60, 9 * 60, 17 * 60));
  EXPECT_TRUE(quiettime::intervalActive(16 * 60 + 59, 9 * 60, 17 * 60));
  EXPECT_FALSE(quiettime::intervalActive(17 * 60, 9 * 60, 17 * 60));
}

TEST(QuietTime, HandlesIntervalAcrossMidnight) {
  EXPECT_TRUE(quiettime::intervalActive(23 * 60, 21 * 60, 7 * 60));
  EXPECT_TRUE(quiettime::intervalActive(6 * 60 + 59, 21 * 60, 7 * 60));
  EXPECT_FALSE(quiettime::intervalActive(12 * 60, 21 * 60, 7 * 60));
}

TEST(QuietTime, RejectsDisabledAndInvalidIntervals) {
  EXPECT_FALSE(quiettime::intervalActive(12 * 60, 7 * 60, 7 * 60));
  EXPECT_FALSE(quiettime::intervalActive(1440, 9 * 60, 17 * 60));
  EXPECT_FALSE(quiettime::intervalActive(12 * 60, 1440, 17 * 60));
}
