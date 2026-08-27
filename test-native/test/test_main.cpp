// Host unit tests. See README.md for how to run them.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MeshRegion.h"
#include "MeshTypes.h"

using namespace libmeshtastic_leaf;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name)                                                         \
  do {                                                                         \
    printf("  %s... ", #name);                                                 \
    tests_run++;                                                               \
    test_##name();                                                             \
    tests_passed++;                                                            \
    printf("PASS\n");                                                          \
  } while (0)

#define ASSERT(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", #cond,          \
             __FILE__, __LINE__);                                              \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_STREQ(a, b) ASSERT(strcmp((a), (b)) == 0)

TEST(constants) {
  ASSERT_EQ(MAX_LORA_PAYLOAD_LEN, 255);
  ASSERT_EQ(MESHTASTIC_HEADER_LENGTH, 16);
  ASSERT_EQ(MESHTASTIC_PKC_OVERHEAD, 12);
  ASSERT_EQ(MAX_ENCRYPTED_PAYLOAD, 239);
  ASSERT_EQ(BROADCAST_ADDR, 0xFFFFFFFF);
  ASSERT_EQ(MESHTASTIC_SYNC_WORD, 0x2B);
  ASSERT_EQ(AES_BLOCK_SIZE, 16);
  ASSERT_EQ(CURVE25519_KEY_SIZE, 32);
}

TEST(default_psk) {
  ASSERT_EQ(DEFAULT_PSK[0], 0xd4);
  ASSERT_EQ(DEFAULT_PSK[1], 0xf1);
  ASSERT_EQ(DEFAULT_PSK[15], 0x01);
}

TEST(crypto_key_default) {
  CryptoKey key;
  ASSERT_FALSE(key.isValid());
  ASSERT_FALSE(key.isAES128());
  ASSERT_FALSE(key.isAES256());
  ASSERT_EQ(key.length, 0);
}

TEST(crypto_key_aes128) {
  CryptoKey key;
  key.length = 16;
  ASSERT_TRUE(key.isValid());
  ASSERT_TRUE(key.isAES128());
  ASSERT_FALSE(key.isAES256());
}

TEST(crypto_key_aes256) {
  CryptoKey key;
  key.length = 32;
  ASSERT_TRUE(key.isValid());
  ASSERT_FALSE(key.isAES128());
  ASSERT_TRUE(key.isAES256());
}

TEST(packet_header_size) { ASSERT_EQ(sizeof(PacketHeader), 16); }

TEST(packet_header_hop_limit) {
  PacketHeader header = {};
  header.flags = 0;

  header.setHopLimit(3);
  ASSERT_EQ(header.getHopLimit(), 3);

  header.setHopLimit(7);
  ASSERT_EQ(header.getHopLimit(), 7);

  header.setHopLimit(0);
  ASSERT_EQ(header.getHopLimit(), 0);
}

TEST(packet_header_hop_start) {
  PacketHeader header = {};
  header.flags = 0;

  header.setHopStart(5);
  ASSERT_EQ(header.getHopStart(), 5);

  header.setHopStart(7);
  ASSERT_EQ(header.getHopStart(), 7);

  header.setHopStart(0);
  ASSERT_EQ(header.getHopStart(), 0);
}

TEST(packet_header_want_ack) {
  PacketHeader header = {};
  header.flags = 0;

  ASSERT_FALSE(header.wantAck());

  header.setWantAck(true);
  ASSERT_TRUE(header.wantAck());

  header.setWantAck(false);
  ASSERT_FALSE(header.wantAck());
}

TEST(packet_header_combined_flags) {
  PacketHeader header = {};
  header.flags = 0;

  header.setHopLimit(5);
  header.setHopStart(6);
  header.setWantAck(true);

  ASSERT_EQ(header.getHopLimit(), 5);
  ASSERT_EQ(header.getHopStart(), 6);
  ASSERT_TRUE(header.wantAck());
}

