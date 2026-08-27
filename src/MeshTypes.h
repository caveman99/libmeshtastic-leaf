
#pragma once

#include <stddef.h>
#include <stdint.h>

// Only portnums.pb.h is reachable from the public headers; Data is parsed in
// exactly one translation unit (MeshPacket.cpp).
#include "generated/meshtastic/portnums.pb.h"

namespace libmeshtastic_leaf {

// Mirrors RegionCode and ModemPreset from upstream config.proto, declared
// natively so no public header pulls in config.pb.h. Append only, never
// renumber.

enum RegionCode : uint8_t {
  REGION_UNSET = 0,
  REGION_US = 1,
  REGION_EU_433 = 2,
  REGION_EU_868 = 3,
  REGION_CN = 4,
  REGION_JP = 5,
  REGION_ANZ = 6,
  REGION_KR = 7,
  REGION_TW = 8,
  REGION_RU = 9,
  REGION_IN = 10,
  REGION_NZ_865 = 11,
  REGION_TH = 12,
  REGION_LORA_24 = 13,
  REGION_UA_433 = 14,
  REGION_UA_868 = 15,
  REGION_MY_433 = 16,
  REGION_MY_919 = 17,
  REGION_SG_923 = 18,
  REGION_PH_433 = 19,
  REGION_PH_868 = 20,
  REGION_PH_915 = 21,
  REGION_ANZ_433 = 22,
  REGION_KZ_433 = 23,
  REGION_KZ_863 = 24,
  REGION_NP_865 = 25,
  REGION_BR_902 = 26,
  REGION_ITU1_2M = 27,
  REGION_ITU2_2M = 28,
  REGION_EU_866 = 29,
  REGION_EU_874 = 30,
  REGION_EU_917 = 31,
  REGION_EU_N_868 = 32,
  REGION_ITU3_2M = 33,
  REGION_ITU1_70CM = 34,
  REGION_ITU2_70CM = 35,
  REGION_ITU3_70CM = 36,
  REGION_ITU2_125CM = 37,
};

enum ModemPreset : uint8_t {
  PRESET_LONG_FAST = 0,
  PRESET_LONG_SLOW = 1,
  PRESET_VERY_LONG_SLOW = 2,
  PRESET_MEDIUM_SLOW = 3,
  PRESET_MEDIUM_FAST = 4,
  PRESET_SHORT_SLOW = 5,
  PRESET_SHORT_FAST = 6,
  PRESET_LONG_MODERATE = 7,
  PRESET_SHORT_TURBO = 8,
  PRESET_LONG_TURBO = 9,
  PRESET_LITE_FAST = 10,
  PRESET_LITE_SLOW = 11,
  PRESET_NARROW_FAST = 12,
  PRESET_NARROW_SLOW = 13,
  PRESET_TINY_FAST = 14,
  PRESET_TINY_SLOW = 15,
  PRESET_MEDIUM_TURBO = 16,
};

/// Maximum LoRa payload length per Semtech datasheets
constexpr size_t MAX_LORA_PAYLOAD_LEN = 255;

constexpr size_t MESHTASTIC_HEADER_LENGTH = 16;

/// PKI encryption overhead: auth tag (8 bytes) + extraNonce (4 bytes)
constexpr size_t MESHTASTIC_PKC_OVERHEAD = 12;

constexpr size_t MAX_ENCRYPTED_PAYLOAD =
    MAX_LORA_PAYLOAD_LEN - MESHTASTIC_HEADER_LENGTH;

constexpr size_t MAX_PKI_PAYLOAD =
    MAX_ENCRYPTED_PAYLOAD - MESHTASTIC_PKC_OVERHEAD;

constexpr uint32_t BROADCAST_ADDR = 0xFFFFFFFF;

constexpr uint8_t MESHTASTIC_SYNC_WORD = 0x2B;

constexpr uint16_t DEFAULT_PREAMBLE_LENGTH = 16;

constexpr size_t AES_BLOCK_SIZE = 16;

constexpr size_t CURVE25519_KEY_SIZE = 32;

constexpr uint8_t PACKET_FLAGS_HOP_LIMIT_MASK = 0x07;

constexpr uint8_t PACKET_FLAGS_WANT_ACK_MASK = 0x08;

constexpr uint8_t PACKET_FLAGS_VIA_MQTT_MASK = 0x10;

constexpr uint8_t PACKET_FLAGS_HOP_START_MASK = 0xE0;

constexpr uint8_t PACKET_FLAGS_HOP_START_SHIFT = 5;

/// 16 bytes of the default public channel PSK (AES128)
static const uint8_t DEFAULT_PSK[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29,
                                        0x07, 0x59, 0xf0, 0xbc, 0xff, 0xab,
                                        0xcf, 0x4e, 0x69, 0x01};

using NodeNum = uint32_t;

using PacketId = uint32_t;

using ChannelIndex = uint8_t;

using ChannelHash = uint8_t;

enum class ReceiveResult {
  OK,
  NO_PACKET,
  CRC_ERROR,
  DECRYPT_FAILED,
  PKI_KEY_UNKNOWN,
  INVALID_HEADER,
  TOO_SHORT,
  TOO_LONG
};

enum class SendResult {
  OK,
  TX_BUSY,
  CHANNEL_BUSY, ///< Channel is busy (CAD detected activity)
  INVALID_PARAM,
  TOO_LONG,
  NO_CHANNEL,
  RADIO_ERROR
};

struct CryptoKey {
  uint8_t bytes[32]; ///< Key bytes (up to 32 for AES256)
  int8_t length;     ///< Key length in bytes, or -1 for invalid

