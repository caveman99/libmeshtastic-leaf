
#include "MeshCryptoPKI.h"
#include "MeshEntropy.h"
#include "MeshLowEntropyKeys.h"
#include "MeshNodeId.h"
#include "MeshNonce.h"
#include "third_party/aes-ccm.h"

// Curve25519::isWeakPoint() is private unless this is defined before the
// header is pulled in.
#define TEST_CURVE25519_FIELD_OPS

#include <Curve25519.h>
#include <RNG.h>
#include <SHA256.h>
#include <XEdDSA.h>
#include <string.h>

namespace libmeshtastic_leaf {

MeshCryptoPKI::MeshCryptoPKI() : hasKey_(false) {
  memset(signPrivateKey_, 0, sizeof(signPrivateKey_));
  memset(signPublicKey_, 0, sizeof(signPublicKey_));
  memset(privateKey_, 0, sizeof(privateKey_));
}

MeshCryptoPKI::~MeshCryptoPKI() {
  memset(privateKey_, 0, sizeof(privateKey_));
  memset(signPrivateKey_, 0, sizeof(signPrivateKey_));
  memset(signPublicKey_, 0, sizeof(signPublicKey_));
}

bool MeshCryptoPKI::isUsablePublicKey(const uint8_t pubKey[32]) {
  // An all zero key means generation never ran.
  bool allZero = true;
  for (int i = 0; i < 32; i++) {
    if (pubKey[i] != 0) {
      allZero = false;
      break;
    }
  }
  if (allZero) {
    return false;
  }

  if (Curve25519::isWeakPoint(pubKey)) {
    return false;
  }

  // The node number comes from the key, so a key that hashes to a reserved
  // number is unusable however good it is.
  if (!MeshNodeId::isUsableNodeNum(
          MeshNodeId::nodeNumFromPublicKey(pubKey, 32))) {
    return false;
  }

  // Keys from broken random number generators are published, so a node using
  // one has no privacy. The firmware regenerates on a match; so do we.
  uint8_t hash[32];
  memcpy(hash, pubKey, 32);
  sha256Hash(hash, 32);
  for (uint16_t i = 0; i < NUM_LOW_ENTROPY_KEY_HASHES; i++) {
    if (memcmp(hash, LOW_ENTROPY_KEY_HASHES[i], 32) == 0) {
      return false;
    }
  }

  return true;
}

bool MeshCryptoPKI::generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]) {
  // A rejection is astronomically unlikely, so a handful of attempts is
  // plenty; exhausting them means the entropy source is broken.
  for (int attempt = 0; attempt < 8; attempt++) {
    Curve25519::dh1(pubKey, privKey);
    if (isUsablePublicKey(pubKey)) {
      return true;
    }
  }

  memset(pubKey, 0, 32);
  memset(privKey, 0, 32);
  return false;
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
  bool allZero = true;
  for (int i = 0; i < 32; i++) {
    if (privKey[i] != 0) {
      allZero = false;
      break;
    }
  }
  if (allZero) {
    return;
  }

  memcpy(privateKey_, privKey, 32);
  hasKey_ = true;

  // Signing uses the Edwards form of the same key, derived once here rather
  // than on every packet.
  uint8_t curve[32];
  memcpy(curve, privKey, 32);
  XEdDSA::priv_curve_to_ed_keys(curve, signPrivateKey_, signPublicKey_);
  memset(curve, 0, sizeof(curve));
}

bool MeshCryptoPKI::signPayload(NodeNum fromNode, PacketId packetId,
                                uint32_t portnum, const uint8_t *payload,
                                size_t payloadLen, uint8_t signature[64]) {
  if (!hasKey_) {
    return false;
  }

  uint8_t buf[12 + MAX_ENCRYPTED_PAYLOAD];
  if (payloadLen > sizeof(buf) - 12) {
    return false;
  }
  memcpy(buf, &fromNode, 4);
  memcpy(buf + 4, &packetId, 4);
  memcpy(buf + 8, &portnum, 4);
  memcpy(buf + 12, payload, payloadLen);

  // XEdDSA::sign folds signature[0..31] into the nonce as the spec's random Z,
  // so it has to start with entropy rather than whatever was in the buffer.
  fillRandom(signature, 32);
  XEdDSA::sign(signature, signPrivateKey_, signPublicKey_, buf,
               12 + payloadLen);
  return true;
}

