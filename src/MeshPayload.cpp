#include "MeshPayload.h"

#include "generated/meshtastic/leafdata.pb.h"
#include <pb_decode.h>
#include <pb_encode.h>
#include <stdio.h>
#include <string.h>

namespace libmeshtastic_leaf {

bool MeshPayloadCodec::encodeDataMessage(meshtastic_PortNum portNum,
                                         const uint8_t *payload,
                                         size_t payloadLen, uint8_t *outBuffer,
                                         size_t &outLen, PacketId requestId,
                                         const uint8_t *signature) {
  meshtastic_Data data = meshtastic_Data_init_zero;
  data.portnum = portNum;
  data.request_id = requestId;
  if (signature != nullptr) {
    data.xeddsa_signature.size = 64;
    memcpy(data.xeddsa_signature.bytes, signature, 64);
  }
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

bool MeshPayloadCodec::decodeDataMessage(
    const uint8_t *buffer, size_t bufLen, meshtastic_PortNum &outPortNum,
    uint8_t *outPayload, size_t &outPayloadLen, PacketId *outRequestId,
    uint8_t *outSignature, bool *outHasSignature) {
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

  const bool signed_ = data.xeddsa_signature.size == 64;
  if (outHasSignature != nullptr) {
    *outHasSignature = signed_;
  }
  if (signed_ && outSignature != nullptr) {
    memcpy(outSignature, data.xeddsa_signature.bytes, 64);
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

bool MeshPayloadCodec::encodeUser(NodeNum nodeNum, const char *longName,
                                  const char *shortName,
                                  const uint8_t *publicKey, uint8_t *outBuffer,
                                  size_t &outLen) {
  meshtastic_User user = meshtastic_User_init_zero;

  // The id is the node number as the mesh writes it, "!" then eight hex.
  snprintf(user.id, sizeof(user.id), "!%08x", (unsigned)nodeNum);

  if (longName != nullptr && longName[0] != '\0') {
    strncpy(user.long_name, longName, sizeof(user.long_name) - 1);
  } else {
    snprintf(user.long_name, sizeof(user.long_name), "Meshtastic %04x",
             (unsigned)(nodeNum & 0xFFFF));
  }

  if (shortName != nullptr && shortName[0] != '\0') {
    strncpy(user.short_name, shortName, sizeof(user.short_name) - 1);
  } else {
    snprintf(user.short_name, sizeof(user.short_name), "%04x",
             (unsigned)(nodeNum & 0xFFFF));
  }

  user.hw_model = meshtastic_HardwareModel_PRIVATE_HW;
  user.role = meshtastic_Config_DeviceConfig_Role_CLIENT;

  if (publicKey != nullptr) {
    user.public_key.size = 32;
    memcpy(user.public_key.bytes, publicKey, 32);
  }

  pb_ostream_t stream =
      pb_ostream_from_buffer(outBuffer, MAX_ENCRYPTED_PAYLOAD);
  if (!pb_encode(&stream, meshtastic_User_fields, &user)) {
    return false;
  }
  outLen = stream.bytes_written;
  return true;
}

bool MeshPayloadCodec::isSignablePortNum(meshtastic_PortNum portNum) {
  switch (portNum) {
  case meshtastic_PortNum_POSITION_APP:
  case meshtastic_PortNum_TELEMETRY_APP:
  case meshtastic_PortNum_WAYPOINT_APP:
  case meshtastic_PortNum_NODEINFO_APP:
    return true;
  default:
    return false;
  }
}

} // namespace libmeshtastic_leaf
