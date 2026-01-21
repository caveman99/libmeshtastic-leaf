/**
 * @file MeshCrypto.h
 * @brief AES-CTR encryption for Meshtastic channel-based encryption
 *
 * This implements the channel-based encryption used by Meshtastic for
 * encrypting packets with a pre-shared key (PSK).
 */

#pragma once

#include "MeshTypes.h"

// AES from rweather/Crypto library
#include <AES.h>
#include <CTR.h>

namespace libmeshtastic_leaf {

/**
 * @brief AES-CTR encryption engine for channel-based encryption
 *
 * This class handles the AES-CTR encryption/decryption used for
 * Meshtastic channel-based encryption. The nonce is constructed from:
 * - 64-bit packet ID (8 bytes, little-endian)
 * - 32-bit sender node number (4 bytes, little-endian)
 * - 32-bit block counter (4 bytes, starts at zero)
 */
class MeshCrypto {
public:
  MeshCrypto();
  ~MeshCrypto();

  /**
   * @brief Set the encryption key
   * @param key The key to use (16 bytes for AES128, 32 bytes for AES256)
   */
  void setKey(const CryptoKey &key);

  /**
   * @brief Set the encryption key from raw bytes
   * @param keyBytes Key data
   * @param keyLen Key length (16 or 32)
   */
  void setKey(const uint8_t *keyBytes, size_t keyLen);

  /**
   * @brief Check if a valid key is set
   * @return true if encryption is enabled
   */
  bool hasKey() const { return key_.isValid(); }

  /**
   * @brief Encrypt a packet payload in-place
   *
   * For AES-CTR mode, encryption and decryption are the same operation.
   *
   * @param fromNode Sender node number (part of nonce)
   * @param packetId Packet ID (part of nonce)
   * @param bytes Data buffer to encrypt (modified in-place)
   * @param numBytes Number of bytes to encrypt
   */
  void encrypt(NodeNum fromNode, uint64_t packetId, uint8_t *bytes,
               size_t numBytes);

  /**
   * @brief Decrypt a packet payload in-place
   *
   * For AES-CTR mode, this is identical to encrypt().
   *
   * @param fromNode Sender node number (part of nonce)
   * @param packetId Packet ID (part of nonce)
   * @param bytes Data buffer to decrypt (modified in-place)
   * @param numBytes Number of bytes to decrypt
   */
  void decrypt(NodeNum fromNode, uint64_t packetId, uint8_t *bytes,
               size_t numBytes);

  /**
   * @brief Get the current key
   * @return Reference to the current key
   */
  const CryptoKey &getKey() const { return key_; }

private:
  /**
   * @brief Initialize the nonce for encryption/decryption
   * @param fromNode Sender node number
   * @param packetId Packet ID
   */
  void initNonce(NodeNum fromNode, uint64_t packetId);

  /**
   * @brief Perform AES-CTR encryption/decryption
   * @param bytes Data buffer
   * @param numBytes Data length
   */
  void encryptAESCtr(uint8_t *bytes, size_t numBytes);

  CryptoKey key_;       ///< Current encryption key
  uint8_t nonce_[16];   ///< 128-bit nonce
  CTR<AES128> *ctr128_; ///< CTR mode for AES128
  CTR<AES256> *ctr256_; ///< CTR mode for AES256
};

} // namespace libmeshtastic_leaf
