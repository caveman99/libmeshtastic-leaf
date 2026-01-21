/**
 * @file MeshRegion.h
 * @brief Region and modem preset helpers
 *
 * This file provides helper functions and lookup tables for:
 * - Region-specific frequency parameters
 * - Modem preset parameters (SF, BW, CR)
 *
 * Uses the protobuf-generated enums for type safety and compatibility
 * with the main Meshtastic firmware.
 */

#pragma once

#include <stdint.h>
#include "generated/meshtastic/config.pb.h"

namespace libmeshtastic_leaf {

// ============================================================================
// Type Aliases for Protobuf Enums
// ============================================================================

/// Region code using protobuf enum
using RegionCode = meshtastic_Config_LoRaConfig_RegionCode;

/// Modem preset using protobuf enum
using ModemPreset = meshtastic_Config_LoRaConfig_ModemPreset;

// ============================================================================
// Region Constants (for convenience)
// ============================================================================

constexpr RegionCode REGION_UNSET   = meshtastic_Config_LoRaConfig_RegionCode_UNSET;
constexpr RegionCode REGION_US      = meshtastic_Config_LoRaConfig_RegionCode_US;
constexpr RegionCode REGION_EU_433  = meshtastic_Config_LoRaConfig_RegionCode_EU_433;
constexpr RegionCode REGION_EU_868  = meshtastic_Config_LoRaConfig_RegionCode_EU_868;
constexpr RegionCode REGION_CN      = meshtastic_Config_LoRaConfig_RegionCode_CN;
constexpr RegionCode REGION_JP      = meshtastic_Config_LoRaConfig_RegionCode_JP;
constexpr RegionCode REGION_ANZ     = meshtastic_Config_LoRaConfig_RegionCode_ANZ;
constexpr RegionCode REGION_KR      = meshtastic_Config_LoRaConfig_RegionCode_KR;
constexpr RegionCode REGION_TW      = meshtastic_Config_LoRaConfig_RegionCode_TW;
constexpr RegionCode REGION_RU      = meshtastic_Config_LoRaConfig_RegionCode_RU;
constexpr RegionCode REGION_IN      = meshtastic_Config_LoRaConfig_RegionCode_IN;
constexpr RegionCode REGION_NZ_865  = meshtastic_Config_LoRaConfig_RegionCode_NZ_865;
constexpr RegionCode REGION_TH      = meshtastic_Config_LoRaConfig_RegionCode_TH;
constexpr RegionCode REGION_LORA_24 = meshtastic_Config_LoRaConfig_RegionCode_LORA_24;
constexpr RegionCode REGION_UA_433  = meshtastic_Config_LoRaConfig_RegionCode_UA_433;
constexpr RegionCode REGION_UA_868  = meshtastic_Config_LoRaConfig_RegionCode_UA_868;
constexpr RegionCode REGION_MY_433  = meshtastic_Config_LoRaConfig_RegionCode_MY_433;
constexpr RegionCode REGION_MY_919  = meshtastic_Config_LoRaConfig_RegionCode_MY_919;
constexpr RegionCode REGION_SG_923  = meshtastic_Config_LoRaConfig_RegionCode_SG_923;
constexpr RegionCode REGION_PH_433  = meshtastic_Config_LoRaConfig_RegionCode_PH_433;
constexpr RegionCode REGION_PH_868  = meshtastic_Config_LoRaConfig_RegionCode_PH_868;
constexpr RegionCode REGION_PH_915  = meshtastic_Config_LoRaConfig_RegionCode_PH_915;
constexpr RegionCode REGION_ANZ_433 = meshtastic_Config_LoRaConfig_RegionCode_ANZ_433;
constexpr RegionCode REGION_KZ_433  = meshtastic_Config_LoRaConfig_RegionCode_KZ_433;
constexpr RegionCode REGION_KZ_863  = meshtastic_Config_LoRaConfig_RegionCode_KZ_863;
constexpr RegionCode REGION_NP_865  = meshtastic_Config_LoRaConfig_RegionCode_NP_865;
constexpr RegionCode REGION_BR_902  = meshtastic_Config_LoRaConfig_RegionCode_BR_902;

// ============================================================================
// Modem Preset Constants (for convenience)
// ============================================================================

constexpr ModemPreset PRESET_LONG_FAST      = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST;
constexpr ModemPreset PRESET_LONG_SLOW      = meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW;
constexpr ModemPreset PRESET_VERY_LONG_SLOW = meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW;
constexpr ModemPreset PRESET_MEDIUM_SLOW    = meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW;
constexpr ModemPreset PRESET_MEDIUM_FAST    = meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST;
constexpr ModemPreset PRESET_SHORT_SLOW     = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW;
constexpr ModemPreset PRESET_SHORT_FAST     = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST;
constexpr ModemPreset PRESET_LONG_MODERATE  = meshtastic_Config_LoRaConfig_ModemPreset_LONG_MODERATE;
constexpr ModemPreset PRESET_SHORT_TURBO    = meshtastic_Config_LoRaConfig_ModemPreset_SHORT_TURBO;
constexpr ModemPreset PRESET_LONG_TURBO     = meshtastic_Config_LoRaConfig_ModemPreset_LONG_TURBO;

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Region information structure
 *
 * Contains all regulatory parameters for a given region.
 */
struct RegionInfo {
    RegionCode code;        ///< Region code
    float freqStart;        ///< Start frequency in MHz
    float freqEnd;          ///< End frequency in MHz
    uint8_t dutyCycle;      ///< Duty cycle percentage (100 = no limit)
    uint8_t spacing;        ///< Channel spacing in kHz
    int8_t powerLimit;      ///< Max TX power in dBm
    bool audioPermitted;    ///< Voice/audio permitted
    bool freqSwitching;     ///< Frequency hopping allowed
    bool wideLora;          ///< 2.4GHz wide LoRa mode
    const char* name;       ///< Region name string

