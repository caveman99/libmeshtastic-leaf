/**
 * @file MeshPacket.cpp
 * @brief Implementation of packet encoding/decoding
 */

#include "MeshPacket.h"
#include "generated/meshtastic/leafdata.pb.h"
#include <pb_decode.h>
#include <pb_encode.h>
#include <string.h>

// For Arduino millis() and random()
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <stdlib.h>
#include <time.h>
#define millis() ((uint32_t)(clock() / (CLOCKS_PER_SEC / 1000)))
#define random() rand()
#endif

namespace libmeshtastic_leaf {

uint32_t MeshPacketCodec::packetIdCounter_ = 0;

void MeshPacketCodec::packHeader(const PacketHeader &header, uint8_t *buffer) {
  // Pack header in little-endian format
  // The PacketHeader struct is already packed, but we do it explicitly
  // to ensure portability

  // to (4 bytes, little-endian)
  buffer[0] = (header.to >> 0) & 0xFF;
  buffer[1] = (header.to >> 8) & 0xFF;
  buffer[2] = (header.to >> 16) & 0xFF;
  buffer[3] = (header.to >> 24) & 0xFF;

  // from (4 bytes, little-endian)
  buffer[4] = (header.from >> 0) & 0xFF;
  buffer[5] = (header.from >> 8) & 0xFF;
  buffer[6] = (header.from >> 16) & 0xFF;
  buffer[7] = (header.from >> 24) & 0xFF;

  // id (4 bytes, little-endian)
  buffer[8] = (header.id >> 0) & 0xFF;
  buffer[9] = (header.id >> 8) & 0xFF;
  buffer[10] = (header.id >> 16) & 0xFF;
  buffer[11] = (header.id >> 24) & 0xFF;

  // flags (1 byte)
  buffer[12] = header.flags;

  // channel (1 byte)
  buffer[13] = header.channel;

  // next_hop (1 byte)
  buffer[14] = header.next_hop;

  // relay_node (1 byte)
  buffer[15] = header.relay_node;
}

void MeshPacketCodec::unpackHeader(const uint8_t *buffer,
                                   PacketHeader &header) {
  // Unpack header from little-endian format

  // to (4 bytes, little-endian)
  header.to = ((uint32_t)buffer[0] << 0) | ((uint32_t)buffer[1] << 8) |
              ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);

  // from (4 bytes, little-endian)
  header.from = ((uint32_t)buffer[4] << 0) | ((uint32_t)buffer[5] << 8) |
                ((uint32_t)buffer[6] << 16) | ((uint32_t)buffer[7] << 24);

  // id (4 bytes, little-endian)
  header.id = ((uint32_t)buffer[8] << 0) | ((uint32_t)buffer[9] << 8) |
              ((uint32_t)buffer[10] << 16) | ((uint32_t)buffer[11] << 24);

  // flags (1 byte)
  header.flags = buffer[12];

  // channel (1 byte)
  header.channel = buffer[13];

  // next_hop (1 byte)
  header.next_hop = buffer[14];

  // relay_node (1 byte)
  header.relay_node = buffer[15];
}

bool MeshPacketCodec::encodePacket(const PacketHeader &header,
                                   const uint8_t *payload, size_t payloadLen,
                                   MeshChannel &channel, uint8_t *outBuffer,
                                   size_t &outLen) {
  // Check payload size
  if (payloadLen > MAX_ENCRYPTED_PAYLOAD) {
    return false;
  }

  // Pack header
  packHeader(header, outBuffer);

  // Copy payload after header
  uint8_t *payloadDst = outBuffer + MESHTASTIC_HEADER_LENGTH;
  memcpy(payloadDst, payload, payloadLen);

  // Encrypt payload in place using channel crypto
  if (channel.isEncrypted()) {
    channel.getCrypto().encrypt(header.from, header.id, payloadDst, payloadLen);
  }

  outLen = MESHTASTIC_HEADER_LENGTH + payloadLen;
  return true;
}

bool MeshPacketCodec::encodePacketPKI(PacketHeader &header,
                                      const uint8_t *payload, size_t payloadLen,
                                      MeshCryptoPKI &pki,
                                      const uint8_t remotePubKey[32],
                                      uint8_t *outBuffer, size_t &outLen) {
  // Check payload size (account for PKI overhead)
  if (payloadLen + MESHTASTIC_PKC_OVERHEAD > MAX_ENCRYPTED_PAYLOAD) {
    return false;
  }

  // PKI packets have channel == 0
  header.channel = 0;

  // Pack header
  packHeader(header, outBuffer);

  // Encrypt payload with PKI
  uint8_t *payloadDst = outBuffer + MESHTASTIC_HEADER_LENGTH;

  if (!pki.encrypt(header.to, header.from, remotePubKey, header.id, payload,
                   payloadLen, payloadDst)) {
    return false;
  }

  outLen = MESHTASTIC_HEADER_LENGTH + payloadLen + MESHTASTIC_PKC_OVERHEAD;
  return true;
}

