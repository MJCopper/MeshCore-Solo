#include <gtest/gtest.h>

#include "../../examples/companion_radio/solo/TimeDeadline.h"

TEST(TimeDeadline, HandlesNormalAndWrappedDeadlines) {
  EXPECT_TRUE(solo::TimeDeadline::due(100, 0));
  EXPECT_FALSE(solo::TimeDeadline::due(100, 200));
  EXPECT_TRUE(solo::TimeDeadline::due(200, 200));
  EXPECT_TRUE(solo::TimeDeadline::due(5, 0xFFFFFFF0U));
  EXPECT_FALSE(solo::TimeDeadline::due(0xFFFFFFF0U, 5));
}

TEST(TimeDeadline, HandlesActiveAndOrderingAcrossWrap) {
  EXPECT_FALSE(solo::TimeDeadline::active(100, 0));
  EXPECT_TRUE(solo::TimeDeadline::active(0xFFFFFFF0U, 5));
  EXPECT_FALSE(solo::TimeDeadline::active(5, 0xFFFFFFF0U));
  EXPECT_TRUE(solo::TimeDeadline::after(5, 0xFFFFFFF0U));
  EXPECT_FALSE(solo::TimeDeadline::after(0xFFFFFFF0U, 5));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
