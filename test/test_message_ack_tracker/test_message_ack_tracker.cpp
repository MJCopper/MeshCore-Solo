#include <gtest/gtest.h>
#include "../../examples/companion_radio/solo/MessageAckTracker.h"

TEST(MessageAckTracker, AcceptsEarlierAndRepeatedAcknowledgements) {
  solo::MessageAckTracker tracker;
  tracker.record(0, 100, 2);
  tracker.record(1, 101, 2);
  tracker.record(2, 102, 3);
  uint8_t route = 0;
  EXPECT_TRUE(tracker.match(100, route));
  EXPECT_EQ(2, route);
  EXPECT_TRUE(tracker.match(100, route));
  EXPECT_TRUE(tracker.match(102, route));
  EXPECT_EQ(3, route);
  EXPECT_FALSE(tracker.match(0, route));
  EXPECT_FALSE(tracker.match(999, route));
}

TEST(MessageAckTracker, CoversWireAttemptWrapWithoutGrowingStorage) {
  solo::MessageAckTracker tracker;
  for (uint8_t i = 0; i < 5; i++) tracker.record(i, 100 + (i & 3), i < 2 ? 2 : 3);
  uint8_t route;
  for (uint32_t tag = 100; tag < 104; tag++) EXPECT_TRUE(tracker.match(tag, route));
  tracker = solo::MessageAckTracker();
  EXPECT_FALSE(tracker.match(100, route));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
