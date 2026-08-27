#include "MeshPayload.h"

#include "generated/meshtastic/leafdata.pb.h"
#include <pb_decode.h>
#include <pb_encode.h>
#include <string.h>

namespace libmeshtastic_leaf {

bool MeshPayloadCodec::encodeDataMessage(meshtastic_PortNum portNum,
                                         const uint8_t *payload,
                                         size_t payloadLen, uint8_t *outBuffer,
                                         size_t &outLen, PacketId requestId) {
  meshtastic_Data data = meshtastic_Data_init_zero;
  data.portnum = portNum;
  data.request_id = requestId;
  data.payload.size = payloadLen;

  if (payloadLen > sizeof(data.payload.bytes)) {
    return false;
  }
  memcpy(data.payload.bytes, payload, payloadLen);

  pb_ostream_t stream =
      pb_ostream_from_buffer(outBuffer, MAX_ENCRYPTED_PAYLOAD);
  if (!pb_encode(&stream, meshtastic_Data_fields, &data)) {
    return false;
  }

  outLen = stream.bytes_written;
  return true;
}

bool MeshPayloadCodec::decodeDataMessage(const uint8_t *buffer, size_t bufLen,
                                         meshtastic_PortNum &outPortNum,
                                         uint8_t *outPayload,
                                         size_t &outPayloadLen,
                                         PacketId *outRequestId) {
  meshtastic_Data data = meshtastic_Data_init_zero;

  pb_istream_t stream = pb_istream_from_buffer(buffer, bufLen);
  if (!pb_decode(&stream, meshtastic_Data_fields, &data)) {
    return false;
  }

  outPortNum = data.portnum;
  outPayloadLen = data.payload.size;
  if (outRequestId != nullptr) {
    *outRequestId = data.request_id;
  }

  if (outPayloadLen > 0 && outPayload != nullptr) {
    memcpy(outPayload, data.payload.bytes, outPayloadLen);
  }

  return true;
}

bool MeshPayloadCodec::encodeRoutingAck(uint8_t *outBuffer, size_t &outLen) {
  meshtastic_Routing routing = meshtastic_Routing_init_zero;
  routing.which_variant = meshtastic_Routing_error_reason_tag;
  routing.error_reason = meshtastic_Routing_Error_NONE;

  pb_ostream_t stream =
      pb_ostream_from_buffer(outBuffer, MAX_ENCRYPTED_PAYLOAD);
  if (!pb_encode(&stream, meshtastic_Routing_fields, &routing)) {
    return false;
  }
  outLen = stream.bytes_written;
  return true;
}

bool MeshPayloadCodec::isRoutingAck(const uint8_t *payload, size_t payloadLen) {
  meshtastic_Routing routing = meshtastic_Routing_init_zero;
  pb_istream_t stream = pb_istream_from_buffer(payload, payloadLen);
  if (!pb_decode(&stream, meshtastic_Routing_fields, &routing)) {
    return false;
  }
  return routing.which_variant == meshtastic_Routing_error_reason_tag &&
         routing.error_reason == meshtastic_Routing_Error_NONE;
}

} // namespace libmeshtastic_leaf
