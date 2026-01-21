/**
 * @file libmeshtastic_leaf.cpp
 * @brief Implementation of main Meshtastic Leaf API
 */

#include "libmeshtastic_leaf.h"
#include <string.h>

namespace libmeshtastic_leaf {

libmeshtastic_leaf::libmeshtastic_leaf()
    : radio_(nullptr),
      pkiKeyLookup_(nullptr),
      receiveCallback_(nullptr),
      initialized_(false),
      rxLen_(0),
      rxPending_(false) {
    memset(rxBuffer_, 0, sizeof(rxBuffer_));
}

libmeshtastic_leaf::~libmeshtastic_leaf() {
    end();
}

bool libmeshtastic_leaf::begin(const MeshConfig& config, MeshRadio* radio) {
    if (radio == nullptr) {
        return false;
    }

    config_ = config;
    radio_ = radio;

    // Initialize with default channel
    setDefaultChannel();

    // Start receiving
    if (!radio_->startReceive()) {
        return false;
    }

    initialized_ = true;
    return true;
}

void libmeshtastic_leaf::end() {
    if (radio_) {
        radio_->sleep();
    }
    initialized_ = false;
    radio_ = nullptr;
}

bool libmeshtastic_leaf::setChannel(const uint8_t* psk, size_t pskLen, const char* name) {
    return channel_.setChannel(psk, pskLen, name);
}

void libmeshtastic_leaf::setDefaultChannel() {
    channel_.setDefaultChannel();
}

ChannelHash libmeshtastic_leaf::getChannelHash() const {
    return channel_.getHash();
}

void libmeshtastic_leaf::generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]) {
    MeshCryptoPKI::generateKeyPair(pubKey, privKey);
}

void libmeshtastic_leaf::setMyPrivateKey(const uint8_t privKey[32]) {
    pki_.setPrivateKey(privKey);
}

void libmeshtastic_leaf::onReceivePKI(PKIKeyLookup callback) {
    pkiKeyLookup_ = callback;
}

uint32_t libmeshtastic_leaf::sendText(const char* text, NodeNum dest) {
    if (!initialized_ || text == nullptr) {
        return 0;
    }

    size_t textLen = strlen(text);
    return sendData(meshtastic_PortNum_TEXT_MESSAGE_APP,
                    (const uint8_t*)text, textLen, dest, false);
}

uint32_t libmeshtastic_leaf::sendData(meshtastic_PortNum port, const uint8_t* data, size_t len,
                                  NodeNum dest, bool wantAck) {
    if (!initialized_) {
        return 0;
    }

    // Create header
    PacketHeader header;
    memset(&header, 0, sizeof(header));
    header.to = dest;
    header.from = config_.nodeNum;
    header.id = MeshPacketCodec::generatePacketId(config_.nodeNum);
    header.channel = channel_.getHash();
    header.setHopLimit(config_.hopLimit);
    header.setHopStart(config_.hopLimit);
    header.setWantAck(wantAck);

    return sendPacket(header, data, len, false, nullptr);
}

uint32_t libmeshtastic_leaf::sendTextPKI(const char* text, NodeNum destNode,
                                     const uint8_t remotePubKey[32]) {
    if (!initialized_ || text == nullptr) {
        return 0;
    }

    size_t textLen = strlen(text);
    return sendDataPKI(meshtastic_PortNum_TEXT_MESSAGE_APP,
                       (const uint8_t*)text, textLen,
                       destNode, remotePubKey, false);
}

uint32_t libmeshtastic_leaf::sendDataPKI(meshtastic_PortNum port, const uint8_t* data, size_t len,
                                     NodeNum destNode, const uint8_t remotePubKey[32],
                                     bool wantAck) {
    if (!initialized_ || !pki_.hasPrivateKey()) {
        return 0;
    }

    // Create header (PKI uses channel = 0)
    PacketHeader header;
    memset(&header, 0, sizeof(header));
    header.to = destNode;
    header.from = config_.nodeNum;
    header.id = MeshPacketCodec::generatePacketId(config_.nodeNum);
    header.channel = 0;  // PKI indicator
    header.setHopLimit(config_.hopLimit);
    header.setHopStart(config_.hopLimit);
    header.setWantAck(wantAck);

    return sendPacket(header, data, len, true, remotePubKey);
}

