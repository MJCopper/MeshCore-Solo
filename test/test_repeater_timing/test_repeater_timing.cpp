#include <gtest/gtest.h>

#include "../../examples/companion_radio/solo/RepeaterTiming.h"

TEST(RepeaterTiming, MatchesStandardRepeaterDefaults) {
  EXPECT_FLOAT_EQ(solo::RepeaterTiming::DEFAULT_RX_DELAY_BASE, 10.0f);
  EXPECT_FLOAT_EQ(solo::RepeaterTiming::DEFAULT_FLOOD_TX_FACTOR, 0.5f);
  EXPECT_FLOAT_EQ(solo::RepeaterTiming::DEFAULT_DIRECT_TX_FACTOR, 0.3f);
  EXPECT_EQ(solo::RepeaterTiming::DEFAULT_YIELD_BOOST, 1);
  EXPECT_EQ(solo::RepeaterTiming::DEFAULT_SUPPRESS_DUP, 1);
}

TEST(RepeaterTiming, ScalesAirtimeIntoDelayWindow) {
  EXPECT_EQ(solo::RepeaterTiming::delayWindow(1000, 0.5f), 500u);
  EXPECT_EQ(solo::RepeaterTiming::delayWindow(1000, 0.3f), 300u);
}

TEST(RepeaterTiming, RejectsInvalidPersistedFactors) {
  EXPECT_FLOAT_EQ(solo::RepeaterTiming::validOrDefault(NAN, 2.0f, 0.5f), 0.5f);
  EXPECT_FLOAT_EQ(solo::RepeaterTiming::validOrDefault(-0.1f, 2.0f, 0.5f), 0.5f);
  EXPECT_FLOAT_EQ(solo::RepeaterTiming::validOrDefault(2.1f, 2.0f, 0.5f), 0.5f);
  EXPECT_FLOAT_EQ(solo::RepeaterTiming::validOrDefault(1.2f, 2.0f, 0.5f), 1.2f);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