TEST(packet_header_is_pki) {
  PacketHeader header = {};

  header.channel = 0;
  header.to = 0x12345678;
  ASSERT_TRUE(header.isPKI());

  header.channel = 1;
  header.to = 0x12345678;
  ASSERT_FALSE(header.isPKI());

  header.channel = 0;
  header.to = BROADCAST_ADDR;
  ASSERT_FALSE(header.isPKI());
}

TEST(mesh_packet_default) {
  MeshPacket packet;
  ASSERT_EQ(packet.portNum, meshtastic_PortNum_UNKNOWN_APP);
  ASSERT_EQ(packet.payloadLen, 0);
  ASSERT_EQ(packet.rxRssi, 0);
  ASSERT_FALSE(packet.isPKI);
}

TEST(region_us) {
  const RegionInfo *region = MeshRegion::getRegion(REGION_US);
  ASSERT_TRUE(region != nullptr);
  ASSERT_EQ(region->code, REGION_US);
  ASSERT_STREQ(region->name, "US");
  ASSERT_EQ(region->powerLimit, 30);
  ASSERT_TRUE(region->freqStart >= 902.0f && region->freqStart <= 902.1f);
  ASSERT_TRUE(region->freqEnd >= 927.9f && region->freqEnd <= 928.1f);
  ASSERT_FALSE(region->wideLora);
}

TEST(region_eu_868) {
  const RegionInfo *region = MeshRegion::getRegion(REGION_EU_868);
  ASSERT_TRUE(region != nullptr);
  ASSERT_EQ(region->code, REGION_EU_868);
  ASSERT_STREQ(region->name, "EU_868");
  ASSERT_EQ(region->powerLimit, 27);
  ASSERT_EQ(region->dutyCycle, 10);
}

TEST(region_lora24) {
  const RegionInfo *region = MeshRegion::getRegion(REGION_LORA_24);
  ASSERT_TRUE(region != nullptr);
  ASSERT_TRUE(region->wideLora);
  ASSERT_TRUE(region->freqStart >= 2399.0f);
}

TEST(region_name_lookup) {
  ASSERT_STREQ(MeshRegion::getRegionName(REGION_US), "US");
  ASSERT_STREQ(MeshRegion::getRegionName(REGION_EU_868), "EU_868");
  ASSERT_STREQ(MeshRegion::getRegionName(REGION_JP), "JP");
  ASSERT_STREQ(MeshRegion::getRegionName(REGION_ANZ), "ANZ");
}

TEST(region_default_frequency) {
  float freq = MeshRegion::getDefaultFrequency(REGION_US);
  ASSERT_TRUE(freq > 902.0f && freq < 903.0f);

  float freq_eu = MeshRegion::getDefaultFrequency(REGION_EU_868);
  ASSERT_TRUE(freq_eu > 869.0f && freq_eu < 870.0f);
}

TEST(region_power_limit) {
  ASSERT_EQ(MeshRegion::getPowerLimit(REGION_US), 30);
  ASSERT_EQ(MeshRegion::getPowerLimit(REGION_EU_868), 27);
  ASSERT_EQ(MeshRegion::getPowerLimit(REGION_JP), 13);
}

TEST(region_is_wide_lora) {
  ASSERT_FALSE(MeshRegion::isWideLoRa(REGION_US));
  ASSERT_FALSE(MeshRegion::isWideLoRa(REGION_EU_868));
  ASSERT_TRUE(MeshRegion::isWideLoRa(REGION_LORA_24));
}

TEST(region_all_regions) {
  size_t count = 0;
  const RegionInfo *regions = MeshRegion::getAllRegions(count);
  ASSERT_TRUE(regions != nullptr);
  ASSERT_TRUE(count > 20);
}

TEST(modem_preset_long_fast) {
  ModemParams params = MeshRegion::getModemParams(PRESET_LONG_FAST, false);
  ASSERT_EQ(params.sf, 11);
  ASSERT_TRUE(params.bw >= 249.0f && params.bw <= 251.0f);
  ASSERT_EQ(params.cr, 5);
}

TEST(modem_preset_short_fast) {
  ModemParams params = MeshRegion::getModemParams(PRESET_SHORT_FAST, false);
  ASSERT_EQ(params.sf, 7);
  ASSERT_TRUE(params.bw >= 249.0f && params.bw <= 251.0f);
  ASSERT_EQ(params.cr, 5);
}

