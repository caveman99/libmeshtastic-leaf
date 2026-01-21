/**
 * @file MeshTypes.h
 * @brief Common types and constants for Meshtastic Leaf library
 *
 * This file contains all the fundamental types, constants, and structures
 * used throughout the libmeshtastic_leaf library.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

// Include generated portnums for PortNum enum
#include "generated/meshtastic/config.pb.h"
#include "generated/meshtastic/portnums.pb.h"

// Forward declare types from MeshRegion.h to avoid circular dependency
// The actual MeshRegion.h provides the full implementation

namespace libmeshtastic_leaf {

// ============================================================================
// Protobuf Type Aliases
// ============================================================================

/// Region code using protobuf enum
using RegionCode = meshtastic_Config_LoRaConfig_RegionCode;

/// Modem preset using protobuf enum
using ModemPreset = meshtastic_Config_LoRaConfig_ModemPreset;

// ============================================================================
// Constants
// ============================================================================

/// Maximum LoRa payload length per Semtech datasheets
constexpr size_t MAX_LORA_PAYLOAD_LEN = 255;

/// Meshtastic packet header length (16 bytes)
constexpr size_t MESHTASTIC_HEADER_LENGTH = 16;

/// PKI encryption overhead: auth tag (8 bytes) + extraNonce (4 bytes)
constexpr size_t MESHTASTIC_PKC_OVERHEAD = 12;

/// Maximum encrypted payload size
constexpr size_t MAX_ENCRYPTED_PAYLOAD =
    MAX_LORA_PAYLOAD_LEN - MESHTASTIC_HEADER_LENGTH;

/// Maximum plaintext payload for PKI messages
constexpr size_t MAX_PKI_PAYLOAD =
    MAX_ENCRYPTED_PAYLOAD - MESHTASTIC_PKC_OVERHEAD;

/// Broadcast address
constexpr uint32_t BROADCAST_ADDR = 0xFFFFFFFF;

/// Meshtastic sync word
constexpr uint8_t MESHTASTIC_SYNC_WORD = 0x2B;

/// Default preamble length
constexpr uint16_t DEFAULT_PREAMBLE_LENGTH = 16;

/// AES block size
constexpr size_t AES_BLOCK_SIZE = 16;

/// Curve25519 key size
constexpr size_t CURVE25519_KEY_SIZE = 32;

// ============================================================================
// Packet Flag Masks
// ============================================================================

/// Hop limit mask (bits 0-2)
constexpr uint8_t PACKET_FLAGS_HOP_LIMIT_MASK = 0x07;

/// Want acknowledgment flag (bit 3)
constexpr uint8_t PACKET_FLAGS_WANT_ACK_MASK = 0x08;

/// Via MQTT flag (bit 4)
constexpr uint8_t PACKET_FLAGS_VIA_MQTT_MASK = 0x10;

/// Hop start mask (bits 5-7)
constexpr uint8_t PACKET_FLAGS_HOP_START_MASK = 0xE0;

/// Hop start shift amount
constexpr uint8_t PACKET_FLAGS_HOP_START_SHIFT = 5;

// ============================================================================
// Default PSK
// ============================================================================

/// 16 bytes of the default public channel PSK (AES128)
static const uint8_t DEFAULT_PSK[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29,
                                        0x07, 0x59, 0xf0, 0xbc, 0xff, 0xab,
                                        0xcf, 0x4e, 0x69, 0x01};

// ============================================================================
// Type Aliases
// ============================================================================

/// Node number type (32-bit)
using NodeNum = uint32_t;

/// Packet ID type (32-bit)
using PacketId = uint32_t;

/// Channel index type
using ChannelIndex = uint8_t;

/// Channel hash type
using ChannelHash = uint8_t;

// ============================================================================
// Enums
// ============================================================================

/// Radio chip types supported by the library
enum class RadioType {
  SX1262, ///< SX1262 (sub-GHz)
  SX1268, ///< SX1268 (sub-GHz)
  LLCC68, ///< LLCC68 (compatible with SX126x)
  SX1276, ///< SX1276 (sub-GHz, older)
  SX1278, ///< SX1278 (sub-GHz, older)
  SX1280, ///< SX1280 (2.4GHz)
  LR1110, ///< LR1110 (sub-GHz + GNSS)
  LR1120, ///< LR1120 (sub-GHz)
  LR1121  ///< LR1121 (sub-GHz)
};

// Note: ModemPreset is now defined in MeshRegion.h using the protobuf enum
// Use libmeshtastic_leaf::ModemPreset (alias for
// meshtastic_Config_LoRaConfig_ModemPreset)

/// Packet reception result
enum class ReceiveResult {
  OK,              ///< Packet received and decrypted successfully
  NO_PACKET,       ///< No packet available
  CRC_ERROR,       ///< CRC check failed
  DECRYPT_FAILED,  ///< Decryption failed (wrong key or corrupted)
  PKI_KEY_UNKNOWN, ///< PKI packet but sender's public key not found
  INVALID_HEADER,  ///< Packet header invalid
  TOO_SHORT,       ///< Packet too short
  TOO_LONG         ///< Packet too long
};

/// Transmission result
enum class SendResult {
  OK,            ///< Packet sent successfully
  TX_BUSY,       ///< Radio is busy transmitting
  CHANNEL_BUSY,  ///< Channel is busy (CAD detected activity)
  INVALID_PARAM, ///< Invalid parameter
  TOO_LONG,      ///< Payload too long
  NO_CHANNEL,    ///< No channel configured
  RADIO_ERROR    ///< Radio hardware error
};

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Cryptographic key storage
 */
