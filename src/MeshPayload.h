#pragma once

#include "MeshTypes.h"
#include <stdint.h>

namespace libmeshtastic_leaf {

// The Data submessage and the Routing payload an ack carries. Kept apart from
// MeshPacket so encoding a payload does not drag in the crypto engine.
class MeshPayloadCodec {
public:
  static bool encodeDataMessage(meshtastic_PortNum portNum,
                                const uint8_t *payload, size_t payloadLen,
                                uint8_t *outBuffer, size_t &outLen,
                                PacketId requestId = 0);

  static bool decodeDataMessage(const uint8_t *buffer, size_t bufLen,
                                meshtastic_PortNum &outPortNum,
                                uint8_t *outPayload, size_t &outPayloadLen,
                                PacketId *outRequestId = nullptr);

  // An ack is a Routing message reporting success. The generated types stay in
  // the implementation so no public header includes them.
  static bool encodeRoutingAck(uint8_t *outBuffer, size_t &outLen);
  static bool isRoutingAck(const uint8_t *payload, size_t payloadLen);
};

} // namespace libmeshtastic_leaf
