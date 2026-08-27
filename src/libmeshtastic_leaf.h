
#pragma once

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

  NodeNum getNodeNum() const { return config_.nodeNum; }
  PhysicalLayer *getRadio() { return phy_; }
  MeshChannel &getChannel() { return channel_; }
  MeshCryptoPKI &getPKI() { return pki_; }

private:
  bool applyRfConfig();
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
};

} // namespace libmeshtastic_leaf
