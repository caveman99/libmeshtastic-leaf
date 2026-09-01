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
                                PacketId requestId = 0,
                                const uint8_t *signature = nullptr);

  // Portnums the firmware treats as signable, dropping an unsigned one from a
  // known signer and any unsigned first contact NodeInfo.
  static bool isSignablePortNum(meshtastic_PortNum portNum);

  // outSignature takes the 64 byte XEdDSA signature when there is one. It is
  // not checked here; that needs a key, which is the application's to choose.
  static bool decodeDataMessage(const uint8_t *buffer, size_t bufLen,
                                meshtastic_PortNum &outPortNum,
                                uint8_t *outPayload, size_t &outPayloadLen,
                                PacketId *outRequestId = nullptr,
                                uint8_t *outSignature = nullptr,
                                bool *outHasSignature = nullptr);

  // An ack is a Routing message reporting success. The generated types stay in
  // the implementation so no public header includes them.
  static bool encodeRoutingAck(uint8_t *outBuffer, size_t &outLen);
  static bool isRoutingAck(const uint8_t *payload, size_t payloadLen);

  // A User message, which is what a NodeInfo carries. publicKey may be null.
  static bool encodeUser(NodeNum nodeNum, const char *longName,
                         const char *shortName, const uint8_t *publicKey,
                         uint8_t *outBuffer, size_t &outLen);
};

} // namespace libmeshtastic_leaf
