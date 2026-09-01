
#pragma once

#include "MeshAirtime.h"
#include "MeshChannel.h"
#include "MeshCryptoPKI.h"
#include "MeshNodeId.h"
#include "MeshPacket.h"
#include "MeshPacketHistory.h"
#include "MeshRandom.h"
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

  // A keypair the mesh will accept. False means the entropy source failed and
  // the buffers hold nothing usable. Storage is the application's job.
  static bool generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]);

  // Check a key loaded back from storage before trusting it.
  static bool isUsablePublicKey(const uint8_t pubKey[32]);

  // Also sets the node number, which is the key's CRC-32. RAM only; storage
  // stays with the application.
  void setMyPublicKey(const uint8_t pubKey[32]);
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

  bool available() const;
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

  // Attempts for a want_ack send, counting the first. The firmware uses 3 for
  // broadcast and 5 for unicast. One disables retransmission.
  void setReliableAttempts(uint8_t broadcast, uint8_t unicast);

  // A send awaiting an ack is still outstanding.
  bool hasPendingAck() const { return pending_.attemptsLeft > 0; }

  // Answer a direct message that asked for an ack. On by default; a node that
  // stays silent makes the sender burn its retransmissions.
  void setSendAcks(bool enabled) { sendAcks_ = enabled; }

  // Name this node announces. Long name is truncated to 40 bytes and short to
  // 4, matching the protobuf limits.
  void setOwner(const char *longName, const char *shortName);

  // Broadcast a NodeInfo with the name and public key. update() already does
  // this on schedule; call it to announce early, such as on request.
  uint32_t sendNodeInfo();

  // How often update() announces. The firmware defaults to three hours and
  // refuses anything under an hour; zero stops the periodic announcement.
  void setNodeInfoInterval(uint32_t seconds);

  // Zero until a public key is set, since 2.8 derives the number from the
  // key and a node number of zero is refused by every receiver.
  NodeNum getNodeNum() const { return hasPublicKey_ ? nodeNum_ : 0; }
  bool hasIdentity() const { return hasPublicKey_; }
  MeshChannel &getChannel() { return channel_; }

private:
  enum class TxState { IDLE, BACKOFF, SENDING };

  struct PendingTx {
    PacketId id;
    NodeNum to;
    uint8_t attemptsLeft;
    uint32_t nextTxMsec;
    uint32_t airtimeMsec;
    size_t len;
    uint8_t buffer[MAX_LORA_PAYLOAD_LEN];
  };

  bool applyRfConfig();
  const char *resolveChannelName(const char *name) const;
  uint32_t retransmissionDelayMsec(uint32_t airtimeMsec) const;
  void startRetransmission(const uint8_t *frame, size_t len, PacketId id,
                           NodeNum to, uint32_t airtimeMsec);
  void stopRetransmission();
  void deferPendingAck(uint32_t airtimeMsec);
  void serviceRetransmission();
  // Header only, so it works on packets this node cannot decrypt.
  bool filterReceived(uint32_t nowMsec);
  void serviceTx();
  void finishTx();
  uint32_t computeSlotTimeMsec() const;
  uint32_t computeBackoffMsec();
  uint32_t sendPacket(PacketHeader &header, meshtastic_PortNum portNum,
                      const uint8_t *payload, size_t len, PacketId requestId,
                      bool usePKI, const uint8_t *remotePubKey);
  void handleDecoded(const MeshPacket &packet);
  void serviceNodeInfo();
  void queueAck(NodeNum to, PacketId requestId);
  bool serviceAck();

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
  MeshRandom rng_;
  MeshPacketHistory history_;
  PendingTx pending_;
  uint8_t reliableBroadcastAttempts_;
  uint8_t reliableUnicastAttempts_;
  bool sendAcks_;
  NodeNum nodeNum_; ///< Derived from the public key, not configured

  // Identity. The key is the application's to generate and store; this is a
  // working copy.
  uint8_t publicKey_[32];
  bool hasPublicKey_;
  char longName_[41];
  char shortName_[5];
  uint32_t nodeInfoIntervalMsec_;
  uint32_t nextNodeInfoMsec_;

  // Acks get their own slot so an application frame already staged cannot
  // delay one. They are a few bytes and the sender is counting retries.
  struct PendingAck {
    bool queued;
    NodeNum to;
    PacketId requestId;
  } ack_;
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
