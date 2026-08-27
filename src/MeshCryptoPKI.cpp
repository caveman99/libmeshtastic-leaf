
#include "MeshCryptoPKI.h"
#include "aes-ccm.h"

// Curve25519::isWeakPoint() is private unless this is defined before the
// header is pulled in. The firmware does the same thing in CryptoEngine.h.
#define TEST_CURVE25519_FIELD_OPS

#include <Curve25519.h>
#include <RNG.h>
#include <SHA256.h>
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <stdlib.h>
#define random() rand()
#endif

namespace libmeshtastic_leaf {

MeshCryptoPKI::MeshCryptoPKI() : hasKey_(false) {
  memset(privateKey_, 0, sizeof(privateKey_));
}

MeshCryptoPKI::~MeshCryptoPKI() { memset(privateKey_, 0, sizeof(privateKey_)); }

void MeshCryptoPKI::generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]) {
  // meshtastic/Crypto renames rweather's RNG global to CryptRNG; some
  // toolchains (STM32, RP2040) declare an unnamespaced RNG and collide.
  CryptRNG.begin("libmeshtastic_leaf");

  uint32_t noise = random();
  CryptRNG.stir((uint8_t *)&noise, sizeof(noise));

  Curve25519::dh1(pubKey, privKey);
}

bool MeshCryptoPKI::regeneratePublicKey(uint8_t pubKey[32],
                                        const uint8_t privKey[32]) {
  bool allZero = true;
  for (int i = 0; i < 32; i++) {
    if (privKey[i] != 0) {
      allZero = false;
      break;
    }
  }
  if (allZero) {
    return false;
  }

  Curve25519::eval(pubKey, privKey, nullptr);

  if (Curve25519::isWeakPoint(pubKey)) {
    memset(pubKey, 0, 32);
    return false;
  }

  return true;
}

void MeshCryptoPKI::setPrivateKey(const uint8_t privKey[32]) {
  memcpy(privateKey_, privKey, 32);
  hasKey_ = true;
}

bool MeshCryptoPKI::hasPrivateKey() const { return hasKey_; }

void MeshCryptoPKI::sha256Hash(uint8_t *data, size_t len) {
  SHA256 hash;
  hash.reset();

  const size_t chunkSize = 16;
  size_t pos = 0;
  while (pos < len) {
    size_t remaining = len - pos;
    size_t toProcess = (remaining > chunkSize) ? chunkSize : remaining;
    hash.update(data + pos, toProcess);
    pos += toProcess;
  }

  hash.finalize(data, 32);
}

bool MeshCryptoPKI::deriveSharedKey(const uint8_t remotePublic[32],
                                    uint8_t sharedKey[32]) {
  if (!hasKey_) {
    return false;
  }

  // dh2() consumes both buffers in place, so hand it copies.
  memcpy(sharedKey, remotePublic, 32);

  uint8_t localPriv[32];
  memcpy(localPriv, privateKey_, 32);

  if (!Curve25519::dh2(sharedKey, localPriv)) {
    memset(sharedKey, 0, 32);
    memset(localPriv, 0, 32);
    return false;
  }

  memset(localPriv, 0, 32);

  sha256Hash(sharedKey, 32);

  return true;
}

void MeshCryptoPKI::initNonce(NodeNum fromNode, uint64_t packetId,
                              uint32_t extraNonce, uint8_t nonce[13]) {

  memset(nonce, 0, 13);
  memcpy(nonce, &packetId, sizeof(uint64_t));
  memcpy(nonce + sizeof(uint64_t), &fromNode, sizeof(uint32_t));
  nonce[12] = extraNonce & 0xFF;
}

bool MeshCryptoPKI::encrypt(NodeNum toNode, NodeNum fromNode,
                            const uint8_t remotePublic[32], uint64_t packetId,
                            const uint8_t *plain, size_t plainLen,
                            uint8_t *crypt) {
  if (!hasKey_ || plain == nullptr || crypt == nullptr) {
    return false;
  }

  uint8_t sharedKey[32];
  if (!deriveSharedKey(remotePublic, sharedKey)) {
    return false;
  }

  uint32_t extraNonce = random();

  uint8_t nonce[13];
  initNonce(fromNode, packetId, extraNonce, nonce);

  uint8_t *auth = crypt + plainLen; // Auth tag position

  int result = aes_ccm_ae(sharedKey, 32, nonce, 8, plain, plainLen, nullptr,
                          0, // No additional authenticated data
                          crypt, auth);

  memcpy(auth + 8, &extraNonce, sizeof(uint32_t));

  memset(sharedKey, 0, sizeof(sharedKey));

  return (result == 0);
}

bool MeshCryptoPKI::decrypt(NodeNum fromNode, const uint8_t remotePublic[32],
                            uint64_t packetId, const uint8_t *crypt,
                            size_t cryptLen, uint8_t *plain) {
  if (!hasKey_ || crypt == nullptr || plain == nullptr) {
    return false;
  }

  if (cryptLen < MESHTASTIC_PKC_OVERHEAD) {
    return false;
  }

  size_t plainLen = cryptLen - MESHTASTIC_PKC_OVERHEAD;

  const uint8_t *auth = crypt + plainLen;
  uint32_t extraNonce;
  memcpy(&extraNonce, auth + 8, sizeof(uint32_t));

  uint8_t sharedKey[32];
  if (!deriveSharedKey(remotePublic, sharedKey)) {
    return false;
  }

  uint8_t nonce[13];
  initNonce(fromNode, packetId, extraNonce, nonce);

  bool result = aes_ccm_ad(sharedKey, 32, nonce, 8, crypt, plainLen, nullptr,
                           0, // No additional authenticated data
                           auth, plain);

  memset(sharedKey, 0, sizeof(sharedKey));

  return result;
}

} // namespace libmeshtastic_leaf
