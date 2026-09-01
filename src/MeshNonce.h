#pragma once

#include "MeshTypes.h"
#include <stdint.h>
#include <string.h>

namespace libmeshtastic_leaf {

// One builder for both ciphers, so their nonces cannot drift apart. Layout in
// ARCHITECTURE.md; a wrong one encrypts and nobody can ever decrypt.
inline void buildMeshNonce(NodeNum fromNode, uint64_t packetId,
                           uint32_t extraNonce, uint8_t nonce[16]) {
  memset(nonce, 0, 16);

  for (uint8_t i = 0; i < 8; i++) {
    nonce[i] = (uint8_t)(packetId >> (8 * i));
  }
  for (uint8_t i = 0; i < 4; i++) {
    nonce[8 + i] = (uint8_t)(fromNode >> (8 * i));
  }
  if (extraNonce != 0) {
    for (uint8_t i = 0; i < 4; i++) {
      nonce[4 + i] = (uint8_t)(extraNonce >> (8 * i));
    }
  }
}

} // namespace libmeshtastic_leaf
