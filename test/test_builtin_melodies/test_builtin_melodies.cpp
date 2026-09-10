#include <gtest/gtest.h>
#include <cstring>

#include "../../examples/companion_radio/solo/BuiltinMelodies.h"
#include "../../examples/companion_radio/solo/NotificationPreferences.h"
#include "../../examples/companion_radio/solo/ConfigMaintenance.h"

TEST(BuiltinMelodies, ProvidesEightNamedShortMelodies) {
  for (uint8_t i = 0; i < solo::BuiltinMelodies::CUSTOM1; i++) {
    const char* name = solo::BuiltinMelodies::label(i);
    const char* melody = solo::BuiltinMelodies::melody(i);
    ASSERT_NE(name, nullptr);
    ASSERT_NE(melody, nullptr);
    EXPECT_GT(std::strlen(name), 0U);
    const char* notes = std::strrchr(melody, ':');
    ASSERT_NE(notes, nullptr);
    int count = 1;
    for (const char* p = notes + 1; *p; p++) if (*p == ',') count++;
    EXPECT_GE(count, 2);
    EXPECT_LE(count, 6);
  }
  EXPECT_EQ(solo::BuiltinMelodies::melody(solo::BuiltinMelodies::CUSTOM1), nullptr);
  EXPECT_EQ(solo::BuiltinMelodies::melody(solo::BuiltinMelodies::NONE), nullptr);
}

TEST(BuiltinMelodies, ResolvesGlobalAndExplicitOverridesForPreview) {
  EXPECT_EQ(solo::BuiltinMelodies::resolveOverride(0, solo::BuiltinMelodies::CHIME),
            solo::BuiltinMelodies::CHIME);
  EXPECT_EQ(solo::BuiltinMelodies::resolveOverride(
                solo::BuiltinMelodies::NONE + 1, solo::BuiltinMelodies::MESSAGE),
            solo::BuiltinMelodies::NONE);
}

