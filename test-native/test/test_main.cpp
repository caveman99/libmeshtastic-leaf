// Host unit tests. See README.md for how to run them.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MeshAirtime.h"
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
  // The default is the hashed slot for the region's default preset, not the
  // bottom of the band.
  float freq = MeshRegion::getDefaultFrequency(REGION_US);
  ASSERT_TRUE(freq > 906.8749f && freq < 906.8751f);

  const RegionInfo *eu = MeshRegion::getRegion(REGION_EU_868);
  float freq_eu = MeshRegion::getDefaultFrequency(REGION_EU_868);
  ASSERT_TRUE(freq_eu >= eu->freqStart && freq_eu <= eu->freqEnd);
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

TEST(modem_preset_long_slow) {
  ModemParams params = MeshRegion::getModemParams(PRESET_LONG_SLOW, false);
  ASSERT_EQ(params.sf, 12);
  ASSERT_TRUE(params.bw >= 124.0f && params.bw <= 126.0f);
  ASSERT_EQ(params.cr, 8);
}

TEST(modem_preset_narrow_and_tiny) {
  ModemParams narrow = MeshRegion::getModemParams(PRESET_NARROW_SLOW, false);
  ASSERT_EQ(narrow.sf, 8);
  ASSERT_TRUE(narrow.bw > 62.0f && narrow.bw < 63.0f);
  ASSERT_EQ(narrow.cr, 6);

  ModemParams tiny = MeshRegion::getModemParams(PRESET_TINY_FAST, false);
  ASSERT_EQ(tiny.sf, 7);
  ASSERT_TRUE(tiny.bw > 15.5f && tiny.bw < 15.7f);
  ASSERT_EQ(tiny.cr, 5);
}

TEST(modem_preset_wide_lora) {
  ModemParams params_normal =
      MeshRegion::getModemParams(PRESET_LONG_FAST, false);
  ModemParams params_wide = MeshRegion::getModemParams(PRESET_LONG_FAST, true);

  ASSERT_TRUE(params_wide.bw > params_normal.bw);
  ASSERT_TRUE(params_wide.bw > 800.0f);
}

TEST(preset_name) {
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_LONG_FAST), "LongFast");
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_SHORT_FAST), "ShortFast");
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_TINY_SLOW), "TinySlow");
}

TEST(flag_masks) {
  ASSERT_EQ(PACKET_FLAGS_HOP_LIMIT_MASK, 0x07);
  ASSERT_EQ(PACKET_FLAGS_WANT_ACK_MASK, 0x08);
  ASSERT_EQ(PACKET_FLAGS_VIA_MQTT_MASK, 0x10);
  ASSERT_EQ(PACKET_FLAGS_HOP_START_MASK, 0xE0);
  ASSERT_EQ(PACKET_FLAGS_HOP_START_SHIFT, 5);
}

TEST(airtime_starts_empty) {
  MeshAirtime air;
  air.reset(0);
  ASSERT_EQ(air.txMsecLastHour(), 0u);
  ASSERT_TRUE(air.txUtilizationPercent() == 0.0f);
  ASSERT_TRUE(air.channelUtilizationPercent() == 0.0f);
}

TEST(airtime_tx_counts_towards_duty_cycle) {
  MeshAirtime air;
  air.reset(0);
  // 1% of an hour is 36 seconds.
  air.logTx(0, 36000);
  ASSERT_EQ(air.txMsecLastHour(), 36000u);
  float pct = air.txUtilizationPercent();
  ASSERT_TRUE(pct > 0.99f && pct < 1.01f);
}

TEST(airtime_rx_excluded_from_duty_cycle) {
  MeshAirtime air;
  air.reset(0);
  air.logRx(0, 36000);
  ASSERT_EQ(air.txMsecLastHour(), 0u);
  ASSERT_TRUE(air.txUtilizationPercent() == 0.0f);
  // but it does occupy the channel
  ASSERT_TRUE(air.channelUtilizationPercent() > 0.0f);
}

TEST(airtime_tx_expires_after_an_hour) {
  MeshAirtime air;
  air.reset(0);
  air.logTx(0, 36000);
  air.advance(59UL * 60UL * 1000UL);
  ASSERT_EQ(air.txMsecLastHour(), 36000u);
  air.advance(61UL * 60UL * 1000UL);
  ASSERT_EQ(air.txMsecLastHour(), 0u);
}

TEST(airtime_channel_window_is_one_minute) {
  MeshAirtime air;
  air.reset(0);
  air.logRx(0, 6000); // 10% of a minute
  float pct = air.channelUtilizationPercent();
  ASSERT_TRUE(pct > 9.9f && pct < 10.1f);
  air.advance(61UL * 1000UL);
  ASSERT_TRUE(air.channelUtilizationPercent() == 0.0f);
}

TEST(airtime_survives_a_long_idle_gap) {
  MeshAirtime air;
  air.reset(0);
  air.logTx(0, 36000);
  // more than a full hour of silence must not wrap the ring back onto itself
  air.advance(5UL * 60UL * 60UL * 1000UL);
  ASSERT_EQ(air.txMsecLastHour(), 0u);
}

