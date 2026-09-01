
#pragma once

#include "MeshCrypto.h"
#include "MeshTypes.h"

namespace libmeshtastic_leaf {

constexpr size_t MAX_CHANNEL_NAME_LEN = 12;

class MeshChannel {
public:
  MeshChannel();
  ~MeshChannel() = default;

  bool setChannel(const uint8_t *psk, size_t pskLen, const char *name = "");

  void setDefaultChannel();

  ChannelHash getHash() const { return hash_; }

  bool isEncrypted() const { return key_.isValid(); }

  const char *getName() const { return name_; }

  MeshCrypto &getCrypto() { return crypto_; }
  const MeshCrypto &getCrypto() const { return crypto_; }

  static ChannelHash calculateHash(const char *name, const CryptoKey &key);

private:
  static void expandPSK(const uint8_t *psk, size_t pskLen, CryptoKey &outKey);

  static uint8_t xorHash(const uint8_t *data, size_t len);

  char name_[MAX_CHANNEL_NAME_LEN + 1]; ///< Channel name
  CryptoKey key_;                       ///< Expanded encryption key
  ChannelHash hash_;                    ///< Calculated channel hash
  MeshCrypto crypto_;                   ///< Crypto engine instance
};

} // namespace libmeshtastic_leaf
