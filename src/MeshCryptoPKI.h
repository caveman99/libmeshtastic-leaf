
#pragma once

#include "MeshTypes.h"

namespace libmeshtastic_leaf {

class MeshCryptoPKI {
public:
  MeshCryptoPKI();
  ~MeshCryptoPKI();

  // A keypair the mesh will accept, retried until usable. False means the
  // entropy source is broken; do not fall back to the buffer contents.
  static bool generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]);

  // Whether a public key is one the mesh will accept, applying the same rules
  // as generateKeyPair. Use it on a key loaded back from storage.
  static bool isUsablePublicKey(const uint8_t pubKey[32]);

  static bool regeneratePublicKey(uint8_t pubKey[32],
                                  const uint8_t privKey[32]);

  void setPrivateKey(const uint8_t privKey[32]);

  // XEdDSA over [fromNode | packetId | portnum | payload], all little endian.
  // A first contact NodeInfo is dropped by the firmware without this.
  bool signPayload(NodeNum fromNode, PacketId packetId, uint32_t portnum,
                   const uint8_t *payload, size_t payloadLen,
                   uint8_t signature[64]);

  // The other side of signPayload. No node database here, so the caller picks
  // the key to trust. See the PKI section of README.md.
  static bool verifyPayload(const uint8_t pubKey[32], NodeNum fromNode,
                            PacketId packetId, uint32_t portnum,
                            const uint8_t *payload, size_t payloadLen,
                            const uint8_t signature[64]);

  // The AES-CCM nonce. Public so a test can pin the layout.
  static void initNonce(NodeNum fromNode, uint64_t packetId,
                        uint32_t extraNonce, uint8_t nonce[13]);

  bool hasPrivateKey() const;

  bool encrypt(NodeNum toNode, NodeNum fromNode, const uint8_t remotePublic[32],
               uint64_t packetId, const uint8_t *plain, size_t plainLen,
               uint8_t *crypt);

  bool decrypt(NodeNum fromNode, const uint8_t remotePublic[32],
               uint64_t packetId, const uint8_t *crypt, size_t cryptLen,
               uint8_t *plain);

private:
  bool deriveSharedKey(const uint8_t remotePublic[32], uint8_t sharedKey[32]);

  static void sha256Hash(uint8_t *data, size_t len);

  uint8_t privateKey_[32]; ///< This node's private key
  uint8_t signPrivateKey_[64];
  uint8_t signPublicKey_[32];
  bool hasKey_; ///< True if private key is set
};

} // namespace libmeshtastic_leaf
