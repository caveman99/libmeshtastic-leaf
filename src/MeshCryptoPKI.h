/**
 * @file MeshCryptoPKI.h
 * @brief PKI encryption using Curve25519 and AES-CCM
 *
 * This implements the public key infrastructure (PKI) encryption used by
 * Meshtastic for direct node-to-node encrypted messages. It uses:
 * - Curve25519 for ECDH key exchange
 * - SHA256 to derive the shared encryption key
 * - AES-CCM for authenticated encryption
 */

#pragma once

#include "MeshTypes.h"

namespace libmeshtastic_leaf {

/**
 * @brief PKI encryption engine using Curve25519 + AES-CCM
 *
 * This class handles PKI encryption/decryption for Meshtastic.
 * The encryption flow is:
 * 1. Generate random extraNonce (4 bytes)
 * 2. ECDH: shared_key = Curve25519(remotePublic, myPrivate)
 * 3. Hash: shared_key = SHA256(shared_key)
 * 4. Build nonce: packetId[8] | fromNode[4] | extraNonce[4]
 * 5. Encrypt: AES-CCM(shared_key, nonce, plaintext) -> ciphertext + auth[8]
 * 6. Append extraNonce[4] after auth tag
 * 7. Output: ciphertext | auth[8] | extraNonce[4]
 *
 * PKI packets are identified by: channel == 0 AND to != BROADCAST
 * PKI overhead is 12 bytes (8 byte auth tag + 4 byte extraNonce)
 */
class MeshCryptoPKI {
public:
  MeshCryptoPKI();
  ~MeshCryptoPKI();

  /**
   * @brief Generate a new Curve25519 keypair
   *
   * Creates a new random private key and derives the public key.
   * The keys should be stored by the caller for later use.
   *
   * @param pubKey Output buffer for 32-byte public key
   * @param privKey Output buffer for 32-byte private key
   */
  static void generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]);

  /**
   * @brief Regenerate public key from existing private key
   *
   * Derives the public key from a previously stored private key.
   *
   * @param pubKey Output buffer for 32-byte public key
   * @param privKey Input 32-byte private key
   * @return true if successful, false if the private key is invalid
   */
  static bool regeneratePublicKey(uint8_t pubKey[32],
                                  const uint8_t privKey[32]);

  /**
   * @brief Set this node's private key
   *
   * Sets the private key to use for encryption and decryption operations.
   *
   * @param privKey 32-byte private key
   */
  void setPrivateKey(const uint8_t privKey[32]);

  /**
   * @brief Check if a private key is set
   * @return true if a private key has been configured
   */
  bool hasPrivateKey() const;

  /**
   * @brief Encrypt a message using PKI
   *
   * Encrypts plaintext for a specific destination node using their public key.
   *
   * @param toNode Destination node number
   * @param fromNode Source node number (this node)
   * @param remotePublic Destination node's 32-byte public key
   * @param packetId Packet ID (part of nonce)
   * @param plain Plaintext input buffer
   * @param plainLen Plaintext length
   * @param crypt Output buffer (must be plainLen + 12 bytes)
   * @return true if encryption succeeded, false otherwise
   */
  bool encrypt(NodeNum toNode, NodeNum fromNode, const uint8_t remotePublic[32],
               uint64_t packetId, const uint8_t *plain, size_t plainLen,
               uint8_t *crypt);

  /**
   * @brief Decrypt a PKI message
   *
   * Decrypts ciphertext from a specific sender using their public key.
   *
   * @param fromNode Sender node number
   * @param remotePublic Sender's 32-byte public key
   * @param packetId Packet ID (part of nonce)
   * @param crypt Ciphertext input buffer (includes auth tag + extraNonce)
   * @param cryptLen Ciphertext length (including 12 bytes overhead)
   * @param plain Output plaintext buffer (cryptLen - 12 bytes)
   * @return true if decryption and verification succeeded, false otherwise
   */
  bool decrypt(NodeNum fromNode, const uint8_t remotePublic[32],
               uint64_t packetId, const uint8_t *crypt, size_t cryptLen,
               uint8_t *plain);

  /**
   * @brief Get the PKI overhead in bytes
   * @return 12 (8 byte auth tag + 4 byte extraNonce)
   */
  static constexpr size_t getOverhead() { return MESHTASTIC_PKC_OVERHEAD; }

private:
  /**
   * @brief Perform ECDH key exchange and derive shared key
   * @param remotePublic Remote node's public key
   * @param sharedKey Output 32-byte shared key (SHA256 of ECDH result)
   * @return true if successful, false if the public key is invalid
   */
  bool deriveSharedKey(const uint8_t remotePublic[32], uint8_t sharedKey[32]);

  /**
   * @brief Hash data using SHA256
   * @param data Data to hash (modified in-place)
   * @param len Data length
   */
  static void sha256Hash(uint8_t *data, size_t len);

  /**
   * @brief Initialize the nonce for PKI encryption
   * @param fromNode Source node number
   * @param packetId Packet ID
   * @param extraNonce Random extra nonce
   * @param nonce Output 13-byte nonce
   */
  static void initNonce(NodeNum fromNode, uint64_t packetId,
                        uint32_t extraNonce, uint8_t nonce[13]);

  uint8_t privateKey_[32]; ///< This node's private key
  bool hasKey_;            ///< True if private key is set
};

} // namespace libmeshtastic_leaf
