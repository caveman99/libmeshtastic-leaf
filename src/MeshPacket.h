
#pragma once

#include "MeshChannel.h"
#include "MeshCryptoPKI.h"
#include "MeshPayload.h"
#include "MeshTypes.h"

namespace libmeshtastic_leaf {

class MeshPacketCodec {
public:
  static void packHeader(const PacketHeader &header, uint8_t *buffer);

  static void unpackHeader(const uint8_t *buffer, PacketHeader &header);

  static bool encodePacket(const PacketHeader &header, const uint8_t *payload,
                           size_t payloadLen, MeshChannel &channel,
                           uint8_t *outBuffer, size_t &outLen);

  static bool encodePacketPKI(PacketHeader &header, const uint8_t *payload,
                              size_t payloadLen, MeshCryptoPKI &pki,
                              const uint8_t remotePubKey[32],
                              uint8_t *outBuffer, size_t &outLen);

  static ReceiveResult decodePacket(const uint8_t *buffer, size_t bufLen,
                                    MeshChannel &channel,
                                    MeshPacket &outPacket);

  static ReceiveResult decodePacketPKI(const uint8_t *buffer, size_t bufLen,
                                       MeshCryptoPKI &pki,
                                       const uint8_t senderPubKey[32],
                                       MeshPacket &outPacket);

  static bool isPKIPacket(const uint8_t *buffer, size_t bufLen);

  static bool matchesChannelHash(const uint8_t *buffer, size_t bufLen,
                                 ChannelHash expectedHash);

  static PacketId generatePacketId(NodeNum nodeNum);

private:
  /// Counter for packet ID generation
  static uint32_t packetIdCounter_;
};

} // namespace libmeshtastic_leaf
