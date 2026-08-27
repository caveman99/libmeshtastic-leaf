
#pragma once

#include "MeshTypes.h"

#include <AES.h>
#include <CTR.h>

namespace libmeshtastic_leaf {

class MeshCrypto {
public:
  MeshCrypto();
  ~MeshCrypto();

  void setKey(const CryptoKey &key);

  void setKey(const uint8_t *keyBytes, size_t keyLen);

  bool hasKey() const { return key_.isValid(); }

  void encrypt(NodeNum fromNode, uint64_t packetId, uint8_t *bytes,
               size_t numBytes);

  void decrypt(NodeNum fromNode, uint64_t packetId, uint8_t *bytes,
               size_t numBytes);

  const CryptoKey &getKey() const { return key_; }

private:
  void initNonce(NodeNum fromNode, uint64_t packetId);

  void encryptAESCtr(uint8_t *bytes, size_t numBytes);

  CryptoKey key_;       ///< Current encryption key
  uint8_t nonce_[16];   ///< 128-bit nonce
  CTR<AES128> *ctr128_; ///< CTR mode for AES128
  CTR<AES256> *ctr256_; ///< CTR mode for AES256
};

} // namespace libmeshtastic_leaf