    /**
     * @brief Get the default frequency for this region
     *
     * Returns the first channel center frequency based on the default
     * LONG_FAST bandwidth of 250 kHz.
     */
    float getDefaultFrequency() const {
        return freqStart + 0.125f; // BW/2 = 250/2/1000 = 0.125 MHz
    }

    /**
     * @brief Get the number of channels available
     *
     * @param bandwidth Bandwidth in kHz
     * @return Number of channels
     */
    uint32_t getNumChannels(float bandwidth) const {
        return (uint32_t)((freqEnd - freqStart) / (spacing / 1000.0f + bandwidth / 1000.0f));
    }

    /**
     * @brief Calculate channel frequency
     *
     * @param channelNum Channel number (0-based)
     * @param bandwidth Bandwidth in kHz
     * @return Channel center frequency in MHz
     */
    float getChannelFrequency(uint32_t channelNum, float bandwidth) const {
        return freqStart + (bandwidth / 2000.0f) + (channelNum * (bandwidth / 1000.0f));
    }
};

/**
 * @brief Modem parameters for a preset
 */
struct ModemParams {
    uint8_t sf;     ///< Spreading factor (7-12)
    float bw;       ///< Bandwidth in kHz
    uint8_t cr;     ///< Coding rate (5-8, represents 4/5 to 4/8)
};

// ============================================================================
// Region Lookup Class
// ============================================================================

/**
 * @brief Region and modem preset lookup utilities
 */
class MeshRegion {
public:
    /**
     * @brief Get region information by code
     *
     * @param code Region code
     * @return Pointer to RegionInfo, or nullptr if not found
     */
    static const RegionInfo* getRegion(RegionCode code);

    /**
     * @brief Get region name string
     *
     * @param code Region code
     * @return Region name, or "UNKNOWN" if not found
     */
    static const char* getRegionName(RegionCode code);

    /**
     * @brief Get default frequency for a region
     *
     * Returns the first channel center frequency using LONG_FAST bandwidth.
     *
     * @param code Region code
     * @return Frequency in MHz, or 0.0 if region not found
     */
    static float getDefaultFrequency(RegionCode code);

    /**
     * @brief Get power limit for a region
     *
     * @param code Region code
     * @return Power limit in dBm, or 0 if region not found
     */
    static int8_t getPowerLimit(RegionCode code);

