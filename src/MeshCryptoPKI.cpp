/**
 * @file MeshCryptoPKI.cpp
 * @brief Implementation of PKI encryption using Curve25519 + AES-CCM
 */

#include "MeshCryptoPKI.h"
#include "aes-ccm.h"

#include <Curve25519.h>
#include <SHA256.h>
#include <RNG.h>
#include <string.h>

// For Arduino random() function
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

MeshCryptoPKI::~MeshCryptoPKI() {
    // Clear sensitive data
    memset(privateKey_, 0, sizeof(privateKey_));
}

void MeshCryptoPKI::generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]) {
    // Initialize RNG with some entropy
    RNG.begin("libmeshtastic_leaf");

    // Add some additional entropy from random()
    uint32_t noise = random();
    RNG.stir((uint8_t*)&noise, sizeof(noise));

    // Generate keypair using Curve25519
    // dh1() generates a random private key and derives the public key
    Curve25519::dh1(pubKey, privKey);
}

bool MeshCryptoPKI::regeneratePublicKey(uint8_t pubKey[32], const uint8_t privKey[32]) {
    // Check for all-zero private key
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

    // Derive public key from private key
    // eval() computes pubKey = privKey * basePoint
    Curve25519::eval(pubKey, privKey, nullptr);

    // Check for weak point
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

bool MeshCryptoPKI::hasPrivateKey() const {
    return hasKey_;
}

void MeshCryptoPKI::sha256Hash(uint8_t* data, size_t len) {
    SHA256 hash;
    hash.reset();

    // Process in 16-byte chunks
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

bool MeshCryptoPKI::deriveSharedKey(const uint8_t remotePublic[32], uint8_t sharedKey[32]) {
    if (!hasKey_) {
        return false;
    }

    // Copy remote public key to shared key buffer (dh2 modifies it in place)
    memcpy(sharedKey, remotePublic, 32);

    // Copy private key to temporary buffer (dh2 modifies it)
    uint8_t localPriv[32];
    memcpy(localPriv, privateKey_, 32);

    // Perform ECDH: sharedKey = remotePublic * privateKey
    // dh2() also performs weak key validation
    if (!Curve25519::dh2(sharedKey, localPriv)) {
        memset(sharedKey, 0, 32);
        memset(localPriv, 0, 32);
        return false;
    }

    // Clear temporary private key
    memset(localPriv, 0, 32);

    // Hash the shared secret with SHA256
    sha256Hash(sharedKey, 32);

    return true;
}

void MeshCryptoPKI::initNonce(NodeNum fromNode, uint64_t packetId, uint32_t extraNonce, uint8_t nonce[13]) {
    // Nonce format (13 bytes for AES-CCM with L=2):
    // bytes 0-7: packetId (64-bit, little-endian)
    // bytes 8-11: fromNode (32-bit, little-endian)
    // byte 12: extraNonce (only first byte of the 4-byte extraNonce)
    // Note: The full extraNonce is stored separately in the packet

    memset(nonce, 0, 13);
    memcpy(nonce, &packetId, sizeof(uint64_t));
    memcpy(nonce + sizeof(uint64_t), &fromNode, sizeof(uint32_t));
    // For the nonce, we use the extraNonce value directly in the last position
    nonce[12] = extraNonce & 0xFF;
}

bool MeshCryptoPKI::encrypt(NodeNum toNode, NodeNum fromNode,
                            const uint8_t remotePublic[32], uint64_t packetId,
                            const uint8_t* plain, size_t plainLen,
                            uint8_t* crypt) {
    if (!hasKey_ || plain == nullptr || crypt == nullptr) {
        return false;
    }

    // Derive shared key using ECDH + SHA256
    uint8_t sharedKey[32];
    if (!deriveSharedKey(remotePublic, sharedKey)) {
        return false;
    }

    // Generate random extraNonce
    uint32_t extraNonce = random();

    // Build nonce
    uint8_t nonce[13];
    initNonce(fromNode, packetId, extraNonce, nonce);

    // Encrypt with AES-CCM
    // Output format: ciphertext | auth[8] | extraNonce[4]
    uint8_t* auth = crypt + plainLen;  // Auth tag position

    int result = aes_ccm_ae(sharedKey, 32, nonce, 8,
                            plain, plainLen,
                            nullptr, 0,  // No additional authenticated data
                            crypt, auth);

    // Append extraNonce after auth tag
    memcpy(auth + 8, &extraNonce, sizeof(uint32_t));

    // Clear sensitive data
    memset(sharedKey, 0, sizeof(sharedKey));

    return (result == 0);
}

bool MeshCryptoPKI::decrypt(NodeNum fromNode,
                            const uint8_t remotePublic[32], uint64_t packetId,
                            const uint8_t* crypt, size_t cryptLen,
                            uint8_t* plain) {
    if (!hasKey_ || crypt == nullptr || plain == nullptr) {
        return false;
    }

    // Minimum size check: at least 12 bytes overhead
    if (cryptLen < MESHTASTIC_PKC_OVERHEAD) {
        return false;
    }

    // Calculate plaintext length
    size_t plainLen = cryptLen - MESHTASTIC_PKC_OVERHEAD;

    // Extract auth tag and extraNonce from end of ciphertext
    const uint8_t* auth = crypt + plainLen;
    uint32_t extraNonce;
    memcpy(&extraNonce, auth + 8, sizeof(uint32_t));

    // Derive shared key using ECDH + SHA256
    uint8_t sharedKey[32];
    if (!deriveSharedKey(remotePublic, sharedKey)) {
        return false;
    }

    // Build nonce
    uint8_t nonce[13];
    initNonce(fromNode, packetId, extraNonce, nonce);

    // Decrypt and verify with AES-CCM
    bool result = aes_ccm_ad(sharedKey, 32, nonce, 8,
                             crypt, plainLen,
                             nullptr, 0,  // No additional authenticated data
                             auth, plain);

    // Clear sensitive data
    memset(sharedKey, 0, sizeof(sharedKey));

    return result;
}

} // namespace libmeshtastic_leaf