struct CryptoKey {
  uint8_t bytes[32]; ///< Key bytes (up to 32 for AES256)
  int8_t length;     ///< Key length in bytes, or -1 for invalid

  CryptoKey() : length(0) { memset(bytes, 0, sizeof(bytes)); }

  bool isValid() const { return length > 0; }
  bool isAES128() const { return length == 16; }
  bool isAES256() const { return length == 32; }
};

/**
 * @brief Over-the-air packet header (16 bytes)
 *
 * This structure must exactly match the wire layout when sent over the radio.
 */
struct __attribute__((packed)) PacketHeader {
  NodeNum to;       ///< Destination node (0xFFFFFFFF = broadcast)
  NodeNum from;     ///< Sender node number
  PacketId id;      ///< Packet ID
  uint8_t flags;    ///< hop_limit(3) | want_ack(1) | via_mqtt(1) | hop_start(3)
  uint8_t channel;  ///< Channel hash (0 for PKI)
  uint8_t next_hop; ///< Last byte of next hop node
  uint8_t relay_node; ///< Last byte of relay node

  /// Get hop limit from flags
  uint8_t getHopLimit() const { return flags & PACKET_FLAGS_HOP_LIMIT_MASK; }

  /// Set hop limit in flags
  void setHopLimit(uint8_t limit) {
    flags = (flags & ~PACKET_FLAGS_HOP_LIMIT_MASK) |
            (limit & PACKET_FLAGS_HOP_LIMIT_MASK);
  }

  /// Get hop start from flags
  uint8_t getHopStart() const {
    return (flags & PACKET_FLAGS_HOP_START_MASK) >>
           PACKET_FLAGS_HOP_START_SHIFT;
  }

  /// Set hop start in flags
  void setHopStart(uint8_t start) {
    flags =
        (flags & ~PACKET_FLAGS_HOP_START_MASK) |
        ((start << PACKET_FLAGS_HOP_START_SHIFT) & PACKET_FLAGS_HOP_START_MASK);
  }

  /// Check if want_ack flag is set
  bool wantAck() const { return (flags & PACKET_FLAGS_WANT_ACK_MASK) != 0; }

