
#include "libmeshtastic_leaf.h"
#include <string.h>

namespace libmeshtastic_leaf {

// RadioLib packet callbacks carry no context pointer, so the flag lives at
// file scope. One active instance per process.
//
// Sent and received share one hook: on SX126x both setPacketReceivedAction()
// and setPacketSentAction() install the same DIO1 action, so a single flag is
// disambiguated by the transmit state.
static volatile bool radioIrqFlag = false;

static void onRadioIrq() { radioIrqFlag = true; }

libmeshtastic_leaf::libmeshtastic_leaf()
    : phy_(nullptr), pkiKeyLookup_(nullptr), receiveCallback_(nullptr),
      initialized_(false), rxLen_(0), rxPending_(false), lastRssi_(0),
      lastSnr_(0.0f), lastSendResult_(SendResult::OK), dutyCycleLimit_(100.0f),
      carrierSense_(true), cwMin_(3), cwMax_(8), slotTimeMsec_(0),
      txState_(TxState::IDLE), txLen_(0), txAirtimeMsec_(0),
      backoffUntilMsec_(0), txDeadlineMsec_(0), reliableBroadcastAttempts_(3),
      reliableUnicastAttempts_(5) {
  memset(&pending_, 0, sizeof(pending_));
  memset(rxBuffer_, 0, sizeof(rxBuffer_));
}

libmeshtastic_leaf::~libmeshtastic_leaf() { end(); }

bool libmeshtastic_leaf::begin(const MeshConfig &config, PhysicalLayer *phy) {
  if (phy == nullptr) {
    return false;
  }

  config_ = config;
  phy_ = phy;

  setDefaultChannel();

  if (!applyRfConfig()) {
    return false;
  }

  slotTimeMsec_ = computeSlotTimeMsec();
  airtime_.reset(millis());
  history_.clear();
  stopRetransmission();
  txState_ = TxState::IDLE;

  const RegionInfo *region = MeshRegion::getRegion(config_.radio.region);
  dutyCycleLimit_ = region ? region->dutyCycle : 100.0f;

  radioIrqFlag = false;
  phy_->setPacketReceivedAction(onRadioIrq);
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

  // An explicit frequency wins, then an explicit one-based slot, otherwise the
  // region picks a slot from the channel name.
  float freq = rf.frequency;
  if (freq <= 0.0f) {
    freq = rf.channelNum > 0 ? MeshRegion::getFrequencyForSlot(
                                   rf.region, rf.preset, rf.channelNum)
                             : MeshRegion::getFrequency(rf.region, rf.preset,
                                                        channel_.getName());
  }
  freq += rf.frequencyOffset;
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

  if (txState_ != TxState::IDLE) {
    lastSendResult_ = SendResult::TX_BUSY;
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

  const uint32_t airMsec = phy_->getTimeOnAir(txLen) / 1000;

  if (dutyCycleLimit_ < 100.0f) {
    airtime_.advance(millis());
    const float wouldBe = (float)(airtime_.txMsecLastHour() + airMsec) *
                          100.0f / (float)MeshAirtime::MSEC_PER_HOUR;
    if (wouldBe > dutyCycleLimit_) {
      lastSendResult_ = SendResult::DUTY_CYCLE;
      return 0;
    }
  }

  memcpy(txBuffer_, txBuffer, txLen);
  txLen_ = txLen;
  txAirtimeMsec_ = airMsec;
  if (header.wantAck()) {
    startRetransmission(txBuffer, txLen, header.id, header.to, airMsec);
  } else {
    stopRetransmission();
  }
  deferPendingAck(airMsec);
  backoffUntilMsec_ = millis() + computeBackoffMsec();
  txState_ = TxState::BACKOFF;
  lastSendResult_ = SendResult::OK;

  return header.id;
}

uint32_t libmeshtastic_leaf::computeSlotTimeMsec() const {
  const bool wide = MeshRegion::isWideLoRa(config_.radio.region);
  const ModemParams p = MeshRegion::getModemParams(config_.radio.preset, wide);

  // Propagation, Tx/Rx turnaround and MAC processing, per the firmware.
  const float overheadMsec = 0.2f + 0.4f + 7.0f;
  const float symbolMsec = (float)(1UL << p.sf) / p.bw;

  if (wide) {
    return (uint32_t)((4 + (2 * p.sf + 3) / 32) * symbolMsec + overheadMsec);
  }
  return (uint32_t)(2.5f * symbolMsec + overheadMsec);
}

uint32_t libmeshtastic_leaf::computeBackoffMsec() {
  if (!carrierSense_) {
    return 0;
  }
  airtime_.advance(millis());
  float util = airtime_.channelUtilizationPercent();
  if (util > 100.0f) {
    util = 100.0f;
  }
  const uint8_t cw = cwMin_ + (uint8_t)((util / 100.0f) * (cwMax_ - cwMin_));
  const uint32_t slots = 1UL << cw;
  return (uint32_t)(rand() % slots) * slotTimeMsec_;
}

void libmeshtastic_leaf::serviceTx() {
  if (txState_ == TxState::BACKOFF) {
    if ((int32_t)(millis() - backoffUntilMsec_) < 0) {
      return;
    }

    // Someone else is mid-transmission: draw a fresh backoff and wait again.
    if (carrierSense_ && phy_->scanChannel() == RADIOLIB_LORA_DETECTED) {
      backoffUntilMsec_ = millis() + computeBackoffMsec();
      phy_->startReceive();
      return;
    }

    radioIrqFlag = false;
    if (phy_->startTransmit(txBuffer_, txLen_) != RADIOLIB_ERR_NONE) {
      lastSendResult_ = SendResult::RADIO_ERROR;
      finishTx();
      return;
    }

    txState_ = TxState::SENDING;
    txDeadlineMsec_ = millis() + txAirtimeMsec_ * 2 + 500;
    return;
  }

  if (txState_ == TxState::SENDING) {
    // The deadline is a backstop for a missed interrupt, not the normal path.
    if (!radioIrqFlag && (int32_t)(millis() - txDeadlineMsec_) < 0) {
      return;
    }
    radioIrqFlag = false;
    phy_->finishTransmit();
    airtime_.logTx(millis(), txAirtimeMsec_);
    finishTx();
  }
}

void libmeshtastic_leaf::finishTx() {
  txState_ = TxState::IDLE;
  txLen_ = 0;
  radioIrqFlag = false;
  phy_->startReceive();
}

void libmeshtastic_leaf::setReliableAttempts(uint8_t broadcast,
                                             uint8_t unicast) {
  reliableBroadcastAttempts_ = broadcast;
  reliableUnicastAttempts_ = unicast;
}

// Long enough for the packet to be relayed and an ack to come back, per the
// firmware: twice the airtime, a spread of contention slots, and the time a
// node needs to turn a packet around.
uint32_t
libmeshtastic_leaf::retransmissionDelayMsec(uint32_t airtimeMsec) const {
  float util = airtime_.channelUtilizationPercent();
  if (util > 100.0f) {
    util = 100.0f;
  }
  const uint8_t cw = cwMin_ + (uint8_t)((util / 100.0f) * (cwMax_ - cwMin_));
  const uint32_t slots =
      (1UL << cw) + (2UL * cwMax_) + (1UL << ((cwMax_ + cwMin_) / 2));
  return (2UL * airtimeMsec) + (slots * slotTimeMsec_) + PROCESSING_TIME_MSEC;
}

void libmeshtastic_leaf::startRetransmission(const uint8_t *frame, size_t len,
                                             PacketId id, NodeNum to,
                                             uint32_t airtimeMsec) {
  const uint8_t attempts = (to == BROADCAST_ADDR) ? reliableBroadcastAttempts_
                                                  : reliableUnicastAttempts_;
  if (attempts <= 1 || len > sizeof(pending_.buffer)) {
    return;
  }

  memcpy(pending_.buffer, frame, len);
  pending_.len = len;
  pending_.id = id;
  pending_.to = to;
  pending_.airtimeMsec = airtimeMsec;
  pending_.attemptsLeft = attempts - 1;
  pending_.nextTxMsec = millis() + retransmissionDelayMsec(airtimeMsec);
}

void libmeshtastic_leaf::stopRetransmission() { pending_.attemptsLeft = 0; }

// While the radio is busy with another packet an ack cannot arrive, so push
// the retry out by that packet's airtime rather than retrying too early.
void libmeshtastic_leaf::deferPendingAck(uint32_t airtimeMsec) {
  if (pending_.attemptsLeft > 0) {
    pending_.nextTxMsec += airtimeMsec;
  }
}

void libmeshtastic_leaf::serviceRetransmission() {
  if (pending_.attemptsLeft == 0 || txState_ != TxState::IDLE) {
    return;
  }
  if ((int32_t)(millis() - pending_.nextTxMsec) < 0) {
    return;
  }
  if (dutyCycleLimit_ < 100.0f) {
    airtime_.advance(millis());
    const float wouldBe =
        (float)(airtime_.txMsecLastHour() + pending_.airtimeMsec) * 100.0f /
        (float)MeshAirtime::MSEC_PER_HOUR;
    if (wouldBe > dutyCycleLimit_) {
      stopRetransmission();
      return;
    }
  }

  memcpy(txBuffer_, pending_.buffer, pending_.len);
  txLen_ = pending_.len;
  txAirtimeMsec_ = pending_.airtimeMsec;
  backoffUntilMsec_ = millis() + computeBackoffMsec();
  txState_ = TxState::BACKOFF;

  pending_.attemptsLeft--;
  pending_.nextTxMsec =
      millis() + retransmissionDelayMsec(pending_.airtimeMsec);
}

// Runs on the raw header, before any decryption, so it also covers packets
// this node has no key for.
bool libmeshtastic_leaf::filterReceived(uint32_t nowMsec) {
  PacketHeader header;
  MeshPacketCodec::unpackHeader(rxBuffer_, header);

  // Hearing our own packet relayed proves it reached the mesh, which is the
  // implicit ack. The header alone identifies it.
  if (header.from == config_.nodeNum) {
    if (pending_.attemptsLeft > 0 && pending_.id == header.id) {
      stopRetransmission();
    }
    return true;
  }

  if (history_.wasSeen(header.from, header.id, nowMsec)) {
    return true;
  }

  deferPendingAck(phy_->getTimeOnAir(rxLen_) / 1000);
  return false;
}

Airtime libmeshtastic_leaf::getAirtime() {
  airtime_.advance(millis());
  Airtime out;
  out.txMsecLastHour = airtime_.txMsecLastHour();
  out.txUtilizationPercent = airtime_.txUtilizationPercent();
  out.channelUtilizationPercent = airtime_.channelUtilizationPercent();
  return out;
}

uint32_t libmeshtastic_leaf::getTimeOnAir(size_t payloadLen) const {
  if (phy_ == nullptr) {
    return 0;
  }
  return phy_->getTimeOnAir(MESHTASTIC_HEADER_LENGTH + payloadLen) / 1000;
}

void libmeshtastic_leaf::setCarrierSense(bool enabled, uint8_t cwMin,
                                         uint8_t cwMax) {
  carrierSense_ = enabled;
  cwMin_ = cwMin;
  cwMax_ = cwMax > cwMin ? cwMax : cwMin;
}

void libmeshtastic_leaf::update() {
  if (!initialized_ || phy_ == nullptr) {
    return;
  }

  serviceTx();
  serviceRetransmission();

  if (txState_ != TxState::IDLE) {
    return;
  }

  if (radioIrqFlag && !rxPending_) {
    radioIrqFlag = false;

    size_t len = phy_->getPacketLength();
    if (len > sizeof(rxBuffer_)) {
      len = sizeof(rxBuffer_);
    }

    if (len > 0 && phy_->readData(rxBuffer_, len) == RADIOLIB_ERR_NONE) {
      rxLen_ = len;
      lastRssi_ = phy_->getRSSI();
      lastSnr_ = phy_->getSNR();
      airtime_.logRx(millis(), phy_->getTimeOnAir(len) / 1000);

      if (filterReceived(millis())) {
        rxLen_ = 0;
        phy_->startReceive();
        return;
      }

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
  return txState_ != TxState::IDLE;
}

} // namespace libmeshtastic_leaf
