
#include "MeshCrypto.h"
#include "MeshNonce.h"
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

void MeshCrypto::initNonce(NodeNum fromNode, uint64_t packetId) {
  buildMeshNonce(fromNode, packetId, 0, nonce_);
}

void MeshCrypto::encryptAESCtr(uint8_t *bytes, size_t numBytes) {
  delete ctr128_;
  delete ctr256_;
  ctr128_ = nullptr;
  ctr256_ = nullptr;

  if (key_.length == 16) {
    ctr128_ = new CTR<AES128>();
    ctr128_->setKey(key_.bytes, 16);
    ctr128_->setIV(nonce_, 16);
    ctr128_->setCounterSize(4);

    ctr128_->encrypt(bytes, bytes, numBytes);
  } else if (key_.length == 32) {
    ctr256_ = new CTR<AES256>();
    ctr256_->setKey(key_.bytes, 32);
    ctr256_->setIV(nonce_, 16);
    ctr256_->setCounterSize(4);

    ctr256_->encrypt(bytes, bytes, numBytes);
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
  // CTR is symmetric.
  encrypt(fromNode, packetId, bytes, numBytes);
}

} // namespace libmeshtastic_leaf