ReceiveResult MeshPacketCodec::decodePacket(const uint8_t *buffer,
                                            size_t bufLen, MeshChannel &channel,
                                            MeshPacket &outPacket) {
  // Minimum size check
  if (bufLen < MESHTASTIC_HEADER_LENGTH) {
    return ReceiveResult::TOO_SHORT;
  }

  // Maximum size check
  if (bufLen > MAX_LORA_PAYLOAD_LEN) {
    return ReceiveResult::TOO_LONG;
  }

  // Unpack header
  unpackHeader(buffer, outPacket.header);

  // Verify channel hash
  if (outPacket.header.channel != channel.getHash()) {
    return ReceiveResult::DECRYPT_FAILED;
  }

  // Calculate payload length
  size_t encryptedLen = bufLen - MESHTASTIC_HEADER_LENGTH;
  if (encryptedLen > MAX_ENCRYPTED_PAYLOAD) {
    return ReceiveResult::TOO_LONG;
  }

  // Copy encrypted payload
  memcpy(outPacket.payload, buffer + MESHTASTIC_HEADER_LENGTH, encryptedLen);
  outPacket.payloadLen = encryptedLen;

  // Decrypt payload in place
  if (channel.isEncrypted()) {
    channel.getCrypto().decrypt(outPacket.header.from, outPacket.header.id,
                                outPacket.payload, outPacket.payloadLen);
  }

  outPacket.isPKI = false;
  return ReceiveResult::OK;
}

ReceiveResult MeshPacketCodec::decodePacketPKI(const uint8_t *buffer,
                                               size_t bufLen,
                                               MeshCryptoPKI &pki,
                                               const uint8_t senderPubKey[32],
                                               MeshPacket &outPacket) {
  // Minimum size check (header + PKI overhead)
  if (bufLen < MESHTASTIC_HEADER_LENGTH + MESHTASTIC_PKC_OVERHEAD) {
    return ReceiveResult::TOO_SHORT;
  }

  // Maximum size check
  if (bufLen > MAX_LORA_PAYLOAD_LEN) {
    return ReceiveResult::TOO_LONG;
  }

  // Unpack header
  unpackHeader(buffer, outPacket.header);

  // Verify this is a PKI packet
  if (!outPacket.header.isPKI()) {
    return ReceiveResult::INVALID_HEADER;
  }

  // Calculate encrypted payload length
  size_t encryptedLen = bufLen - MESHTASTIC_HEADER_LENGTH;
  const uint8_t *encryptedData = buffer + MESHTASTIC_HEADER_LENGTH;

  // Calculate plaintext length
  outPacket.payloadLen = encryptedLen - MESHTASTIC_PKC_OVERHEAD;

  // Decrypt with PKI
  if (!pki.decrypt(outPacket.header.from, senderPubKey, outPacket.header.id,
                   encryptedData, encryptedLen, outPacket.payload)) {
    return ReceiveResult::DECRYPT_FAILED;
  }

  outPacket.isPKI = true;
  return ReceiveResult::OK;
}

bool MeshPacketCodec::isPKIPacket(const uint8_t *buffer, size_t bufLen) {
  if (bufLen < MESHTASTIC_HEADER_LENGTH) {
    return false;
  }

  PacketHeader header;
  unpackHeader(buffer, header);
  return header.isPKI();
}

bool MeshPacketCodec::matchesChannelHash(const uint8_t *buffer, size_t bufLen,
                                         ChannelHash expectedHash) {
  if (bufLen < MESHTASTIC_HEADER_LENGTH) {
    return false;
  }

  // Channel hash is at offset 13
  return buffer[13] == expectedHash;
}

PacketId MeshPacketCodec::generatePacketId(NodeNum nodeNum) {
  // Combine counter, time, and node number for a pseudo-random ID
  packetIdCounter_++;

  uint32_t time = millis();
  uint32_t rnd = random();

  // Mix the values together
  return (packetIdCounter_ ^ (time << 16) ^ rnd ^ nodeNum);
}

bool MeshPacketCodec::encodeDataMessage(meshtastic_PortNum portNum,
                                        const uint8_t *payload,
                                        size_t payloadLen, uint8_t *outBuffer,
                                        size_t &outLen) {
  // Create a Data message
  meshtastic_Data data = meshtastic_Data_init_zero;
  data.portnum = portNum;
  data.payload.size = payloadLen;

  // Check payload fits
  if (payloadLen > sizeof(data.payload.bytes)) {
    return false;
  }
  memcpy(data.payload.bytes, payload, payloadLen);

  // Encode with nanopb
  pb_ostream_t stream =
      pb_ostream_from_buffer(outBuffer, MAX_ENCRYPTED_PAYLOAD);
  if (!pb_encode(&stream, meshtastic_Data_fields, &data)) {
    return false;
  }

  outLen = stream.bytes_written;
  return true;
}

bool MeshPacketCodec::decodeDataMessage(const uint8_t *buffer, size_t bufLen,
                                        meshtastic_PortNum &outPortNum,
                                        uint8_t *outPayload,
                                        size_t &outPayloadLen) {
  // Decode with nanopb
  meshtastic_Data data = meshtastic_Data_init_zero;

  pb_istream_t stream = pb_istream_from_buffer(buffer, bufLen);
  if (!pb_decode(&stream, meshtastic_Data_fields, &data)) {
    return false;
  }

  outPortNum = data.portnum;
  outPayloadLen = data.payload.size;

  if (outPayloadLen > 0 && outPayload != nullptr) {
    memcpy(outPayload, data.payload.bytes, outPayloadLen);
  }

  return true;
}

} // namespace libmeshtastic_leaf