uint32_t libmeshtastic_leaf::sendPacket(PacketHeader& header, const uint8_t* payload, size_t len,
                                    bool usePKI, const uint8_t* remotePubKey) {
    if (!initialized_ || radio_ == nullptr) {
        return 0;
    }

    // Encode the Data message
    uint8_t dataMsg[MAX_ENCRYPTED_PAYLOAD];
    size_t dataMsgLen;
    if (!MeshPacketCodec::encodeDataMessage(meshtastic_PortNum_TEXT_MESSAGE_APP,
                                            payload, len, dataMsg, dataMsgLen)) {
        return 0;
    }

    // Build complete packet
    uint8_t txBuffer[MAX_LORA_PAYLOAD_LEN];
    size_t txLen;

    if (usePKI && remotePubKey != nullptr) {
        // PKI encryption
        if (!MeshPacketCodec::encodePacketPKI(header, dataMsg, dataMsgLen,
                                               pki_, remotePubKey, txBuffer, txLen)) {
            return 0;
        }
    } else {
        // Channel encryption
        if (!MeshPacketCodec::encodePacket(header, dataMsg, dataMsgLen,
                                           channel_, txBuffer, txLen)) {
            return 0;
        }
    }

    // Transmit
    if (!radio_->transmit(txBuffer, txLen)) {
        return 0;
    }

    // Restart receive mode
    radio_->startReceive();

    return header.id;
}

void libmeshtastic_leaf::update() {
    if (!initialized_ || radio_ == nullptr) {
        return;
    }

    // Check for received packet
    if (radio_->available() && !rxPending_) {
        if (radio_->readPacket(rxBuffer_, sizeof(rxBuffer_), rxLen_)) {
            rxPending_ = true;

            // If callback is set, process immediately
            if (receiveCallback_ != nullptr) {
                MeshPacket packet;
                if (receive(packet) == ReceiveResult::OK) {
                    receiveCallback_(packet);
                }
            }
        }

        // Restart receive mode
        radio_->startReceive();
    }
}

bool libmeshtastic_leaf::available() {
    return rxPending_;
}

ReceiveResult libmeshtastic_leaf::receive(MeshPacket& packet) {
    if (!rxPending_ || rxLen_ == 0) {
        return ReceiveResult::NO_PACKET;
    }

    // Check minimum size
    if (rxLen_ < MESHTASTIC_HEADER_LENGTH) {
        rxPending_ = false;
        return ReceiveResult::TOO_SHORT;
    }

    // Check if PKI packet
    if (MeshPacketCodec::isPKIPacket(rxBuffer_, rxLen_)) {
        // PKI decryption
        if (pkiKeyLookup_ == nullptr) {
            rxPending_ = false;
            return ReceiveResult::PKI_KEY_UNKNOWN;
        }

        // Get sender's public key
        PacketHeader tempHeader;
        MeshPacketCodec::unpackHeader(rxBuffer_, tempHeader);

        uint8_t senderPubKey[32];
        if (!pkiKeyLookup_(tempHeader.from, senderPubKey)) {
            rxPending_ = false;
            return ReceiveResult::PKI_KEY_UNKNOWN;
        }

        ReceiveResult result = MeshPacketCodec::decodePacketPKI(
            rxBuffer_, rxLen_, pki_, senderPubKey, packet);

        if (result == ReceiveResult::OK) {
            packet.rxRssi = radio_->getRSSI();
            packet.rxSnr = radio_->getSNR();
            // Note: packet.rxTime would need millis() from Arduino

            // Decode the Data message
            meshtastic_PortNum portNum;
            uint8_t innerPayload[MAX_ENCRYPTED_PAYLOAD];
            size_t innerLen;
            if (MeshPacketCodec::decodeDataMessage(packet.payload, packet.payloadLen,
                                                   portNum, innerPayload, innerLen)) {
                packet.portNum = portNum;
                memcpy(packet.payload, innerPayload, innerLen);
                packet.payloadLen = innerLen;
            }
        }

        rxPending_ = false;
        return result;
    } else {
        // Channel decryption - check hash matches
        if (!MeshPacketCodec::matchesChannelHash(rxBuffer_, rxLen_, channel_.getHash())) {
            rxPending_ = false;
            return ReceiveResult::DECRYPT_FAILED;
        }

        ReceiveResult result = MeshPacketCodec::decodePacket(
            rxBuffer_, rxLen_, channel_, packet);

        if (result == ReceiveResult::OK) {
            packet.rxRssi = radio_->getRSSI();
            packet.rxSnr = radio_->getSNR();

            // Decode the Data message
            meshtastic_PortNum portNum;
            uint8_t innerPayload[MAX_ENCRYPTED_PAYLOAD];
            size_t innerLen;
            if (MeshPacketCodec::decodeDataMessage(packet.payload, packet.payloadLen,
                                                   portNum, innerPayload, innerLen)) {
                packet.portNum = portNum;
                memcpy(packet.payload, innerPayload, innerLen);
                packet.payloadLen = innerLen;
            }
        }

        rxPending_ = false;
        return result;
    }
}

void libmeshtastic_leaf::onReceive(PacketCallback callback) {
    receiveCallback_ = callback;
}

bool libmeshtastic_leaf::isTransmitting() const {
    return radio_ != nullptr && radio_->isTransmitting();
}

} // namespace libmeshtastic_leaf
