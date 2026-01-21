/**
 * @file MeshPacket.h
 * @brief Packet encoding/decoding utilities
 *
 * This file provides utilities for packing and unpacking Meshtastic
 * packets, including header serialization and Data payload handling.
 */

#pragma once

#include "MeshTypes.h"
#include "MeshChannel.h"
#include "MeshCryptoPKI.h"

namespace libmeshtastic_leaf {

/**
 * @brief Packet encoder/decoder utilities
 *
 * Provides functions for serializing and deserializing Meshtastic packets,
 * including both channel-encrypted and PKI-encrypted packets.
 */
class MeshPacketCodec {
public:
    /**
     * @brief Pack a packet header into a byte buffer
     *
     * Serializes a PacketHeader structure into the wire format.
     *
     * @param header Source header structure
     * @param buffer Output buffer (must be at least MESHTASTIC_HEADER_LENGTH bytes)
     */
    static void packHeader(const PacketHeader& header, uint8_t* buffer);

    /**
     * @brief Unpack a packet header from a byte buffer
     *
     * Deserializes a PacketHeader structure from the wire format.
     *
     * @param buffer Input buffer (must be at least MESHTASTIC_HEADER_LENGTH bytes)
     * @param header Output header structure
     */
    static void unpackHeader(const uint8_t* buffer, PacketHeader& header);

    /**
     * @brief Encode a complete packet for transmission (channel encryption)
     *
     * Creates a complete radio packet with header and encrypted payload.
     *
     * @param header Packet header (must have to, from, id set)
     * @param payload Plaintext payload
     * @param payloadLen Payload length
     * @param channel Channel to use for encryption
     * @param outBuffer Output buffer for complete packet
     * @param outLen Output: total packet length
     * @return true if encoding succeeded
     */
    static bool encodePacket(const PacketHeader& header,
                             const uint8_t* payload, size_t payloadLen,
                             MeshChannel& channel,
                             uint8_t* outBuffer, size_t& outLen);

    /**
     * @brief Encode a complete PKI packet for transmission
     *
     * Creates a complete radio packet with header and PKI-encrypted payload.
     *
     * @param header Packet header (must have to, from, id set; channel will be set to 0)
     * @param payload Plaintext payload
     * @param payloadLen Payload length
     * @param pki PKI crypto engine (must have private key set)
     * @param remotePubKey Destination node's public key
     * @param outBuffer Output buffer for complete packet
     * @param outLen Output: total packet length
     * @return true if encoding succeeded
     */
    static bool encodePacketPKI(PacketHeader& header,
                                const uint8_t* payload, size_t payloadLen,
                                MeshCryptoPKI& pki,
                                const uint8_t remotePubKey[32],
                                uint8_t* outBuffer, size_t& outLen);

    /**
     * @brief Decode a received packet (channel encryption)
     *
     * Parses header and decrypts payload using the provided channel.
     *
     * @param buffer Complete received packet
     * @param bufLen Packet length
     * @param channel Channel to use for decryption
     * @param outPacket Output decoded packet
     * @return Result of decoding operation
     */
    static ReceiveResult decodePacket(const uint8_t* buffer, size_t bufLen,
                                      MeshChannel& channel,
                                      MeshPacket& outPacket);

    /**
     * @brief Decode a received PKI packet
     *
     * Parses header and decrypts payload using PKI.
     *
     * @param buffer Complete received packet
     * @param bufLen Packet length
     * @param pki PKI crypto engine (must have private key set)
     * @param senderPubKey Sender's public key (32 bytes)
     * @param outPacket Output decoded packet
     * @return Result of decoding operation
     */
    static ReceiveResult decodePacketPKI(const uint8_t* buffer, size_t bufLen,
                                         MeshCryptoPKI& pki,
                                         const uint8_t senderPubKey[32],
                                         MeshPacket& outPacket);

    /**
     * @brief Check if a received packet uses PKI encryption
     *
     * PKI packets have channel == 0 and to != BROADCAST_ADDR
     *
     * @param buffer Packet buffer (at least header portion)
     * @param bufLen Buffer length
     * @return true if this appears to be a PKI packet
     */
    static bool isPKIPacket(const uint8_t* buffer, size_t bufLen);

    /**
     * @brief Check if channel hash matches for a received packet
     *
     * @param buffer Packet buffer
     * @param bufLen Buffer length
     * @param expectedHash Expected channel hash
     * @return true if the packet's channel hash matches
     */
    static bool matchesChannelHash(const uint8_t* buffer, size_t bufLen,
                                   ChannelHash expectedHash);

    /**
     * @brief Generate a new packet ID
     *
     * Creates a pseudo-random packet ID using the node number as a seed component.
     *
     * @param nodeNum This node's number
     * @return New packet ID
     */
    static PacketId generatePacketId(NodeNum nodeNum);

    /**
     * @brief Encode a Data protobuf message
     *
     * Creates the inner Data message that contains the application payload.
     *
     * @param portNum Application port number
     * @param payload Application payload
     * @param payloadLen Payload length
     * @param outBuffer Output buffer for encoded Data message
     * @param outLen Output: encoded message length
     * @return true if encoding succeeded
     */
    static bool encodeDataMessage(meshtastic_PortNum portNum,
                                  const uint8_t* payload, size_t payloadLen,
                                  uint8_t* outBuffer, size_t& outLen);

    /**
     * @brief Decode a Data protobuf message
     *
     * Parses the inner Data message to extract port number and payload.
     *
     * @param buffer Encoded Data message
     * @param bufLen Message length
     * @param outPortNum Output: application port number
     * @param outPayload Output buffer for payload
     * @param outPayloadLen Output: payload length
     * @return true if decoding succeeded
     */
    static bool decodeDataMessage(const uint8_t* buffer, size_t bufLen,
                                  meshtastic_PortNum& outPortNum,
                                  uint8_t* outPayload, size_t& outPayloadLen);

private:
    /// Counter for packet ID generation
    static uint32_t packetIdCounter_;
};

} // namespace libmeshtastic_leaf
