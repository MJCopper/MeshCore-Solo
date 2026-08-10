#include <gtest/gtest.h>
#include <cstring>

#define SOLO_FEAT_CHILD_MODE 1
#define SOLO_FEAT_QUIET_TIME 1
#include "../../examples/companion_radio/solo/SoloPrefsCodec.h"

namespace {

NodePrefs emptyPrefs() {
  NodePrefs prefs;
  std::memset(&prefs, 0, sizeof(prefs));
  return prefs;
}

uint32_t fnv(const uint8_t* data, size_t len) {
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < len; i++) { h ^= data[i]; h *= 16777619UL; }
  return h;
}

void put32(uint8_t* out, uint32_t value) {
  for (int i = 0; i < 4; i++) { out[i] = (uint8_t)value; value >>= 8; }
}

} // namespace

TEST(SoloPrefsCodec, RoundTripsSoloOwnedSettings) {
  NodePrefs source = emptyPrefs();
  source.child_mode_enabled = 1;
  source.child_mode_pin_hash = 0x12345678;
  source.child_visible_pages = 0x0521;
  source.child_channels_enabled = 1;
  source.quiet_time_enabled = 1;
  source.quiet_time_start_min = 21 * 60;
  source.quiet_time_end_min = 7 * 60;

  uint8_t encoded[solo::PrefsCodec::MAX_ENCODED_SIZE];
  size_t size = solo::PrefsCodec::encode(source, encoded, sizeof(encoded));
  ASSERT_GT(size, 0u);

  NodePrefs decoded = emptyPrefs();
  ASSERT_TRUE(solo::PrefsCodec::decode(decoded, encoded, size));
  EXPECT_EQ(source.child_mode_enabled, decoded.child_mode_enabled);
  EXPECT_EQ(source.child_mode_pin_hash, decoded.child_mode_pin_hash);
  EXPECT_EQ(source.child_visible_pages, decoded.child_visible_pages);
  EXPECT_EQ(source.child_channels_enabled, decoded.child_channels_enabled);
  EXPECT_EQ(source.quiet_time_enabled, decoded.quiet_time_enabled);
  EXPECT_EQ(source.quiet_time_start_min, decoded.quiet_time_start_min);
  EXPECT_EQ(source.quiet_time_end_min, decoded.quiet_time_end_min);
}

TEST(SoloPrefsCodec, RejectsCorruptionWithoutPartiallyApplyingRecords) {
  NodePrefs source = emptyPrefs();
  source.child_mode_enabled = 1;
  source.child_mode_pin_hash = 42;
  source.quiet_time_enabled = 1;
  source.quiet_time_start_min = 600;
  source.quiet_time_end_min = 900;

  uint8_t encoded[solo::PrefsCodec::MAX_ENCODED_SIZE];
  size_t size = solo::PrefsCodec::encode(source, encoded, sizeof(encoded));
  ASSERT_GT(size, 12u);
  encoded[12] ^= 0x80;

  NodePrefs destination = emptyPrefs();
  destination.child_mode_pin_hash = 99;
  destination.quiet_time_start_min = 123;
  EXPECT_FALSE(solo::PrefsCodec::decode(destination, encoded, size));
  EXPECT_EQ(99u, destination.child_mode_pin_hash);
  EXPECT_EQ(123u, destination.quiet_time_start_min);
}

TEST(SoloPrefsCodec, RejectsInvalidTimeWithoutApplyingChildRecord) {
  NodePrefs source = emptyPrefs();
  source.child_mode_enabled = 1;
  source.child_mode_pin_hash = 42;
  source.quiet_time_start_min = 1440;

  uint8_t encoded[solo::PrefsCodec::MAX_ENCODED_SIZE];
  size_t size = solo::PrefsCodec::encode(source, encoded, sizeof(encoded));

  NodePrefs destination = emptyPrefs();
  destination.child_mode_pin_hash = 99;
  EXPECT_FALSE(solo::PrefsCodec::decode(destination, encoded, size));
  EXPECT_EQ(99u, destination.child_mode_pin_hash);
}

TEST(SoloPrefsCodec, SkipsLargerUnknownRecordsFromCompatibleVersions) {
  NodePrefs source = emptyPrefs();
  source.child_mode_enabled = 1;
  source.child_mode_pin_hash = 1234;

  uint8_t encoded[solo::PrefsCodec::MAX_ENCODED_SIZE] = {};
  size_t original_size = solo::PrefsCodec::encode(source, encoded, sizeof(encoded));
  ASSERT_GT(original_size, 0u);

  const uint16_t unknown_len = 20;
  const size_t record_size = 3 + unknown_len;
  const size_t old_checksum = original_size - 4;
  std::memmove(encoded + old_checksum + record_size, encoded + old_checksum, 4);
  encoded[old_checksum] = 99;
  encoded[old_checksum + 1] = (uint8_t)unknown_len;
  encoded[old_checksum + 2] = (uint8_t)(unknown_len >> 8);
  std::memset(encoded + old_checksum + 3, 0xA5, unknown_len);

  uint16_t payload_len = encoded[5] | ((uint16_t)encoded[6] << 8);
  payload_len += record_size;
  encoded[5] = (uint8_t)payload_len;
  encoded[6] = (uint8_t)(payload_len >> 8);
  encoded[4] = 2; // compatible record-envelope revision
  size_t expanded_size = original_size + record_size;
  put32(encoded + expanded_size - 4, fnv(encoded, expanded_size - 4));
  ASSERT_GT(expanded_size, 40u);

  NodePrefs decoded = emptyPrefs();
  ASSERT_TRUE(solo::PrefsCodec::decode(decoded, encoded, expanded_size));
  EXPECT_EQ(source.child_mode_enabled, decoded.child_mode_enabled);
  EXPECT_EQ(source.child_mode_pin_hash, decoded.child_mode_pin_hash);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