// Slot time drives the contention window. Mirrors computeSlotTimeMsec():
// max(2.25, NUM_SYM_CAD + 0.5) * 2^SF / BW + 7.6 ms.
static uint32_t slotTimeFor(ModemPreset preset) {
  ModemParams p = MeshRegion::getModemParams(preset, false);
  float symbolMsec = (float)(1UL << p.sf) / p.bw;
  return (uint32_t)(2.5f * symbolMsec + 7.6f);
}

TEST(slot_time_long_fast) {
  // SF11 at 250 kHz: symbol is 8.192 ms, so the slot is about 28 ms.
  uint32_t slot = slotTimeFor(PRESET_LONG_FAST);
  ASSERT_TRUE(slot >= 27 && slot <= 29);
}

TEST(slot_time_shorter_for_faster_preset) {
  ASSERT_TRUE(slotTimeFor(PRESET_SHORT_FAST) < slotTimeFor(PRESET_LONG_FAST));
}

TEST(preset_names_match_firmware) {
  // These strings are hashed to pick a frequency slot and to derive the
  // channel hash, so they have to be byte identical to the firmware's.
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_LONG_FAST), "LongFast");
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_LONG_MODERATE), "LongMod");
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_MEDIUM_SLOW), "MediumSlow");
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_NARROW_SLOW), "NarrowSlow");
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_LONG_FAST, true), "LongF");
  ASSERT_STREQ(MeshRegion::getPresetName(PRESET_LONG_MODERATE, true), "LongM");
}

TEST(djb2_hash) {
  ASSERT_EQ(MeshRegion::hashName(""), 5381u);
  // 5381 * 33 + 'a'
  ASSERT_EQ(MeshRegion::hashName("a"), 177670u);
}

TEST(slot_width_plain_region) {
  const RegionInfo *us = MeshRegion::getRegion(REGION_US);
  // No spacing and no padding, so a slot is exactly the bandwidth.
  ASSERT_TRUE(us->slotWidth(250.0f) > 0.2499f &&
              us->slotWidth(250.0f) < 0.2501f);
}

TEST(slot_width_includes_padding_and_spacing) {
  // EU_866: 400 kHz spacing plus 37.5 kHz padding either side of a 125 kHz
  // channel gives 0.4 + 0.075 + 0.125 = 0.6 MHz.
  const RegionInfo *eu866 = MeshRegion::getRegion(REGION_EU_866);
  float w = eu866->slotWidth(125.0f);
  ASSERT_TRUE(w > 0.5999f && w < 0.6001f);

  // EU_N_868: no spacing, 10.4 kHz either side of 62.5 kHz gives 0.0833 MHz.
  const RegionInfo *eun = MeshRegion::getRegion(REGION_EU_N_868);
  w = eun->slotWidth(62.5f);
  ASSERT_TRUE(w > 0.0832f && w < 0.0834f);
}

TEST(eu866_lands_on_the_regulatory_channels) {
  // 865.7 / 866.3 / 866.9 / 867.5 MHz, four channels 600 kHz apart.
  const RegionInfo *eu866 = MeshRegion::getRegion(REGION_EU_866);
  ASSERT_EQ(eu866->numSlots(125.0f), 4u);

  float first = eu866->slotFrequency(0, 125.0f);
  ASSERT_TRUE(first > 865.6999f && first < 865.7001f);

  float last = eu866->slotFrequency(3, 125.0f);
  ASSERT_TRUE(last > 867.4999f && last < 867.5001f);
}

TEST(eu_n_868_uses_its_override_slot) {
  // The region pins slot 1, so the channel name must not move it.
  ASSERT_EQ(MeshRegion::getDefaultSlot(REGION_EU_N_868, PRESET_NARROW_SLOW,
                                       "anything"),
            0u);
  float freq =
      MeshRegion::getFrequency(REGION_EU_N_868, PRESET_NARROW_SLOW, "");
  ASSERT_TRUE(freq > 869.4f && freq < 869.65f);
}

TEST(ham_padding_widens_the_channel) {
  // 15.6 kHz coerced to 20 kHz: 0.0022 either side gives 0.02 MHz.
  const RegionInfo *itu = MeshRegion::getRegion(REGION_ITU1_2M);
  float w = itu->slotWidth(15.6f);
  ASSERT_TRUE(w > 0.0199f && w < 0.0201f);
  ASSERT_TRUE(itu->licensedOnly());
}

TEST(us_longfast_default_slot) {
  // The documented default: LongFast on US is slot 20, 906.875 MHz.
  ASSERT_EQ(MeshRegion::getDefaultSlot(REGION_US, PRESET_LONG_FAST, ""), 19u);
  float freq = MeshRegion::getFrequency(REGION_US, PRESET_LONG_FAST, "");
  ASSERT_TRUE(freq > 906.8749f && freq < 906.8751f);
}

