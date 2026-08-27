
#pragma once

#include "MeshAirtime.h"
#include "MeshChannel.h"
#include "MeshCryptoPKI.h"
#include "MeshNodeId.h"
#include "MeshPacket.h"
#include "MeshRegion.h"
#include "MeshTypes.h"

// Any RadioLib LoRa radio is driven through the generic PhysicalLayer API,
// so this library needs no per-chip driver of its own.
#include <RadioLib.h>

namespace libmeshtastic_leaf {

class libmeshtastic_leaf {
public:
  libmeshtastic_leaf();
  ~libmeshtastic_leaf();

  // The caller brings up the radio first, including LoRa CRC, which
  // Meshtastic requires and PhysicalLayer does not expose.
  bool begin(const MeshConfig &config, PhysicalLayer *phy);
  void end();
  void update();

  bool setChannel(const uint8_t *psk, size_t pskLen, const char *name = "");
  void setDefaultChannel();
  ChannelHash getChannelHash() const;

  static void generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]);
  void setMyPrivateKey(const uint8_t privKey[32]);
  void onReceivePKI(PKIKeyLookup callback);

  // All four return the packet id, or 0 on failure.
  uint32_t sendText(const char *text, NodeNum dest = BROADCAST_ADDR);
  uint32_t sendData(meshtastic_PortNum port, const uint8_t *data, size_t len,
                    NodeNum dest = BROADCAST_ADDR, bool wantAck = false);
  uint32_t sendTextPKI(const char *text, NodeNum destNode,
                       const uint8_t remotePubKey[32]);
  uint32_t sendDataPKI(meshtastic_PortNum port, const uint8_t *data, size_t len,
                       NodeNum destNode, const uint8_t remotePubKey[32],
                       bool wantAck = false);

  bool available();
  ReceiveResult receive(MeshPacket &packet);
  void onReceive(PacketCallback callback);
  bool isTransmitting() const;

  // Why the last send was refused. Cleared by the next send.
  SendResult getLastSendResult() const { return lastSendResult_; }

  Airtime getAirtime();
  uint32_t getTimeOnAir(size_t payloadLen) const;

  // Transmissions are refused above this share of the last hour. Defaults to
  // the configured region's limit; 100 disables the gate.
  void setDutyCycleLimit(float percent) { dutyCycleLimit_ = percent; }
  float getDutyCycleLimit() const { return dutyCycleLimit_; }

  void setCarrierSense(bool enabled, uint8_t cwMin = 3, uint8_t cwMax = 8);

  NodeNum getNodeNum() const { return config_.nodeNum; }
  PhysicalLayer *getRadio() { return phy_; }
  MeshChannel &getChannel() { return channel_; }
  MeshCryptoPKI &getPKI() { return pki_; }

private:
  enum class TxState { IDLE, BACKOFF, SENDING };

  bool applyRfConfig();
  void serviceTx();
  void finishTx();
  uint32_t computeSlotTimeMsec() const;
  uint32_t computeBackoffMsec();
  uint32_t sendPacket(PacketHeader &header, const uint8_t *payload, size_t len,
                      bool usePKI, const uint8_t *remotePubKey);

  MeshConfig config_;
  PhysicalLayer *phy_;
  MeshChannel channel_;
  MeshCryptoPKI pki_;
  PKIKeyLookup pkiKeyLookup_;
  PacketCallback receiveCallback_;
  bool initialized_;

  uint8_t rxBuffer_[MAX_LORA_PAYLOAD_LEN];
  size_t rxLen_;
  bool rxPending_;
  int16_t lastRssi_;
  float lastSnr_;

  MeshAirtime airtime_;
  SendResult lastSendResult_;
  float dutyCycleLimit_;
  bool carrierSense_;
  uint8_t cwMin_;
  uint8_t cwMax_;
  uint32_t slotTimeMsec_;

  // One pending frame is enough for a leaf; a second send is refused until
  // this one is on the air.
  TxState txState_;
  uint8_t txBuffer_[MAX_LORA_PAYLOAD_LEN];
  size_t txLen_;
  uint32_t txAirtimeMsec_;
  uint32_t backoffUntilMsec_;
  uint32_t txDeadlineMsec_;
};

} // namespace libmeshtastic_leaf