    /**
     * @brief Check if a region uses 2.4GHz (wide LoRa)
     *
     * @param code Region code
     * @return true if 2.4GHz region
     */
    static bool isWideLoRa(RegionCode code);

    /**
     * @brief Get modem parameters for a preset
     *
     * @param preset Modem preset
     * @param wideLora true for 2.4GHz regions (uses different bandwidths)
     * @return Modem parameters
     */
    static ModemParams getModemParams(ModemPreset preset, bool wideLora = false);

    /**
     * @brief Get preset name string
     *
     * @param preset Modem preset
     * @return Preset name
     */
    static const char* getPresetName(ModemPreset preset);

    /**
     * @brief Get all region codes
     *
     * @param count Output: number of regions
     * @return Pointer to array of RegionInfo
     */
    static const RegionInfo* getAllRegions(size_t& count);
};

// ============================================================================
// Implementation
// ============================================================================

namespace detail {

// Region table matching main firmware (RadioInterface.cpp)
static const RegionInfo REGIONS[] = {
    // code,        freqStart,  freqEnd,  duty, space, power, audio, fhop, wide, name
    { REGION_US,        902.0f,   928.0f,  100,  0,  30, true,  false, false, "US" },
    { REGION_EU_433,    433.0f,   434.0f,   10,  0,  10, true,  false, false, "EU_433" },
    { REGION_EU_868,    869.4f,  869.65f,   10,  0,  27, false, false, false, "EU_868" },
    { REGION_CN,        470.0f,   510.0f,  100,  0,  19, true,  false, false, "CN" },
    { REGION_JP,        920.5f,   923.5f,  100,  0,  13, true,  false, false, "JP" },
    { REGION_ANZ,       915.0f,   928.0f,  100,  0,  30, true,  false, false, "ANZ" },
    { REGION_KR,        920.0f,   923.0f,  100,  0,  23, true,  false, false, "KR" },
    { REGION_TW,        920.0f,   925.0f,  100,  0,  27, true,  false, false, "TW" },
    { REGION_RU,        868.7f,   869.2f,  100,  0,  20, true,  false, false, "RU" },
    { REGION_IN,        865.0f,   867.0f,  100,  0,  30, true,  false, false, "IN" },
    { REGION_NZ_865,    864.0f,   868.0f,  100,  0,  36, true,  false, false, "NZ_865" },
    { REGION_TH,        920.0f,   925.0f,  100,  0,  16, true,  false, false, "TH" },
    { REGION_UA_433,    433.0f,   434.7f,   10,  0,  10, true,  false, false, "UA_433" },
    { REGION_UA_868,    868.0f,   868.6f,    1,  0,  14, true,  false, false, "UA_868" },
    { REGION_MY_433,    433.0f,   435.0f,  100,  0,  20, true,  false, false, "MY_433" },
    { REGION_MY_919,    919.0f,   924.0f,  100,  0,  27, true,  true,  false, "MY_919" },
    { REGION_SG_923,    917.0f,   925.0f,  100,  0,  20, true,  false, false, "SG_923" },
    { REGION_PH_433,    433.0f,   434.7f,  100,  0,  10, true,  false, false, "PH_433" },
    { REGION_PH_868,    868.0f,   869.4f,  100,  0,  14, true,  false, false, "PH_868" },
    { REGION_PH_915,    915.0f,   918.0f,  100,  0,  24, true,  false, false, "PH_915" },
    { REGION_ANZ_433,   433.05f, 434.79f,  100,  0,  14, true,  false, false, "ANZ_433" },
    { REGION_KZ_433,   433.075f, 434.775f, 100,  0,  10, true,  false, false, "KZ_433" },
    { REGION_KZ_863,    863.0f,   868.0f,  100,  0,  30, true,  false, false, "KZ_863" },
    { REGION_NP_865,    865.0f,   868.0f,  100,  0,  30, true,  false, false, "NP_865" },
    { REGION_BR_902,    902.0f,   907.5f,  100,  0,  30, true,  false, false, "BR_902" },
    { REGION_LORA_24,  2400.0f,  2483.5f,  100,  0,  10, true,  false, true,  "LORA_24" },
    { REGION_UNSET,     902.0f,   928.0f,  100,  0,  30, true,  false, false, "UNSET" },
};

static constexpr size_t NUM_REGIONS = sizeof(REGIONS) / sizeof(REGIONS[0]);

} // namespace detail

inline const RegionInfo* MeshRegion::getRegion(RegionCode code) {
    for (size_t i = 0; i < detail::NUM_REGIONS; i++) {
        if (detail::REGIONS[i].code == code) {
            return &detail::REGIONS[i];
        }
    }
    return nullptr;
}

inline const char* MeshRegion::getRegionName(RegionCode code) {
    const RegionInfo* region = getRegion(code);
    return region ? region->name : "UNKNOWN";
}

inline float MeshRegion::getDefaultFrequency(RegionCode code) {
    const RegionInfo* region = getRegion(code);
    return region ? region->getDefaultFrequency() : 0.0f;
}

inline int8_t MeshRegion::getPowerLimit(RegionCode code) {
    const RegionInfo* region = getRegion(code);
    return region ? region->powerLimit : 0;
}

inline bool MeshRegion::isWideLoRa(RegionCode code) {
    const RegionInfo* region = getRegion(code);
    return region ? region->wideLora : false;
}

inline ModemParams MeshRegion::getModemParams(ModemPreset preset, bool wideLora) {
    ModemParams p;

    // Bandwidth values differ for 2.4GHz wide LoRa mode
    switch (preset) {
        case PRESET_SHORT_TURBO:
            p.sf = 7;
            p.bw = wideLora ? 1625.0f : 500.0f;
            p.cr = 5;
            break;

        case PRESET_SHORT_FAST:
            p.sf = 7;
            p.bw = wideLora ? 812.5f : 250.0f;
            p.cr = 5;
            break;

        case PRESET_SHORT_SLOW:
            p.sf = 8;
            p.bw = wideLora ? 812.5f : 250.0f;
            p.cr = 5;
            break;

        case PRESET_MEDIUM_FAST:
            p.sf = 9;
            p.bw = wideLora ? 812.5f : 250.0f;
            p.cr = 5;
            break;

        case PRESET_MEDIUM_SLOW:
            p.sf = 10;
            p.bw = wideLora ? 812.5f : 250.0f;
            p.cr = 5;
            break;

        case PRESET_LONG_TURBO:
            p.sf = 11;
            p.bw = wideLora ? 1625.0f : 500.0f;
            p.cr = 8;
            break;

        case PRESET_LONG_FAST:
        default:
            p.sf = 11;
            p.bw = wideLora ? 812.5f : 250.0f;
            p.cr = 5;
            break;

        case PRESET_LONG_MODERATE:
            p.sf = 11;
            p.bw = wideLora ? 406.25f : 125.0f;
            p.cr = 8;
            break;

        case PRESET_LONG_SLOW:
            p.sf = 12;
            p.bw = wideLora ? 406.25f : 125.0f;
            p.cr = 8;
            break;

        case PRESET_VERY_LONG_SLOW:
            p.sf = 12;
            p.bw = wideLora ? 203.125f : 62.5f;
            p.cr = 8;
            break;
    }

    return p;
}

inline const char* MeshRegion::getPresetName(ModemPreset preset) {
    switch (preset) {
        case PRESET_LONG_FAST:      return "Long Fast";
        case PRESET_LONG_SLOW:      return "Long Slow";
        case PRESET_VERY_LONG_SLOW: return "Very Long Slow";
        case PRESET_MEDIUM_SLOW:    return "Medium Slow";
        case PRESET_MEDIUM_FAST:    return "Medium Fast";
        case PRESET_SHORT_SLOW:     return "Short Slow";
        case PRESET_SHORT_FAST:     return "Short Fast";
        case PRESET_LONG_MODERATE:  return "Long Moderate";
        case PRESET_SHORT_TURBO:    return "Short Turbo";
        case PRESET_LONG_TURBO:     return "Long Turbo";
        default:                    return "Unknown";
    }
}

inline const RegionInfo* MeshRegion::getAllRegions(size_t& count) {
    count = detail::NUM_REGIONS;
    return detail::REGIONS;
}

} // namespace libmeshtastic_leaf
