
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
  uint8_t h = 0;
  if (name && name[0]) {
    h = xorHash(reinterpret_cast<const uint8_t *>(name), strlen(name));
  }

  if (key.isValid()) {
    h ^= xorHash(key.bytes, key.length);
  }

  return h;
}

void MeshChannel::expandPSK(const uint8_t *psk, size_t pskLen,
                            CryptoKey &outKey) {
  memset(outKey.bytes, 0, sizeof(outKey.bytes));

  if (pskLen == 0) {
    outKey.length = 0;
    return;
  }

  if (pskLen == 1) {
    uint8_t pskIndex = psk[0];

    if (pskIndex == 0) {
      outKey.length = 0;
      return;
    }

    memcpy(outKey.bytes, DEFAULT_PSK, sizeof(DEFAULT_PSK));
    outKey.length = sizeof(DEFAULT_PSK);

    if (pskIndex > 1) {
      outKey.bytes[sizeof(DEFAULT_PSK) - 1] += (pskIndex - 1);
    }
    return;
  }

  size_t copyLen =
      (pskLen <= sizeof(outKey.bytes)) ? pskLen : sizeof(outKey.bytes);
  memcpy(outKey.bytes, psk, copyLen);

  outKey.length = pskLen <= 16 ? 16 : 32;
}

bool MeshChannel::setChannel(const uint8_t *psk, size_t pskLen,
                             const char *name) {
  memset(name_, 0, sizeof(name_));
  if (name && name[0]) {
    strncpy(name_, name, MAX_CHANNEL_NAME_LEN);
    name_[MAX_CHANNEL_NAME_LEN] = '\0';
  }

  expandPSK(psk, pskLen, key_);

  hash_ = calculateHash(name_, key_);

  crypto_.setKey(key_);

  return true;
}

void MeshChannel::setDefaultChannel() {
  // Default channel uses PSK index 1 (the default public key)
  uint8_t defaultPskIndex = 1;
  setChannel(&defaultPskIndex, 1, "");
}

} // namespace libmeshtastic_leaf
