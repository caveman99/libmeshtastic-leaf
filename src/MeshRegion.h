#pragma once

#include "MeshTypes.h"
#include <math.h>
#include <stdint.h>

namespace libmeshtastic_leaf {

// Slot selection when a region does not pin one.
constexpr int16_t OVERRIDE_SLOT_PRESET_HASH = -1;

// Regulatory parameters shared by several regions. spacing gaps the channels,
// padding gaps each side of one. See the slot section of ARCHITECTURE.md.
struct RegionProfile {
  float spacing;     ///< MHz
  float padding;     ///< MHz, applied at both sides
  bool licensedOnly; ///< licensed operators only
};

struct RegionInfo {
  RegionCode code;
  float freqStart;              ///< MHz, band start
  float freqEnd;                ///< MHz, band end
  float dutyCycle;              ///< percent, 100 means no limit
  int8_t powerLimit;            ///< dBm
  bool freqSwitching;           ///< frequency hopping allowed
  bool wideLora;                ///< 2.4 GHz wide LoRa
  const RegionProfile *profile; ///< spacing and padding
  ModemPreset defaultPreset;
  int16_t overrideSlot; ///< >0 pins a slot, -1 hashes the preset name, 0 hashes
                        ///< the channel name
  const char *name;

  bool licensedOnly() const { return profile->licensedOnly; }

  // Width of one frequency slot, bandwidth in kHz.
  float slotWidth(float bandwidthKHz) const {
    return profile->spacing + (profile->padding * 2.0f) +
           (bandwidthKHz / 1000.0f);
  }

  uint32_t numSlots(float bandwidthKHz) const {
    const float width = slotWidth(bandwidthKHz);
    if (width <= 0.0f) {
      return 0;
    }
    const float slots = (freqEnd - freqStart + profile->spacing) / width;
    return slots < 0.0f ? 0 : (uint32_t)(slots + 0.5f);
  }

  // Centre of a zero-based slot. Half a bandwidth plus the padding puts the
  // first channel clear of the band edge.
  float slotFrequency(uint32_t slot, float bandwidthKHz) const {
    return freqStart + (bandwidthKHz / 2000.0f) + profile->padding +
           ((float)slot * slotWidth(bandwidthKHz));
  }
};

struct ModemParams {
  uint8_t sf; ///< Spreading factor (7-12)
  float bw;   ///< Bandwidth in kHz
  uint8_t cr; ///< Coding rate (5-8, represents 4/5 to 4/8)
};

class MeshRegion {
public:
  static const RegionInfo *getRegion(RegionCode code);

  static const char *getRegionName(RegionCode code);

  static float getDefaultFrequency(RegionCode code);

  static int8_t getPowerLimit(RegionCode code);

  static bool isWideLoRa(RegionCode code);

  static ModemParams getModemParams(ModemPreset preset, bool wideLora = false);

  // Firmware-exact names. The long form is what an unnamed channel is called,
  // and both the channel hash and the preset slot hash depend on it.
  static const char *getPresetName(ModemPreset preset, bool shortName = false);

  // djb2, as used by the firmware to pick a default frequency slot.
  static uint32_t hashName(const char *name);

  // Slot the region would use for this channel name, zero based.
  static uint32_t getDefaultSlot(RegionCode code, ModemPreset preset,
                                 const char *channelName);

  // Centre frequency for the region, preset and channel name.
  static float getFrequency(RegionCode code, ModemPreset preset,
                            const char *channelName);

  // Centre frequency of an explicit one-based slot, as configured by a user.
  static float getFrequencyForSlot(RegionCode code, ModemPreset preset,
                                   uint32_t slotOneBased);

