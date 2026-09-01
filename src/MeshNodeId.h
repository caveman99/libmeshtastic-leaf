#pragma once

#include "MeshTypes.h"
#include <stdint.h>

namespace libmeshtastic_leaf {

class MeshNodeId {
public:
  // Firmware 2.8 node number: CRC-32 of the 32 public key bytes. Returns 0,
  // which is not usable, when there is nothing to hash.
  static NodeNum nodeNumFromPublicKey(const uint8_t *publicKey, size_t len);

  // The mesh reserves the lowest few numbers and the broadcast address, so a
  // key hashing into either is unusable however good the key is.
  static bool isUsableNodeNum(NodeNum nodeNum) {
    return nodeNum >= NUM_RESERVED_NODE_NUMS && nodeNum != BROADCAST_ADDR;
  }

  // The four hex digits a node is known by, as in "!1a2b". buffer needs 5
  // bytes.
  static void getShortName(NodeNum nodeNum, char *buffer);

  static uint8_t getLastByte(NodeNum nodeNum) { return nodeNum & 0xFF; }
};

inline NodeNum MeshNodeId::nodeNumFromPublicKey(const uint8_t *publicKey,
                                                size_t len) {
  if (publicKey == nullptr || len == 0) {
    return 0;
  }

  // CRC-32, the reflected polynomial, computed a nibble at a time so the
  // table costs 64 bytes rather than a kilobyte.
  static const uint32_t kNibble[16] = {
      0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
      0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
      0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
      0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};

  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= publicKey[i];
    crc = (crc >> 4) ^ kNibble[crc & 0x0F];
    crc = (crc >> 4) ^ kNibble[crc & 0x0F];
  }
  return crc ^ 0xFFFFFFFFu;
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