  CryptoKey() : length(0) { memset(bytes, 0, sizeof(bytes)); }

  bool isValid() const { return length > 0; }
  bool isAES128() const { return length == 16; }
  bool isAES256() const { return length == 32; }
};

struct __attribute__((packed)) PacketHeader {
  NodeNum to; ///< Destination node (0xFFFFFFFF = broadcast)
  NodeNum from;
  PacketId id;
  uint8_t flags;    ///< hop_limit(3) | want_ack(1) | via_mqtt(1) | hop_start(3)
  uint8_t channel;  ///< Channel hash (0 for PKI)
  uint8_t next_hop; ///< Last byte of next hop node
  uint8_t relay_node; ///< Last byte of relay node

  uint8_t getHopLimit() const { return flags & PACKET_FLAGS_HOP_LIMIT_MASK; }

  void setHopLimit(uint8_t limit) {
    flags = (flags & ~PACKET_FLAGS_HOP_LIMIT_MASK) |
            (limit & PACKET_FLAGS_HOP_LIMIT_MASK);
  }

  uint8_t getHopStart() const {
    return (flags & PACKET_FLAGS_HOP_START_MASK) >>
           PACKET_FLAGS_HOP_START_SHIFT;
  }

  void setHopStart(uint8_t start) {
    flags =
        (flags & ~PACKET_FLAGS_HOP_START_MASK) |
        ((start << PACKET_FLAGS_HOP_START_SHIFT) & PACKET_FLAGS_HOP_START_MASK);
  }

  bool wantAck() const { return (flags & PACKET_FLAGS_WANT_ACK_MASK) != 0; }

  void setWantAck(bool ack) {
    if (ack) {
      flags |= PACKET_FLAGS_WANT_ACK_MASK;
    } else {
      flags &= ~PACKET_FLAGS_WANT_ACK_MASK;
    }
  }

  bool viaMqtt() const { return (flags & PACKET_FLAGS_VIA_MQTT_MASK) != 0; }

  bool isPKI() const { return channel == 0 && to != BROADCAST_ADDR; }
};

struct __attribute__((packed)) RadioBuffer {
  PacketHeader header;
  uint8_t payload[MAX_LORA_PAYLOAD_LEN + 1 - sizeof(PacketHeader)];
};

struct MeshPacket {
  PacketHeader header;
  meshtastic_PortNum portNum;
  uint8_t payload[MAX_ENCRYPTED_PAYLOAD];
  size_t payloadLen;
  int16_t rxRssi;            ///< Received signal strength (dBm)
  float rxSnr;               ///< Signal-to-noise ratio (dB)
  uint32_t rxTime;           ///< Reception timestamp (millis)
  bool isPKI;                ///< True if PKI-encrypted
  ChannelIndex channelIndex; ///< Channel index (for channel-encrypted)

  MeshPacket()
      : portNum(meshtastic_PortNum_UNKNOWN_APP), payloadLen(0), rxRssi(0),
        rxSnr(0), rxTime(0), isPKI(false), channelIndex(0) {
    memset(&header, 0, sizeof(header));
    memset(payload, 0, sizeof(payload));
  }
};

struct RadioConfig {
  RegionCode region; ///< Region code (for frequency/power lookup)
  float frequency;   ///< Center frequency in MHz (0 = auto from region)
  int8_t txPower;    ///< Transmit power in dBm (0 = auto from region)
  ModemPreset preset;
  RadioConfig()
      : region(REGION_US), frequency(0.0f), txPower(0),
        preset(PRESET_LONG_FAST) {}
};

struct MeshConfig {
  RadioConfig radio; ///< Radio configuration
  NodeNum nodeNum;   ///< This node's number
  uint8_t hopLimit;  ///< Default hop limit for transmissions

  MeshConfig() : nodeNum(0), hopLimit(3) {}
};

using PKIKeyLookup = bool (*)(NodeNum nodeNum, uint8_t pubKey[32]);

using PacketCallback = void (*)(const MeshPacket &packet);

} // namespace libmeshtastic_leaf
