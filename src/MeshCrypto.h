
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

  void encrypt(NodeNum fromNode, uint64_t packetId, uint8_t *bytes,
               size_t numBytes);

  void decrypt(NodeNum fromNode, uint64_t packetId, uint8_t *bytes,
               size_t numBytes);

private:
  void initNonce(NodeNum fromNode, uint64_t packetId);

  void encryptAESCtr(uint8_t *bytes, size_t numBytes);

  CryptoKey key_;       ///< Current encryption key
  uint8_t nonce_[16];   ///< 128-bit nonce
  CTR<AES128> *ctr128_; ///< CTR mode for AES128
  CTR<AES256> *ctr256_; ///< CTR mode for AES256
};

} // namespace libmeshtastic_leaf