// RFC 7748 birational map, Curve25519 u to Ed25519 y. XEdDSA normalises the
// sign bit to zero when signing, so it is cleared here rather than recovered.
static void curveToEdPublic(const uint8_t curvePub[32], uint8_t edPub[32]) {
  fe u, y, one, uMinusOne, uPlusOne, uPlusOneInv;

  fe_frombytes(u, curvePub);
  fe_1(one);
  fe_sub(uMinusOne, u, one);
  fe_add(uPlusOne, u, one);
  fe_invert(uPlusOneInv, uPlusOne);
  fe_mul(y, uMinusOne, uPlusOneInv);
  fe_tobytes(edPub, y);
  edPub[31] &= 0x7F;
}

bool MeshCryptoPKI::verifyPayload(const uint8_t pubKey[32], NodeNum fromNode,
                                  PacketId packetId, uint32_t portnum,
                                  const uint8_t *payload, size_t payloadLen,
                                  const uint8_t signature[64]) {
  uint8_t buf[12 + MAX_ENCRYPTED_PAYLOAD];
  if (pubKey == nullptr || signature == nullptr ||
      payloadLen > sizeof(buf) - 12) {
    return false;
  }
  memcpy(buf, &fromNode, 4);
  memcpy(buf + 4, &packetId, 4);
  memcpy(buf + 8, &portnum, 4);
  if (payloadLen > 0) {
    memcpy(buf + 12, payload, payloadLen);
  }

  uint8_t edPub[32];
  curveToEdPublic(pubKey, edPub);
  return XEdDSA::verify(signature, edPub, buf, 12 + payloadLen);
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
  uint8_t full[16];
  buildMeshNonce(fromNode, packetId, extraNonce, full);
  memcpy(nonce, full, 13);
}

bool MeshCryptoPKI::encrypt(NodeNum toNode, NodeNum fromNode,
                            const uint8_t remotePublic[32], uint64_t packetId,
                            const uint8_t *plain, size_t plainLen,
                            uint8_t *crypt) {
  if (!hasKey_ || plain == nullptr || crypt == nullptr ||
      plainLen + MESHTASTIC_PKC_OVERHEAD > MAX_ENCRYPTED_PAYLOAD) {
    return false;
  }

  uint8_t sharedKey[32];
  if (!deriveSharedKey(remotePublic, sharedKey)) {
    return false;
  }

  const uint32_t extraNonce = randomUint32();

  uint8_t nonce[13];
  initNonce(fromNode, packetId, extraNonce, nonce);

  uint8_t *auth = crypt + plainLen; // Auth tag position

  int result = aes_ccm_ae(sharedKey, 32, nonce, 8, plain, plainLen, nullptr,
                          0, // No additional authenticated data
                          crypt, auth);

  for (uint8_t i = 0; i < 4; i++) {
    auth[8 + i] = (uint8_t)(extraNonce >> (8 * i));
  }

  memset(sharedKey, 0, sizeof(sharedKey));

  return (result == 0);
}

bool MeshCryptoPKI::decrypt(NodeNum fromNode, const uint8_t remotePublic[32],
                            uint64_t packetId, const uint8_t *crypt,
                            size_t cryptLen, uint8_t *plain) {
  if (!hasKey_ || crypt == nullptr || plain == nullptr) {
    return false;
  }

  if (cryptLen < MESHTASTIC_PKC_OVERHEAD || cryptLen > MAX_ENCRYPTED_PAYLOAD) {
    return false;
  }

  size_t plainLen = cryptLen - MESHTASTIC_PKC_OVERHEAD;

  const uint8_t *auth = crypt + plainLen;
  uint32_t extraNonce = 0;
  for (uint8_t i = 0; i < 4; i++) {
    extraNonce |= (uint32_t)auth[8 + i] << (8 * i);
  }

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
