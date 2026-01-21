/**
 * @file MeshChannel.h
 * @brief Channel management with PSK expansion and hash calculation
 *
 * This handles Meshtastic channel configuration including:
 * - PSK (pre-shared key) expansion from short forms
 * - Channel hash calculation for identifying channels
 * - Key management for encryption/decryption
 */

#pragma once

#include "MeshTypes.h"
#include "MeshCrypto.h"

namespace libmeshtastic_leaf {

/// Maximum channel name length
constexpr size_t MAX_CHANNEL_NAME_LEN = 12;

/**
 * @brief Channel configuration and management
 *
 * Handles channel setup, PSK expansion, and hash calculation for
 * Meshtastic channel-based encryption. The channel hash is an XOR
 * of the channel name and PSK bytes, used to quickly identify
 * which channel a received packet belongs to.
 */
class MeshChannel {
public:
    MeshChannel();
    ~MeshChannel() = default;

    /**
     * @brief Set channel from raw PSK and name
     *
     * Configures the channel with the provided PSK. The PSK is expanded
     * according to Meshtastic conventions:
     * - If pskLen == 0: No encryption (cleartext)
     * - If pskLen == 1: Use as index into default PSK variations
     *   - Index 0: No encryption
     *   - Index 1: Default PSK
     *   - Index 2+: Default PSK with last byte incremented
     * - If pskLen < 16: Pad with zeros to 16 bytes (AES128)
     * - If 16 <= pskLen < 32: Pad with zeros to 32 bytes (AES256)
     * - If pskLen == 16 or 32: Use directly
     *
     * @param psk PSK bytes
     * @param pskLen PSK length (0, 1, or 16-32)
     * @param name Channel name (optional, for hash calculation)
     * @return true if the channel was configured successfully
     */
    bool setChannel(const uint8_t* psk, size_t pskLen, const char* name = "");

    /**
     * @brief Set to the default public channel
     *
     * Configures using the default Meshtastic PSK (index 1).
     * This is equivalent to setChannel({1}, 1, "").
     */
    void setDefaultChannel();

    /**
     * @brief Disable encryption for this channel
     *
     * Configures the channel for cleartext transmission.
     */
    void disableEncryption();

    /**
     * @brief Get the channel hash
     *
     * Returns the XOR hash of channel name and PSK, used to
     * identify the channel for received packets.
     *
     * @return Channel hash (0-255)
     */
    ChannelHash getHash() const { return hash_; }

    /**
     * @brief Get the expanded encryption key
     * @return Reference to the crypto key
     */
    const CryptoKey& getKey() const { return key_; }

    /**
     * @brief Check if encryption is enabled
     * @return true if the channel has a valid encryption key
     */
    bool isEncrypted() const { return key_.isValid(); }

    /**
     * @brief Get the channel name
     * @return Pointer to channel name string
     */
    const char* getName() const { return name_; }

    /**
     * @brief Get the crypto engine for this channel
     * @return Reference to the MeshCrypto instance
     */
    MeshCrypto& getCrypto() { return crypto_; }
    const MeshCrypto& getCrypto() const { return crypto_; }

    /**
     * @brief Calculate channel hash for arbitrary name and key
     *
     * Static utility function to calculate what the hash would be
     * for a given channel configuration.
     *
     * @param name Channel name
     * @param key Expanded crypto key
     * @return Channel hash (0-255)
     */
    static ChannelHash calculateHash(const char* name, const CryptoKey& key);

private:
    /**
     * @brief Expand a short PSK to full key
     * @param psk Input PSK bytes
     * @param pskLen Input PSK length
     * @param outKey Output expanded key
     */
    void expandPSK(const uint8_t* psk, size_t pskLen, CryptoKey& outKey);

    /**
     * @brief Calculate XOR hash of byte array
     * @param data Input bytes
     * @param len Length
     * @return XOR of all bytes
     */
    static uint8_t xorHash(const uint8_t* data, size_t len);

    char name_[MAX_CHANNEL_NAME_LEN + 1];   ///< Channel name
    CryptoKey key_;                          ///< Expanded encryption key
    ChannelHash hash_;                       ///< Calculated channel hash
    MeshCrypto crypto_;                      ///< Crypto engine instance
};

} // namespace libmeshtastic_leaf
