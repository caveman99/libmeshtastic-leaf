/**
 * @file MeshNodeId.h
 * @brief Hardware-based node ID generation utilities
 *
 * This file provides platform-specific functions to obtain a unique node
 * number from hardware identifiers (MAC address, device ID, etc.), similar
 * to how the main Meshtastic firmware generates node IDs.
 *
 * Supported platforms:
 * - ESP32/ESP32-S2/ESP32-S3/ESP32-C3/ESP32-C6
 * - nRF52840/nRF52832
 * - RP2040/RP2350
 * - STM32WL
 * - Generic (uses random if no hardware ID available)
 */

#pragma once

#include "MeshTypes.h"
#include <stdint.h>

namespace libmeshtastic_leaf {

/**
 * @brief Hardware node ID utilities
 *
 * Provides methods to obtain unique identifiers from hardware.
 */
class MeshNodeId {
public:
    /**
     * @brief Get the hardware MAC address
     *
     * Retrieves the 6-byte MAC address or device identifier from the
     * hardware. On platforms without a MAC address, this may return
     * a unique device ID or a pseudo-random value.
     *
     * @param mac Output buffer for 6-byte MAC address
     */
    static void getMacAddr(uint8_t mac[6]);

    /**
     * @brief Generate a node number from hardware
     *
     * Creates a 32-bit node number derived from the hardware MAC address,
     * using the same algorithm as the main Meshtastic firmware:
     * `(mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]`
     *
     * This ensures that the node number is likely to be unique and
     * consistent across reboots.
     *
     * @return 32-bit node number
     */
    static NodeNum getNodeNum();

    /**
     * @brief Generate a node number from a provided MAC address
     *
     * @param mac 6-byte MAC address
     * @return 32-bit node number
     */
    static NodeNum nodeNumFromMac(const uint8_t mac[6]);

    /**
     * @brief Get the short form of a node number
     *
     * Returns the last 4 hex characters of the node number as a string,
     * useful for display purposes (e.g., "!1234").
     *
     * @param nodeNum Node number
     * @param buffer Output buffer (must be at least 5 bytes)
     */
    static void getShortName(NodeNum nodeNum, char* buffer);

    /**
     * @brief Get the last byte of a node number
     *
     * Used for relay_node and next_hop fields in packet headers.
     *
     * @param nodeNum Node number
     * @return Last byte of node number
     */
    static uint8_t getLastByte(NodeNum nodeNum) {
        return nodeNum & 0xFF;
    }
};

// ============================================================================
// Platform-specific implementations
// ============================================================================

#if defined(ARDUINO)

// ---- ESP32 Family ----
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include <esp_mac.h>

inline void MeshNodeId::getMacAddr(uint8_t mac[6]) {
    #if defined(CONFIG_IDF_TARGET_ESP32C6) && defined(CONFIG_SOC_IEEE802154_SUPPORTED)
    esp_base_mac_addr_get(mac);
    #else
    esp_efuse_mac_get_default(mac);
    #endif
}

// ---- nRF52 Family ----
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)

inline void MeshNodeId::getMacAddr(uint8_t mac[6]) {
    // nRF52 stores device address in FICR registers
    const uint8_t* src = (const uint8_t*)0x100000A4; // NRF_FICR->DEVICEADDR
    mac[5] = src[0];
    mac[4] = src[1];
    mac[3] = src[2];
    mac[2] = src[3];
    mac[1] = src[4];
    mac[0] = src[5] | 0xC0; // Set upper two bits per BLE spec
}

// ---- RP2040/RP2350 ----
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
#include <pico/unique_id.h>

inline void MeshNodeId::getMacAddr(uint8_t mac[6]) {
    pico_unique_board_id_t boardId;
    pico_get_unique_board_id(&boardId);
    // Use last 6 bytes of 8-byte unique ID
    mac[5] = boardId.id[7];
    mac[4] = boardId.id[6];
    mac[3] = boardId.id[5];
    mac[2] = boardId.id[4];
    mac[1] = boardId.id[3];
    mac[0] = boardId.id[2];
}

// ---- STM32WL ----
#elif defined(STM32WLxx) || defined(ARDUINO_ARCH_STM32)

inline void MeshNodeId::getMacAddr(uint8_t mac[6]) {
    // STM32 has a 96-bit unique device ID at specific addresses
    // Using the last 48 bits (6 bytes)
    const uint32_t* uid = (const uint32_t*)0x1FFF7590; // UID base address for STM32WL
    mac[0] = (uid[0] >> 0) & 0xFF;
    mac[1] = (uid[0] >> 8) & 0xFF;
    mac[2] = (uid[0] >> 16) & 0xFF;
    mac[3] = (uid[0] >> 24) & 0xFF;
    mac[4] = (uid[1] >> 0) & 0xFF;
    mac[5] = (uid[1] >> 8) & 0xFF;
}

// ---- Generic/Unknown platform ----
#else

inline void MeshNodeId::getMacAddr(uint8_t mac[6]) {
    // No hardware ID available - generate pseudo-random based on time
    // This is NOT ideal as it won't be consistent across reboots
    // Users should provide their own node number in this case
    uint32_t seed = micros();
    mac[0] = 0x02; // Locally administered MAC
    mac[1] = (seed >> 24) & 0xFF;
    mac[2] = (seed >> 16) & 0xFF;
    mac[3] = (seed >> 8) & 0xFF;
    mac[4] = seed & 0xFF;
    mac[5] = (seed >> 4) ^ 0x55;
}

#endif // Platform detection

#else // Non-Arduino environment

inline void MeshNodeId::getMacAddr(uint8_t mac[6]) {
    // Non-Arduino: User must implement or provide their own node number
    // Fill with placeholder that indicates manual configuration needed
    mac[0] = 0x02;
    mac[1] = 0x00;
    mac[2] = 0x00;
    mac[3] = 0x00;
    mac[4] = 0x00;
    mac[5] = 0x01;
}

#endif // ARDUINO

// ============================================================================
// Platform-independent implementations
// ============================================================================

inline NodeNum MeshNodeId::nodeNumFromMac(const uint8_t mac[6]) {
    // Use last 4 bytes of MAC address to create node number
    // Same algorithm as main Meshtastic firmware
    return ((uint32_t)mac[2] << 24) |
           ((uint32_t)mac[3] << 16) |
           ((uint32_t)mac[4] << 8) |
           ((uint32_t)mac[5]);
}

inline NodeNum MeshNodeId::getNodeNum() {
    uint8_t mac[6];
    getMacAddr(mac);
    return nodeNumFromMac(mac);
}

inline void MeshNodeId::getShortName(NodeNum nodeNum, char* buffer) {
    // Format: last 4 hex digits (e.g., "1a2b")
    const char hex[] = "0123456789abcdef";
    buffer[0] = hex[(nodeNum >> 12) & 0xF];
    buffer[1] = hex[(nodeNum >> 8) & 0xF];
    buffer[2] = hex[(nodeNum >> 4) & 0xF];
    buffer[3] = hex[nodeNum & 0xF];
    buffer[4] = '\0';
}

} // namespace libmeshtastic_leaf
