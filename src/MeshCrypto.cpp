/**
 * @file MeshCrypto.cpp
 * @brief Implementation of AES-CTR encryption for Meshtastic
 */

#include "MeshCrypto.h"
#include <string.h>

namespace libmeshtastic_leaf {

MeshCrypto::MeshCrypto() : ctr128_(nullptr), ctr256_(nullptr) {
  memset(nonce_, 0, sizeof(nonce_));
}

MeshCrypto::~MeshCrypto() {
  delete ctr128_;
  delete ctr256_;
}

void MeshCrypto::setKey(const CryptoKey &key) { key_ = key; }

void MeshCrypto::setKey(const uint8_t *keyBytes, size_t keyLen) {
  if (keyLen == 0) {
    key_.length = 0;
    return;
  }

  memset(key_.bytes, 0, sizeof(key_.bytes));
  if (keyLen <= sizeof(key_.bytes)) {
    memcpy(key_.bytes, keyBytes, keyLen);
  }

  // Normalize key length to valid AES sizes
  if (keyLen == 0) {
    key_.length = 0; // No encryption
  } else if (keyLen <= 16) {
    key_.length = 16; // AES128
  } else {
    key_.length = 32; // AES256
  }
}

void MeshCrypto::initNonce(NodeNum fromNode, uint64_t packetId) {
  // The nonce is constructed as:
  // - bytes 0-7: packetId (64-bit, little-endian)
  // - bytes 8-11: fromNode (32-bit, little-endian)
  // - bytes 12-15: block counter (starts at zero)
  memset(nonce_, 0, sizeof(nonce_));
  memcpy(nonce_, &packetId, sizeof(uint64_t));
  memcpy(nonce_ + sizeof(uint64_t), &fromNode, sizeof(uint32_t));
  // Block counter (last 4 bytes) starts at 0
}

void MeshCrypto::encryptAESCtr(uint8_t *bytes, size_t numBytes) {
  // Clean up previous instances
  delete ctr128_;
  delete ctr256_;
  ctr128_ = nullptr;
  ctr256_ = nullptr;

  // Create appropriate CTR instance based on key size
  if (key_.length == 16) {
    ctr128_ = new CTR<AES128>();
    ctr128_->setKey(key_.bytes, 16);
    ctr128_->setIV(nonce_, 16);
    ctr128_->setCounterSize(4);

    // Use a scratch buffer to avoid in-place issues
    uint8_t scratch[256];
    if (numBytes <= sizeof(scratch)) {
      memcpy(scratch, bytes, numBytes);
      memset(scratch + numBytes, 0, sizeof(scratch) - numBytes);
      ctr128_->encrypt(bytes, scratch, numBytes);
    }
  } else if (key_.length == 32) {
    ctr256_ = new CTR<AES256>();
    ctr256_->setKey(key_.bytes, 32);
    ctr256_->setIV(nonce_, 16);
    ctr256_->setCounterSize(4);

    // Use a scratch buffer to avoid in-place issues
    uint8_t scratch[256];
    if (numBytes <= sizeof(scratch)) {
      memcpy(scratch, bytes, numBytes);
      memset(scratch + numBytes, 0, sizeof(scratch) - numBytes);
      ctr256_->encrypt(bytes, scratch, numBytes);
    }
  }
}

void MeshCrypto::encrypt(NodeNum fromNode, uint64_t packetId, uint8_t *bytes,
                         size_t numBytes) {
  if (!key_.isValid() || numBytes == 0) {
    return; // No encryption if key is not set
  }
  initNonce(fromNode, packetId);
  encryptAESCtr(bytes, numBytes);
}

void MeshCrypto::decrypt(NodeNum fromNode, uint64_t packetId, uint8_t *bytes,
                         size_t numBytes) {
  // For CTR mode, decryption is the same operation as encryption
  encrypt(fromNode, packetId, bytes, numBytes);
}

} // namespace libmeshtastic_leaf
