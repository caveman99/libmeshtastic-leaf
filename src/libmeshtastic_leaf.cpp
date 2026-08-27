
#include "libmeshtastic_leaf.h"
#include <string.h>

namespace libmeshtastic_leaf {

// RadioLib packet callbacks carry no context pointer, so the flag lives at
// file scope. Same constraint as LoRaWANNode: one active instance per process.
static volatile bool packetReceivedFlag = false;

static void onPacketReceived() { packetReceivedFlag = true; }

libmeshtastic_leaf::libmeshtastic_leaf()
    : phy_(nullptr), pkiKeyLookup_(nullptr), receiveCallback_(nullptr),
      initialized_(false), rxLen_(0), rxPending_(false), lastRssi_(0),
      lastSnr_(0.0f) {
  memset(rxBuffer_, 0, sizeof(rxBuffer_));
}

libmeshtastic_leaf::~libmeshtastic_leaf() { end(); }

bool libmeshtastic_leaf::begin(const MeshConfig &config, PhysicalLayer *phy) {
  if (phy == nullptr) {
    return false;
  }

  config_ = config;
  phy_ = phy;

  if (!applyRfConfig()) {
    return false;
  }

  setDefaultChannel();

  packetReceivedFlag = false;
  phy_->setPacketReceivedAction(onPacketReceived);
  if (phy_->startReceive() != RADIOLIB_ERR_NONE) {
    return false;
  }

  initialized_ = true;
  return true;
}

bool libmeshtastic_leaf::applyRfConfig() {
  const RadioConfig &rf = config_.radio;

  const bool wideLora = MeshRegion::isWideLoRa(rf.region);
  const ModemParams params = MeshRegion::getModemParams(rf.preset, wideLora);

  // SF, bandwidth and coding rate are set atomically. Every RadioLib LoRa
  // driver implements this, so no per-chip code is needed here.
  DataRate_t dr = {};
  dr.lora.spreadingFactor = params.sf;
  dr.lora.bandwidth = params.bw;
  dr.lora.codingRate = params.cr;
  if (phy_->setDataRate(dr, RADIOLIB_MODEM_LORA) != RADIOLIB_ERR_NONE) {
    return false;
  }

  const float freq = rf.frequency > 0.0f
                         ? rf.frequency
                         : MeshRegion::getDefaultFrequency(rf.region);
  if (phy_->setFrequency(freq) != RADIOLIB_ERR_NONE) {
    return false;
  }

  // A one-byte sync word maps to the LoRa sync word on every driver
  uint8_t syncWord = MESHTASTIC_SYNC_WORD;
  if (phy_->setSyncWord(&syncWord, 1) != RADIOLIB_ERR_NONE) {
    return false;
  }

  if (phy_->setPreambleLength(DEFAULT_PREAMBLE_LENGTH) != RADIOLIB_ERR_NONE) {
    return false;
  }

  int8_t power =
      rf.txPower != 0 ? rf.txPower : MeshRegion::getPowerLimit(rf.region);
  phy_->checkOutputPower(power, &power);
  return phy_->setOutputPower(power) == RADIOLIB_ERR_NONE;
}

void libmeshtastic_leaf::end() {
  if (phy_) {
    phy_->clearPacketReceivedAction();
    phy_->sleep();
  }
  initialized_ = false;
  phy_ = nullptr;
}

bool libmeshtastic_leaf::setChannel(const uint8_t *psk, size_t pskLen,
                                    const char *name) {
  return channel_.setChannel(psk, pskLen, name);
}

void libmeshtastic_leaf::setDefaultChannel() { channel_.setDefaultChannel(); }

ChannelHash libmeshtastic_leaf::getChannelHash() const {
  return channel_.getHash();
}

void libmeshtastic_leaf::generateKeyPair(uint8_t pubKey[32],
                                         uint8_t privKey[32]) {
  MeshCryptoPKI::generateKeyPair(pubKey, privKey);
}

void libmeshtastic_leaf::setMyPrivateKey(const uint8_t privKey[32]) {
  pki_.setPrivateKey(privKey);
}

void libmeshtastic_leaf::onReceivePKI(PKIKeyLookup callback) {
  pkiKeyLookup_ = callback;
}

uint32_t libmeshtastic_leaf::sendText(const char *text, NodeNum dest) {
  if (!initialized_ || text == nullptr) {
    return 0;
  }

  size_t textLen = strlen(text);
  return sendData(meshtastic_PortNum_TEXT_MESSAGE_APP, (const uint8_t *)text,
                  textLen, dest, false);
}

uint32_t libmeshtastic_leaf::sendData(meshtastic_PortNum port,
                                      const uint8_t *data, size_t len,
                                      NodeNum dest, bool wantAck) {
  if (!initialized_) {
    return 0;
  }

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

uint32_t libmeshtastic_leaf::sendTextPKI(const char *text, NodeNum destNode,
                                         const uint8_t remotePubKey[32]) {
  if (!initialized_ || text == nullptr) {
    return 0;
  }

  size_t textLen = strlen(text);
  return sendDataPKI(meshtastic_PortNum_TEXT_MESSAGE_APP, (const uint8_t *)text,
                     textLen, destNode, remotePubKey, false);
}

uint32_t libmeshtastic_leaf::sendDataPKI(meshtastic_PortNum port,
                                         const uint8_t *data, size_t len,
                                         NodeNum destNode,
                                         const uint8_t remotePubKey[32],
                                         bool wantAck) {
  if (!initialized_ || !pki_.hasPrivateKey()) {
    return 0;
  }

  PacketHeader header;
  memset(&header, 0, sizeof(header));
  header.to = destNode;
  header.from = config_.nodeNum;
  header.id = MeshPacketCodec::generatePacketId(config_.nodeNum);
  header.channel = 0; // PKI indicator
  header.setHopLimit(config_.hopLimit);
  header.setHopStart(config_.hopLimit);
  header.setWantAck(wantAck);

  return sendPacket(header, data, len, true, remotePubKey);
}

uint32_t libmeshtastic_leaf::sendPacket(PacketHeader &header,
                                        const uint8_t *payload, size_t len,
                                        bool usePKI,
                                        const uint8_t *remotePubKey) {
  if (!initialized_ || phy_ == nullptr) {
    return 0;
  }

  uint8_t dataMsg[MAX_ENCRYPTED_PAYLOAD];
  size_t dataMsgLen;
  if (!MeshPacketCodec::encodeDataMessage(meshtastic_PortNum_TEXT_MESSAGE_APP,
                                          payload, len, dataMsg, dataMsgLen)) {
    return 0;
  }

  uint8_t txBuffer[MAX_LORA_PAYLOAD_LEN];
  size_t txLen;

  if (usePKI && remotePubKey != nullptr) {
    if (!MeshPacketCodec::encodePacketPKI(header, dataMsg, dataMsgLen, pki_,
                                          remotePubKey, txBuffer, txLen)) {
      return 0;
    }
  } else {
    if (!MeshPacketCodec::encodePacket(header, dataMsg, dataMsgLen, channel_,
                                       txBuffer, txLen)) {
      return 0;
    }
  }

  if (phy_->transmit(txBuffer, txLen) != RADIOLIB_ERR_NONE) {
    return 0;
  }

  packetReceivedFlag = false;
  phy_->startReceive();

  return header.id;
}

void libmeshtastic_leaf::update() {
  if (!initialized_ || phy_ == nullptr) {
    return;
  }

  if (packetReceivedFlag && !rxPending_) {
    packetReceivedFlag = false;

    size_t len = phy_->getPacketLength();
    if (len > sizeof(rxBuffer_)) {
      len = sizeof(rxBuffer_);
    }

    if (len > 0 && phy_->readData(rxBuffer_, len) == RADIOLIB_ERR_NONE) {
      rxLen_ = len;
      lastRssi_ = phy_->getRSSI();
      lastSnr_ = phy_->getSNR();
      rxPending_ = true;

      if (receiveCallback_ != nullptr) {
        MeshPacket packet;
        if (receive(packet) == ReceiveResult::OK) {
          receiveCallback_(packet);
        }
      }
    }

    phy_->startReceive();
  }
}

bool libmeshtastic_leaf::available() { return rxPending_; }

ReceiveResult libmeshtastic_leaf::receive(MeshPacket &packet) {
  if (!rxPending_ || rxLen_ == 0) {
    return ReceiveResult::NO_PACKET;
  }

  if (rxLen_ < MESHTASTIC_HEADER_LENGTH) {
    rxPending_ = false;
    return ReceiveResult::TOO_SHORT;
  }

  if (MeshPacketCodec::isPKIPacket(rxBuffer_, rxLen_)) {
    if (pkiKeyLookup_ == nullptr) {
      rxPending_ = false;
      return ReceiveResult::PKI_KEY_UNKNOWN;
    }

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
      packet.rxRssi = lastRssi_;
      packet.rxSnr = lastSnr_;
      // rxTime is left unset; it would need millis() from Arduino.

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
    if (!MeshPacketCodec::matchesChannelHash(rxBuffer_, rxLen_,
                                             channel_.getHash())) {
      rxPending_ = false;
      return ReceiveResult::DECRYPT_FAILED;
    }

    ReceiveResult result =
        MeshPacketCodec::decodePacket(rxBuffer_, rxLen_, channel_, packet);

    if (result == ReceiveResult::OK) {
      packet.rxRssi = lastRssi_;
      packet.rxSnr = lastSnr_;

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
  // transmit() is blocking today, so this is never true from the outside.
  // ponytail: becomes real when the MAC layer moves to startTransmit().
  return false;
}

} // namespace libmeshtastic_leaf
