
#pragma once

#include "MeshTypes.h"
#include <stdint.h>

#if defined(ARDUINO)
// The platform branches below call micros() directly, so pull Arduino.h in
// here rather than rely on the sketch having included it.
#include <Arduino.h>
#endif

namespace libmeshtastic_leaf {

class MeshNodeId {
public:
  static void getMacAddr(uint8_t mac[6]);

  static NodeNum getNodeNum();

  static NodeNum nodeNumFromMac(const uint8_t mac[6]);

  static void getShortName(NodeNum nodeNum, char *buffer);

  static uint8_t getLastByte(NodeNum nodeNum) { return nodeNum & 0xFF; }
};

#if defined(ARDUINO)

// ---- ESP32 Family ----
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include <esp_mac.h>

inline void MeshNodeId::getMacAddr(uint8_t mac[6]) {
#if defined(CONFIG_IDF_TARGET_ESP32C6) &&                                      \
    defined(CONFIG_SOC_IEEE802154_SUPPORTED)
  esp_base_mac_addr_get(mac);
#else
  esp_efuse_mac_get_default(mac);
#endif
}

// ---- nRF52 Family ----
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)

inline void MeshNodeId::getMacAddr(uint8_t mac[6]) {
  const uint8_t *src = (const uint8_t *)0x100000A4; // NRF_FICR->DEVICEADDR
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
  // Last 48 bits of the 96-bit unique device ID.
  const uint32_t *uid =
      (const uint32_t *)0x1FFF7590; // UID base address for STM32WL
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
  // No hardware ID here, so this is not stable across reboots. Callers on
  // such a platform should supply their own node number.
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
  // Placeholder; a non-Arduino host must supply its own node number.
  mac[0] = 0x02;
  mac[1] = 0x00;
  mac[2] = 0x00;
  mac[3] = 0x00;
  mac[4] = 0x00;
  mac[5] = 0x01;
}

#endif // ARDUINO

inline NodeNum MeshNodeId::nodeNumFromMac(const uint8_t mac[6]) {
  // Node number is the last 4 bytes of the MAC.
  return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) |
         ((uint32_t)mac[4] << 8) | ((uint32_t)mac[5]);
}

inline NodeNum MeshNodeId::getNodeNum() {
  uint8_t mac[6];
  getMacAddr(mac);
  return nodeNumFromMac(mac);
}

inline void MeshNodeId::getShortName(NodeNum nodeNum, char *buffer) {
  const char hex[] = "0123456789abcdef";
  buffer[0] = hex[(nodeNum >> 12) & 0xF];
  buffer[1] = hex[(nodeNum >> 8) & 0xF];
  buffer[2] = hex[(nodeNum >> 4) & 0xF];
  buffer[3] = hex[nodeNum & 0xF];
  buffer[4] = '\0';
}

} // namespace libmeshtastic_leaf