TEST(explicit_slot_is_one_based) {
  float slot1 = MeshRegion::getFrequencyForSlot(REGION_US, PRESET_LONG_FAST, 1);
  const RegionInfo *us = MeshRegion::getRegion(REGION_US);
  ASSERT_TRUE(slot1 == us->slotFrequency(0, 250.0f));
  ASSERT_TRUE(MeshRegion::getFrequencyForSlot(REGION_US, PRESET_LONG_FAST, 0) ==
              0.0f);
}

TEST(jp_slots_fit_the_band) {
  // 920.5 to 923.5 MHz at 250 kHz gives 12 slots, all inside the band.
  const RegionInfo *jp = MeshRegion::getRegion(REGION_JP);
  uint32_t slots = jp->numSlots(250.0f);
  ASSERT_EQ(slots, 12u);
  ASSERT_TRUE(jp->slotFrequency(0, 250.0f) >= jp->freqStart);
  ASSERT_TRUE(jp->slotFrequency(slots - 1, 250.0f) <= jp->freqEnd);
}

TEST(every_region_first_slot_inside_band) {
  size_t count = 0;
  const RegionInfo *regions = MeshRegion::getAllRegions(count);
  for (size_t i = 0; i < count; i++) {
    const RegionInfo &r = regions[i];
    ModemParams p = MeshRegion::getModemParams(r.defaultPreset, r.wideLora);
    ASSERT_TRUE(r.numSlots(p.bw) > 0);
    float first = r.slotFrequency(0, p.bw);
    ASSERT_TRUE(first >= r.freqStart);
    ASSERT_TRUE(first <= r.freqEnd);
  }
}

TEST(slot_count_rounds_like_the_firmware) {
  // numSlots rounds to nearest, which can put the last slot's upper edge past
  // the band edge. That is what the firmware does, and interoperating matters
  // more than the band edge here, so it is reproduced rather than corrected.
  const RegionInfo *ph868 = MeshRegion::getRegion(REGION_PH_868);
  ASSERT_EQ(ph868->numSlots(250.0f), 6u);
  float top = ph868->slotFrequency(5, 250.0f) + 0.125f;
  ASSERT_TRUE(top > ph868->freqEnd);
}

TEST(ham_regions_are_exact_after_padding) {
  // 15.6 kHz padded to 20 kHz gives exactly 100 slots in 2 MHz.
  const RegionInfo *itu1 = MeshRegion::getRegion(REGION_ITU1_2M);
  ASSERT_EQ(itu1->numSlots(15.6f), 100u);
  // 62.5 kHz padded to 100 kHz gives exactly 100 slots in 10 MHz.
  const RegionInfo *itu70 = MeshRegion::getRegion(REGION_ITU1_70CM);
  ASSERT_EQ(itu70->numSlots(62.5f), 100u);
}

TEST(eu_n_868_narrow_slots) {
  // 62.5 kHz padded to 83.3 kHz fits three slots in the 250 kHz band.
  const RegionInfo *eun = MeshRegion::getRegion(REGION_EU_N_868);
  ASSERT_EQ(eun->numSlots(62.5f), 3u);
}

int main() {
  printf("libmeshtastic_leaf Unit Tests\n");
  printf("=========================\n\n");

  printf("MeshTypes Tests:\n");
  printf("MeshAirtime Tests:\n");
  RUN_TEST(airtime_starts_empty);
  RUN_TEST(airtime_tx_counts_towards_duty_cycle);
  RUN_TEST(airtime_rx_excluded_from_duty_cycle);
  RUN_TEST(airtime_tx_expires_after_an_hour);
  RUN_TEST(airtime_channel_window_is_one_minute);
  RUN_TEST(airtime_survives_a_long_idle_gap);
  RUN_TEST(slot_time_long_fast);
  RUN_TEST(slot_time_shorter_for_faster_preset);

  printf("\nMeshTypes Tests:\n");
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
  RUN_TEST(preset_names_match_firmware);
  RUN_TEST(djb2_hash);
  RUN_TEST(slot_width_plain_region);
  RUN_TEST(slot_width_includes_padding_and_spacing);
  RUN_TEST(eu866_lands_on_the_regulatory_channels);
  RUN_TEST(eu_n_868_uses_its_override_slot);
  RUN_TEST(ham_padding_widens_the_channel);
  RUN_TEST(us_longfast_default_slot);
  RUN_TEST(explicit_slot_is_one_based);
  RUN_TEST(jp_slots_fit_the_band);
  RUN_TEST(every_region_first_slot_inside_band);
  RUN_TEST(slot_count_rounds_like_the_firmware);
  RUN_TEST(ham_regions_are_exact_after_padding);
  RUN_TEST(eu_n_868_narrow_slots);
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
  RUN_TEST(modem_preset_long_slow);
  RUN_TEST(modem_preset_narrow_and_tiny);
  RUN_TEST(modem_preset_wide_lora);
  RUN_TEST(preset_name);

  printf("\n=========================\n");
  printf("Tests run: %d\n", tests_run);
  printf("Tests passed: %d\n", tests_passed);
  printf("Result: %s\n", tests_passed == tests_run ? "ALL PASSED" : "FAILED");

  return tests_passed == tests_run ? 0 : 1;
}
