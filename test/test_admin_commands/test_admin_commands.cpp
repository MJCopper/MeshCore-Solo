#include <gtest/gtest.h>
#include "../../examples/companion_radio/solo/AdminCommands.h"

TEST(AdminCommands, DoesNotTreatErrorAsFetchedValueOrSuccess) {
  EXPECT_EQ(solo::admin::value("Error: unsupported"), nullptr);
  EXPECT_EQ(solo::admin::value("OK"), nullptr);
  EXPECT_STREQ(solo::admin::value("> name"), "name");
  EXPECT_TRUE(solo::admin::confirmed("OK"));
  EXPECT_TRUE(solo::admin::confirmed("OK - Advert sent"));
  EXPECT_FALSE(solo::admin::confirmed("Error"));
  EXPECT_FALSE(solo::admin::confirmed("Sent"));
}

TEST(AdminCommands, RejectsMalformedOrOutOfRangeNumbers) {
  float n = 4;
  EXPECT_FALSE(solo::admin::number("Error", 0, 20, n));
  EXPECT_FALSE(solo::admin::number("nan", 0, 20, n));
  EXPECT_FALSE(solo::admin::number("10junk", 0, 20, n));
  EXPECT_FALSE(solo::admin::number("21", 0, 20, n));
  EXPECT_EQ(n, 4);
  EXPECT_TRUE(solo::admin::number("10.5", 0, 20, n));
  EXPECT_FLOAT_EQ(n, 10.5f);
}

TEST(AdminCommands, ValidatesRadioTupleAndPreservesOtherFields) {
  float freq = 0, bw = 0;
  uint8_t sf = 0, cr = 0;
  EXPECT_FALSE(solo::admin::parseRadio("915,250,99,5", freq, bw, sf, cr));
  EXPECT_FALSE(solo::admin::parseRadio("915,250,10,5junk", freq, bw, sf, cr));
  ASSERT_TRUE(solo::admin::parseRadio("915,62.500,8,5", freq, bw, sf, cr));
  char value[60], cmd[80];
  EXPECT_TRUE(solo::admin::formatRadio(value, sizeof(value), freq + 1, bw, sf, cr));
  EXPECT_TRUE(solo::admin::formatCommand(cmd, sizeof(cmd), "set radio", value));
  EXPECT_STREQ(cmd, "set radio 916.000,62.500,8,5");
  EXPECT_FALSE(solo::admin::formatCommand(cmd, 6, "set name", "too long"));
}

TEST(AdminCommands, AdvertStepsRespectBaselineDisabledAndMinimumIntervals) {
  for (const auto& field : solo::admin::FIELDS) {
    if (!strcmp(field.get, "get advert.interval")) {
      EXPECT_EQ(solo::admin::stepNumber(field, 0, 1), 60);
      EXPECT_EQ(solo::admin::stepNumber(field, 60, -1), 0);
      EXPECT_EQ(solo::admin::stepNumber(field, 60, 1), 62);
    } else if (!strcmp(field.get, "get flood.advert.interval")) {
      EXPECT_EQ(solo::admin::stepNumber(field, 0, 1), 3);
      EXPECT_EQ(solo::admin::stepNumber(field, 3, -1), 0);
    }
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
