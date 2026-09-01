/**
 * @file aes-ccm.h
 * @brief AES-CCM authenticated encryption for PKI
 *
 * Counter with CBC-MAC (CCM) with AES for Meshtastic PKI encryption.
 * Based on the implementation from the main Meshtastic firmware.
 *
 * Copyright (c) 2010-2012, Jouni Malinen <j@w1.fi>
 * This software may be distributed under the terms of the BSD license.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace libmeshtastic_leaf {

/**
 * @brief AES-CCM authenticated encryption
 *
 * Encrypts plaintext and generates authentication tag.
 * Uses fixed L=2 (2-byte length field) and supports aad_len <= 30.
 *
 * @param key Encryption key (16 or 32 bytes)
 * @param key_len Key length in bytes
 * @param nonce Nonce value (13 bytes for L=2)
 * @param M Authentication tag length (typically 8)
 * @param plain Plaintext input
 * @param plain_len Plaintext length
 * @param aad Additional authenticated data (optional)
 * @param aad_len AAD length (must be <= 30)
 * @param crypt Output ciphertext buffer (same size as plaintext)
 * @param auth Output authentication tag buffer (M bytes)
 * @return 0 on success, -1 on error
 */
int aes_ccm_ae(const uint8_t *key, size_t key_len, const uint8_t *nonce,
               size_t M, const uint8_t *plain, size_t plain_len,
               const uint8_t *aad, size_t aad_len, uint8_t *crypt,
               uint8_t *auth);

/**
 * @brief AES-CCM authenticated decryption
 *
 * Decrypts ciphertext and verifies authentication tag.
 * Uses fixed L=2 (2-byte length field) and supports aad_len <= 30.
 *
 * @param key Encryption key (16 or 32 bytes)
 * @param key_len Key length in bytes
 * @param nonce Nonce value (13 bytes for L=2)
 * @param M Authentication tag length (typically 8)
 * @param crypt Ciphertext input
 * @param crypt_len Ciphertext length
 * @param aad Additional authenticated data (optional)
 * @param aad_len AAD length (must be <= 30)
 * @param auth Authentication tag to verify (M bytes)
 * @param plain Output plaintext buffer (same size as ciphertext)
 * @return true if decryption and verification succeeded, false otherwise
 */
bool aes_ccm_ad(const uint8_t *key, size_t key_len, const uint8_t *nonce,
                size_t M, const uint8_t *crypt, size_t crypt_len,
                const uint8_t *aad, size_t aad_len, const uint8_t *auth,
                uint8_t *plain);

} // namespace libmeshtastic_leaf