TEST(modem_preset_very_long_slow) {
  ModemParams params = MeshRegion::getModemParams(PRESET_VERY_LONG_SLOW, false);
  ASSERT_EQ(params.sf, 12);
  ASSERT_TRUE(params.bw >= 62.0f && params.bw <= 63.0f);
  ASSERT_EQ(params.cr, 8);
}

TEST(modem_preset_wide_lora) {
  ModemParams params_normal =
      MeshRegion::getModemParams(PRESET_LONG_FAST, false);
  ModemParams params_wide = MeshRegion::getModemParams(PRESET_LONG_FAST, true);

  ASSERT_TRUE(params_wide.bw > params_normal.bw);
  ASSERT_TRUE(params_wide.bw > 800.0f);
}

TEST(preset_name) {
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_LONG_FAST), "Long Fast");
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_SHORT_FAST), "Short Fast");
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_VERY_LONG_SLOW),
               "Very Long Slow");
}

TEST(region_channel_calculation) {
  const RegionInfo *region = MeshRegion::getRegion(REGION_US);
  ASSERT_TRUE(region != nullptr);

  float bw = 250.0f;
  float freq0 = region->getChannelFrequency(0, bw);
  float freq1 = region->getChannelFrequency(1, bw);

  ASSERT_TRUE(freq0 > 902.0f && freq0 < 903.0f);

  // ponytail: encodes the current slot maths, which ignores the region
  // profile's spacing and padding. Fix together with getChannelFrequency().
  float spacing = freq1 - freq0;
  ASSERT_TRUE(spacing >= 0.24f && spacing <= 0.26f);
}

TEST(flag_masks) {
  ASSERT_EQ(PACKET_FLAGS_HOP_LIMIT_MASK, 0x07);
  ASSERT_EQ(PACKET_FLAGS_WANT_ACK_MASK, 0x08);
  ASSERT_EQ(PACKET_FLAGS_VIA_MQTT_MASK, 0x10);
  ASSERT_EQ(PACKET_FLAGS_HOP_START_MASK, 0xE0);
  ASSERT_EQ(PACKET_FLAGS_HOP_START_SHIFT, 5);
}

int main() {
  printf("libmeshtastic_leaf Unit Tests\n");
  printf("=========================\n\n");

  printf("MeshTypes Tests:\n");
  RUN_TEST(constants);
  RUN_TEST(default_psk);
  RUN_TEST(crypto_key_default);
  RUN_TEST(crypto_key_aes128);
  RUN_TEST(crypto_key_aes256);
  RUN_TEST(packet_header_size);
  RUN_TEST(packet_header_hop_limit);
  RUN_TEST(packet_header_hop_start);
  RUN_TEST(packet_header_want_ack);
  RUN_TEST(packet_header_combined_flags);
  RUN_TEST(packet_header_is_pki);
  RUN_TEST(mesh_packet_default);
  RUN_TEST(flag_masks);

  printf("\nMeshRegion Tests:\n");
  RUN_TEST(region_us);
  RUN_TEST(region_eu_868);
  RUN_TEST(region_lora24);
  RUN_TEST(region_name_lookup);
  RUN_TEST(region_default_frequency);
  RUN_TEST(region_power_limit);
  RUN_TEST(region_is_wide_lora);
  RUN_TEST(region_all_regions);
  RUN_TEST(modem_preset_long_fast);
  RUN_TEST(modem_preset_short_fast);
  RUN_TEST(modem_preset_very_long_slow);
  RUN_TEST(modem_preset_wide_lora);
  RUN_TEST(preset_name);
  RUN_TEST(region_channel_calculation);

  printf("\n=========================\n");
  printf("Tests run: %d\n", tests_run);
  printf("Tests passed: %d\n", tests_passed);
  printf("Result: %s\n", tests_passed == tests_run ? "ALL PASSED" : "FAILED");

  return tests_passed == tests_run ? 0 : 1;
}
