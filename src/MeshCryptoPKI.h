
#pragma once

#include "MeshTypes.h"

namespace libmeshtastic_leaf {

class MeshCryptoPKI {
public:
  MeshCryptoPKI();
  ~MeshCryptoPKI();

  static void generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]);

  static bool regeneratePublicKey(uint8_t pubKey[32],
                                  const uint8_t privKey[32]);

  void setPrivateKey(const uint8_t privKey[32]);

  bool hasPrivateKey() const;

  bool encrypt(NodeNum toNode, NodeNum fromNode, const uint8_t remotePublic[32],
               uint64_t packetId, const uint8_t *plain, size_t plainLen,
               uint8_t *crypt);

  bool decrypt(NodeNum fromNode, const uint8_t remotePublic[32],
               uint64_t packetId, const uint8_t *crypt, size_t cryptLen,
               uint8_t *plain);

  static constexpr size_t getOverhead() { return MESHTASTIC_PKC_OVERHEAD; }

private:
  bool deriveSharedKey(const uint8_t remotePublic[32], uint8_t sharedKey[32]);

  static void sha256Hash(uint8_t *data, size_t len);

  static void initNonce(NodeNum fromNode, uint64_t packetId,
                        uint32_t extraNonce, uint8_t nonce[13]);

  uint8_t privateKey_[32]; ///< This node's private key
  bool hasKey_;            ///< True if private key is set
};

} // namespace libmeshtastic_leaf