  static const RegionInfo *getAllRegions(size_t &count);
};

namespace detail {

// Profiles shared across regions: spacing, padding, audio, licensed only.
static const RegionProfile PROFILE_STD = {0.0f, 0.0f, false};
static const RegionProfile PROFILE_EU868 = {0.0f, 0.0f, false};
static const RegionProfile PROFILE_UNDEF = {0.0f, 0.0f, false};
static const RegionProfile PROFILE_LITE = {0.4f, 0.0375f, false};
static const RegionProfile PROFILE_NARROW = {0.0f, 0.0104f, false};
// 15.6 kHz bandwidth coerced up to a 20 kHz channel by padding.
static const RegionProfile PROFILE_HAM_20KHZ = {0.0f, 0.0022f, true};
// 62.5 kHz bandwidth coerced up to a 100 kHz channel by padding.
static const RegionProfile PROFILE_HAM_100KHZ = {0.0f, 0.01875f, true};

// Regulatory limits per region. UNSET must stay last.
static const RegionInfo REGIONS[] = {
    // code, freqStart, freqEnd, duty, power, fhop, wide, profile, preset, slot,
    // name
    {REGION_US, 902.0f, 928.0f, 100, 30, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "US"},
    {REGION_EU_433, 433.0f, 434.0f, 10, 10, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "EU_433"},
    {REGION_EU_868, 869.4f, 869.65f, 10, 27, false, false, &PROFILE_EU868,
     PRESET_LONG_FAST, 0, "EU_868"},
    {REGION_EU_866, 865.6f, 867.6f, 2.5f, 27, false, false, &PROFILE_LITE,
     PRESET_LITE_FAST, 0, "EU_866"},
    {REGION_EU_N_868, 869.4f, 869.65f, 10, 27, false, false, &PROFILE_NARROW,
     PRESET_NARROW_SLOW, 1, "EU_N_868"},
    {REGION_CN, 470.0f, 510.0f, 100, 19, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "CN"},
    {REGION_JP, 920.5f, 923.5f, 100, 13, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "JP"},
    {REGION_ANZ, 915.0f, 928.0f, 100, 30, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "ANZ"},
    {REGION_ANZ_433, 433.05f, 434.79f, 100, 14, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "ANZ_433"},
    {REGION_RU, 868.7f, 869.2f, 100, 20, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "RU"},
    {REGION_KR, 920.0f, 923.0f, 100, 23, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "KR"},
    {REGION_TW, 920.0f, 925.0f, 100, 27, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "TW"},
    {REGION_IN, 865.0f, 867.0f, 100, 30, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "IN"},
    {REGION_NZ_865, 864.0f, 868.0f, 100, 36, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "NZ_865"},
    {REGION_TH, 920.0f, 925.0f, 10, 27, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "TH"},
    {REGION_UA_433, 433.0f, 434.7f, 10, 10, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "UA_433"},
    {REGION_UA_868, 868.0f, 868.6f, 1, 14, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "UA_868"},
    {REGION_MY_433, 433.0f, 435.0f, 100, 20, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "MY_433"},
    {REGION_MY_919, 919.0f, 924.0f, 100, 27, true, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "MY_919"},
    {REGION_SG_923, 917.0f, 925.0f, 100, 20, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "SG_923"},
    {REGION_PH_433, 433.0f, 434.7f, 100, 10, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "PH_433"},
    {REGION_PH_868, 868.0f, 869.4f, 100, 14, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "PH_868"},
    {REGION_PH_915, 915.0f, 918.0f, 100, 24, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "PH_915"},
    {REGION_KZ_433, 433.075f, 434.775f, 100, 10, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "KZ_433"},
    {REGION_KZ_863, 863.0f, 868.0f, 100, 30, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "KZ_863"},
    {REGION_NP_865, 865.0f, 868.0f, 100, 30, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "NP_865"},
    {REGION_BR_902, 902.0f, 907.5f, 100, 30, false, false, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "BR_902"},
    {REGION_ITU1_2M, 144.0f, 146.0f, 100, 30, false, false, &PROFILE_HAM_20KHZ,
     PRESET_TINY_FAST, 26, "ITU1_2M"},
    {REGION_ITU2_2M, 144.0f, 148.0f, 100, 30, false, false, &PROFILE_HAM_20KHZ,
     PRESET_TINY_FAST, 51, "ITU2_2M"},
    {REGION_ITU3_2M, 144.0f, 148.0f, 100, 30, false, false, &PROFILE_HAM_20KHZ,
     PRESET_TINY_FAST, 33, "ITU3_2M"},
    {REGION_ITU2_125CM, 220.0f, 225.0f, 100, 30, false, false,
     &PROFILE_HAM_100KHZ, PRESET_NARROW_SLOW, 37, "ITU2_125CM"},
    {REGION_ITU1_70CM, 430.0f, 440.0f, 100, 30, false, false,
     &PROFILE_HAM_100KHZ, PRESET_NARROW_SLOW, 37, "ITU1_70CM"},
    {REGION_ITU2_70CM, 420.0f, 450.0f, 100, 30, false, false,
     &PROFILE_HAM_100KHZ, PRESET_NARROW_SLOW, 137, "ITU2_70CM"},
    {REGION_ITU3_70CM, 430.0f, 450.0f, 100, 30, false, false,
     &PROFILE_HAM_100KHZ, PRESET_NARROW_SLOW, 37, "ITU3_70CM"},
    {REGION_LORA_24, 2400.0f, 2483.5f, 100, 10, false, true, &PROFILE_STD,
     PRESET_LONG_FAST, 0, "LORA_24"},
    {REGION_UNSET, 902.0f, 928.0f, 100, 30, false, false, &PROFILE_UNDEF,
     PRESET_LONG_FAST, 0, "UNSET"},
};

static constexpr size_t NUM_REGIONS = sizeof(REGIONS) / sizeof(REGIONS[0]);

} // namespace detail

inline const RegionInfo *MeshRegion::getRegion(RegionCode code) {
  for (size_t i = 0; i < detail::NUM_REGIONS; i++) {
    if (detail::REGIONS[i].code == code) {
      return &detail::REGIONS[i];
    }
  }
  return nullptr;
}

inline const char *MeshRegion::getRegionName(RegionCode code) {
  const RegionInfo *region = getRegion(code);
  return region ? region->name : "UNKNOWN";
}

inline float MeshRegion::getDefaultFrequency(RegionCode code) {
  const RegionInfo *region = getRegion(code);
  return region ? getFrequency(code, region->defaultPreset, "") : 0.0f;
}

inline uint32_t MeshRegion::hashName(const char *name) {
  uint32_t hash = 5381;
  while (*name) {
    hash = ((hash << 5) + hash) + (uint8_t)(*name++);
  }
  return hash;
}

inline uint32_t MeshRegion::getDefaultSlot(RegionCode code, ModemPreset preset,
                                           const char *channelName) {
  const RegionInfo *region = getRegion(code);
  if (region == nullptr) {
    return 0;
  }

  const ModemParams p = getModemParams(preset, region->wideLora);
  const uint32_t slots = region->numSlots(p.bw);
  if (slots == 0) {
    return 0;
  }

  if (region->overrideSlot > 0) {
    return (uint32_t)(region->overrideSlot - 1);
  }
  if (region->overrideSlot == OVERRIDE_SLOT_PRESET_HASH) {
    return hashName(getPresetName(preset)) % slots;
  }

  // An unnamed channel is called after the preset, so it hashes the same way
  // the firmware's does.
  const char *name = (channelName != nullptr && channelName[0] != '\0')
                         ? channelName
                         : getPresetName(preset);
  return hashName(name) % slots;
}

inline float MeshRegion::getFrequency(RegionCode code, ModemPreset preset,
                                      const char *channelName) {
  const RegionInfo *region = getRegion(code);
  if (region == nullptr) {
    return 0.0f;
  }
  const ModemParams p = getModemParams(preset, region->wideLora);
  return region->slotFrequency(getDefaultSlot(code, preset, channelName), p.bw);
}

inline float MeshRegion::getFrequencyForSlot(RegionCode code,
                                             ModemPreset preset,
                                             uint32_t slotOneBased) {
  const RegionInfo *region = getRegion(code);
  if (region == nullptr || slotOneBased == 0) {
    return 0.0f;
  }
  const ModemParams p = getModemParams(preset, region->wideLora);
  return region->slotFrequency(slotOneBased - 1, p.bw);
}

inline int8_t MeshRegion::getPowerLimit(RegionCode code) {
  const RegionInfo *region = getRegion(code);
  return region ? region->powerLimit : 0;
}

inline bool MeshRegion::isWideLoRa(RegionCode code) {
  const RegionInfo *region = getRegion(code);
  return region ? region->wideLora : false;
}

inline ModemParams MeshRegion::getModemParams(ModemPreset preset,
                                              bool wideLora) {
  ModemParams p;
  switch (preset) {
  case PRESET_SHORT_TURBO:
    p.bw = wideLora ? 1625.0f : 500.0f;
    p.sf = 7;
    p.cr = 5;
    break;
  case PRESET_SHORT_FAST:
    p.bw = wideLora ? 812.5f : 250.0f;
    p.sf = 7;
    p.cr = 5;
    break;
  case PRESET_SHORT_SLOW:
    p.bw = wideLora ? 812.5f : 250.0f;
    p.sf = 8;
    p.cr = 5;
    break;
  case PRESET_MEDIUM_FAST:
    p.bw = wideLora ? 812.5f : 250.0f;
    p.sf = 9;
    p.cr = 5;
    break;
  case PRESET_MEDIUM_SLOW:
    p.bw = wideLora ? 812.5f : 250.0f;
    p.sf = 10;
    p.cr = 5;
    break;
  case PRESET_MEDIUM_TURBO:
    p.bw = wideLora ? 1625.0f : 500.0f;
    p.sf = 9;
    p.cr = 5;
    break;
  case PRESET_LONG_TURBO:
    p.bw = wideLora ? 1625.0f : 500.0f;
    p.sf = 11;
    p.cr = 8;
    break;
  case PRESET_LONG_MODERATE:
    p.bw = wideLora ? 406.25f : 125.0f;
    p.sf = 11;
    p.cr = 8;
    break;
  case PRESET_LONG_SLOW:
    p.bw = wideLora ? 406.25f : 125.0f;
    p.sf = 12;
    p.cr = 8;
    break;
  case PRESET_LITE_FAST:
    p.bw = 125.0f;
    p.sf = 9;
    p.cr = 5;
    break;
  case PRESET_LITE_SLOW:
    p.bw = 125.0f;
    p.sf = 10;
    p.cr = 5;
    break;
  case PRESET_NARROW_FAST:
    p.bw = 62.5f;
    p.sf = 7;
    p.cr = 6;
    break;
  case PRESET_NARROW_SLOW:
    p.bw = 62.5f;
    p.sf = 8;
    p.cr = 6;
    break;
  case PRESET_TINY_FAST:
    p.bw = 15.6f;
    p.sf = 7;
    p.cr = 5;
    break;
  case PRESET_TINY_SLOW:
    p.bw = 15.6f;
    p.sf = 8;
    p.cr = 6;
    break;
  case PRESET_LONG_FAST:
  default:
    p.bw = wideLora ? 812.5f : 250.0f;
    p.sf = 11;
    p.cr = 5;
    break;
  }
  return p;
}

inline const char *MeshRegion::getPresetName(ModemPreset preset,
                                             bool shortName) {
  switch (preset) {
  case PRESET_LONG_FAST:
    return shortName ? "LongF" : "LongFast";
  case PRESET_LONG_SLOW:
    return shortName ? "LongS" : "LongSlow";
  case PRESET_LONG_MODERATE:
    return shortName ? "LongM" : "LongMod";
  case PRESET_LONG_TURBO:
    return shortName ? "LongT" : "LongTurbo";
  case PRESET_MEDIUM_FAST:
    return shortName ? "MedF" : "MediumFast";
  case PRESET_MEDIUM_SLOW:
    return shortName ? "MedS" : "MediumSlow";
  case PRESET_MEDIUM_TURBO:
    return shortName ? "MedT" : "MediumTurbo";
  case PRESET_SHORT_FAST:
    return shortName ? "ShortF" : "ShortFast";
  case PRESET_SHORT_SLOW:
    return shortName ? "ShortS" : "ShortSlow";
  case PRESET_SHORT_TURBO:
    return shortName ? "ShortT" : "ShortTurbo";
  case PRESET_LITE_FAST:
    return shortName ? "LiteF" : "LiteFast";
  case PRESET_LITE_SLOW:
    return shortName ? "LiteS" : "LiteSlow";
  case PRESET_NARROW_FAST:
    return shortName ? "NarF" : "NarrowFast";
  case PRESET_NARROW_SLOW:
    return shortName ? "NarS" : "NarrowSlow";
  case PRESET_TINY_FAST:
    return shortName ? "TinyF" : "TinyFast";
  case PRESET_TINY_SLOW:
    return shortName ? "TinyS" : "TinySlow";
  default:
    return shortName ? "Custom" : "Invalid";
  }
}

inline const RegionInfo *MeshRegion::getAllRegions(size_t &count) {
  count = detail::NUM_REGIONS;
  return detail::REGIONS;
}

} // namespace libmeshtastic_leaf
