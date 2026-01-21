/**
 * @file MeshChannel.cpp
 * @brief Implementation of channel management
 */

#include "MeshChannel.h"
#include <string.h>

namespace libmeshtastic_leaf {

MeshChannel::MeshChannel() : hash_(0) { memset(name_, 0, sizeof(name_)); }

uint8_t MeshChannel::xorHash(const uint8_t *data, size_t len) {
  uint8_t hash = 0;
  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
  }
  return hash;
}

ChannelHash MeshChannel::calculateHash(const char *name, const CryptoKey &key) {
  // Hash the channel name
  uint8_t h = 0;
  if (name && name[0]) {
    h = xorHash((const uint8_t *)name, strlen(name));
  }

  // XOR with the key bytes
  if (key.isValid()) {
    h ^= xorHash(key.bytes, key.length);
  }

  return h;
}

void MeshChannel::expandPSK(const uint8_t *psk, size_t pskLen,
                            CryptoKey &outKey) {
  // Clear the key
  memset(outKey.bytes, 0, sizeof(outKey.bytes));

  if (pskLen == 0) {
    // No encryption
    outKey.length = 0;
    return;
  }

  if (pskLen == 1) {
    // Single byte: index into default PSK variations
    uint8_t pskIndex = psk[0];

    if (pskIndex == 0) {
      // Index 0 means no encryption
      outKey.length = 0;
      return;
    }

    // Copy default PSK
    memcpy(outKey.bytes, DEFAULT_PSK, sizeof(DEFAULT_PSK));
    outKey.length = sizeof(DEFAULT_PSK);

    // Increment last byte based on index
    // Index 1 = default PSK unchanged
    // Index 2 = last byte + 1
    // Index 3 = last byte + 2, etc.
    if (pskIndex > 1) {
      outKey.bytes[sizeof(DEFAULT_PSK) - 1] += (pskIndex - 1);
    }
    return;
  }

  // Copy the provided PSK
  size_t copyLen =
      (pskLen <= sizeof(outKey.bytes)) ? pskLen : sizeof(outKey.bytes);
  memcpy(outKey.bytes, psk, copyLen);

  // Determine final key length (pad to valid AES size)
  if (pskLen == 0) {
    outKey.length = 0; // No encryption
  } else if (pskLen <= 16) {
    // Pad to AES128
    outKey.length = 16;
  } else {
    // Pad to AES256
    outKey.length = 32;
  }
}

bool MeshChannel::setChannel(const uint8_t *psk, size_t pskLen,
                             const char *name) {
  // Store channel name
  memset(name_, 0, sizeof(name_));
  if (name && name[0]) {
    strncpy(name_, name, MAX_CHANNEL_NAME_LEN);
    name_[MAX_CHANNEL_NAME_LEN] = '\0';
  }

  // Expand PSK to full key
  expandPSK(psk, pskLen, key_);

  // Calculate channel hash
  hash_ = calculateHash(name_, key_);

  // Configure the crypto engine
  crypto_.setKey(key_);

  return true;
}

void MeshChannel::setDefaultChannel() {
  // Default channel uses PSK index 1 (the default public key)
  uint8_t defaultPskIndex = 1;
  setChannel(&defaultPskIndex, 1, "");
}

void MeshChannel::disableEncryption() {
  // PSK index 0 means no encryption
  uint8_t noPsk = 0;
  setChannel(&noPsk, 1, "");
}

} // namespace libmeshtastic_leaf