TEST(BuiltinMelodies, RetainsTheTwoExistingDefaults) {
  EXPECT_STREQ(solo::BuiltinMelodies::melody(solo::BuiltinMelodies::MESSAGE),
               "MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
  EXPECT_STREQ(solo::BuiltinMelodies::melody(solo::BuiltinMelodies::KERPLOP),
               "kerplop:d=16,o=6,b=120:32g#,32c#");
}

TEST(BuiltinMelodies, CheerOrbitAndAlertHaveDistinctLengths) {
  static const uint8_t selections[] = {
    solo::BuiltinMelodies::CHEER,
    solo::BuiltinMelodies::ORBIT,
    solo::BuiltinMelodies::ALERT
  };
  const int expected[] = { 6, 5, 2 };
  for (int i = 0; i < 3; i++) {
    const char* notes = std::strrchr(solo::BuiltinMelodies::melody(selections[i]), ':');
    ASSERT_NE(notes, nullptr);
    int count = 1;
    for (const char* p = notes + 1; *p; p++) if (*p == ',') count++;
    EXPECT_EQ(count, expected[i]);
  }
}

TEST(BuiltinMelodies, MigratesLegacyGlobalSelectionsByContext) {
  EXPECT_EQ(solo::BuiltinMelodies::migrateLegacyGlobal(0, false), solo::BuiltinMelodies::MESSAGE);
  EXPECT_EQ(solo::BuiltinMelodies::migrateLegacyGlobal(0, true), solo::BuiltinMelodies::KERPLOP);
  EXPECT_EQ(solo::BuiltinMelodies::migrateLegacyGlobal(1, false), solo::BuiltinMelodies::CUSTOM1);
  EXPECT_EQ(solo::BuiltinMelodies::migrateLegacyGlobal(2, true), solo::BuiltinMelodies::CUSTOM2);
  EXPECT_EQ(solo::BuiltinMelodies::migrateLegacyGlobal(3, false), solo::BuiltinMelodies::NONE);
}

TEST(BuiltinMelodies, StoresAllChannelAndDirectMessageOverrides) {
  NodePrefs prefs{};
  for (uint8_t selection = 0; selection < solo::BuiltinMelodies::COUNT; selection++) {
    uint8_t stored = selection + 1;
    solo::NotificationPreferences::setChannelMelody(&prefs, selection, stored);
    EXPECT_EQ(solo::NotificationPreferences::channelMelody(&prefs, selection), stored);
  }
  uint8_t key[4] = { 1, 2, 3, 4 };
  solo::NotificationPreferences::setDmMelody(&prefs, key, solo::BuiltinMelodies::ALERT + 1);
  EXPECT_EQ(solo::NotificationPreferences::dmMelody(&prefs, key),
            solo::BuiltinMelodies::ALERT + 1);
  solo::NotificationPreferences::setChannelMelody(&prefs, 1, 0);
  EXPECT_EQ(solo::NotificationPreferences::channelMelody(&prefs, 1), 0);
  EXPECT_NE(solo::NotificationPreferences::channelMelody(&prefs, 0), 0);
  EXPECT_NE(solo::NotificationPreferences::channelMelody(&prefs, 2), 0);
}

TEST(BuiltinMelodies, FullDirectMessageTableRejectsInsteadOfEvicting) {
  NodePrefs prefs{};
  uint8_t key[4] = {};
  for (uint8_t i = 0; i < NodePrefs::DM_MELODY_TABLE_MAX; i++) {
    key[0] = i + 1;
    ASSERT_TRUE(solo::NotificationPreferences::setDmMelody(
        &prefs, key, solo::BuiltinMelodies::CHIME + 1));
  }
  uint8_t overflow[4] = { 99, 1, 2, 3 };
  EXPECT_FALSE(solo::NotificationPreferences::setDmMelody(
      &prefs, overflow, solo::BuiltinMelodies::ALERT + 1));
  uint8_t first[4] = { 1, 0, 0, 0 };
  EXPECT_EQ(solo::NotificationPreferences::dmMelody(&prefs, first),
            solo::BuiltinMelodies::CHIME + 1);
  EXPECT_EQ(solo::NotificationPreferences::dmMelody(&prefs, overflow), 0);
}

TEST(BuiltinMelodies, MigratesACompleteLegacyPreferenceState) {
  NodePrefs prefs{};
  prefs.notif_melody_dm = 1;
  prefs.notif_melody_ch = 0;
  prefs.notif_melody_ad = 3;
  prefs.dm_melody[0].prefix[0] = 42;
  prefs.dm_melody[0].slot = 2;
  prefs.ch_notif_melody_set = (1ULL << 3) | (1ULL << 4);
  prefs.ch_notif_melody_2 = 1ULL << 4;

  EXPECT_TRUE(solo::ConfigMaintenance::migrateMelodySchema(prefs, 0xC0DE0029));
  EXPECT_EQ(prefs.notif_melody_dm, solo::BuiltinMelodies::CUSTOM1);
  EXPECT_EQ(prefs.notif_melody_ch, solo::BuiltinMelodies::KERPLOP);
  EXPECT_EQ(prefs.notif_melody_ad, solo::BuiltinMelodies::NONE);
  EXPECT_EQ(prefs.dm_melody[0].slot, solo::BuiltinMelodies::CUSTOM2 + 1);
  EXPECT_EQ(solo::NotificationPreferences::channelMelody(&prefs, 3),
            solo::BuiltinMelodies::CUSTOM1 + 1);
  EXPECT_EQ(solo::NotificationPreferences::channelMelody(&prefs, 4),
            solo::BuiltinMelodies::CUSTOM2 + 1);
  EXPECT_EQ(prefs.ch_notif_melody_set, 0U);
  EXPECT_EQ(prefs.ch_notif_melody_2, 0U);
  EXPECT_FALSE(solo::ConfigMaintenance::migrateMelodySchema(prefs, 0xC0DE002A));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