  /// Set want_ack flag
  void setWantAck(bool ack) {
    if (ack) {
      flags |= PACKET_FLAGS_WANT_ACK_MASK;
    } else {
      flags &= ~PACKET_FLAGS_WANT_ACK_MASK;
    }
  }

  /// Check if via_mqtt flag is set
  bool viaMqtt() const { return (flags & PACKET_FLAGS_VIA_MQTT_MASK) != 0; }

  /// Check if this is a PKI-encrypted packet
  bool isPKI() const { return channel == 0 && to != BROADCAST_ADDR; }
};

/**
 * @brief Complete radio buffer with header and payload
 */
struct __attribute__((packed)) RadioBuffer {
  PacketHeader header;
  uint8_t payload[MAX_LORA_PAYLOAD_LEN + 1 - sizeof(PacketHeader)];
};

/**
 * @brief Decoded mesh packet (after decryption)
 */
struct MeshPacket {
  PacketHeader header;                    ///< Packet header
  meshtastic_PortNum portNum;             ///< Application port number
  uint8_t payload[MAX_ENCRYPTED_PAYLOAD]; ///< Decrypted payload
  size_t payloadLen;                      ///< Payload length
  int16_t rxRssi;                         ///< Received signal strength (dBm)
  float rxSnr;                            ///< Signal-to-noise ratio (dB)
  uint32_t rxTime;                        ///< Reception timestamp (millis)
  bool isPKI;                             ///< True if PKI-encrypted
  ChannelIndex channelIndex; ///< Channel index (for channel-encrypted)

  MeshPacket()
      : portNum(meshtastic_PortNum_UNKNOWN_APP), payloadLen(0), rxRssi(0),
        rxSnr(0), rxTime(0), isPKI(false), channelIndex(0) {
    memset(&header, 0, sizeof(header));
    memset(payload, 0, sizeof(payload));
  }
};

/**
 * @brief Radio configuration structure
 */
struct RadioConfig {
  RadioType type;     ///< Radio chip type
  RegionCode region;  ///< Region code (for frequency/power lookup)
  float frequency;    ///< Center frequency in MHz (0 = auto from region)
  int8_t txPower;     ///< Transmit power in dBm (0 = auto from region)
  ModemPreset preset; ///< Modem preset
  uint8_t csPin;      ///< Chip select pin
  uint8_t irqPin;     ///< IRQ pin (DIO0/DIO1)
  uint8_t rstPin;     ///< Reset pin
  uint8_t busyPin;    ///< Busy pin (for SX126x/LR11x0)
  float tcxoVoltage;  ///< TCXO voltage (0 to disable)

  RadioConfig()
      : type(RadioType::SX1262),
        region(meshtastic_Config_LoRaConfig_RegionCode_US), frequency(0.0f),
        txPower(0), preset(meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST),
        csPin(0), irqPin(0), rstPin(0), busyPin(0), tcxoVoltage(0.0f) {}
};

/**
 * @brief Library configuration structure
 */
struct MeshConfig {
  RadioConfig radio; ///< Radio configuration
  NodeNum nodeNum;   ///< This node's number
  uint8_t hopLimit;  ///< Default hop limit for transmissions

  MeshConfig() : nodeNum(0), hopLimit(3) {}
};

// Note: ModemParams is now defined in MeshRegion.h
// Use MeshRegion::getModemParams(preset) to get parameters for a preset

// ============================================================================
// Callback Types
// ============================================================================

/**
 * @brief Callback for looking up a node's public key for PKI decryption
 * @param nodeNum The node number to look up
 * @param pubKey Output buffer for the 32-byte public key
 * @return true if the key was found, false otherwise
 */
using PKIKeyLookup = bool (*)(NodeNum nodeNum, uint8_t pubKey[32]);

/**
 * @brief Callback for received packets
 * @param packet The received and decoded packet
 */
using PacketCallback = void (*)(const MeshPacket &packet);

} // namespace libmeshtastic_leaf
