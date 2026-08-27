/**
 * @file libmeshtastic_leaf.h
 * @brief Main public API for Meshtastic Leaf library
 *
 * This is the main header file that users should include in their projects.
 * It provides a simple API for sending and receiving Meshtastic packets
 * as a leaf node (no routing or rebroadcasting).
 *
 * Basic usage:
 * @code
 * #include <libmeshtastic_leaf.h>
 *
 * libmeshtastic_leaf::libmeshtastic_leaf mesh;
 *
 * SX1262 radio(new Module(NSS, DIO1, RST, BUSY));
 *
 * void setup() {
 *     // the sketch owns chip-specific bring-up: pins, TCXO, CRC, current
 *     // limit, RF switch. Meshtastic requires LoRa CRC to be enabled.
 *     radio.begin();
 *     radio.setCRC(2);
 *
 *     libmeshtastic_leaf::MeshConfig config;
 *     // Get node number from hardware (MAC address)
 *     config.nodeNum = libmeshtastic_leaf::MeshNodeId::getNodeNum();
 *     config.radio.region = libmeshtastic_leaf::REGION_EU_868;
 *     config.radio.preset = libmeshtastic_leaf::PRESET_LONG_FAST;
 *
 *     // the library applies frequency, SF/BW/CR, sync word, preamble and
 *     // power itself, through the generic RadioLib PhysicalLayer API
 *     mesh.begin(config, &radio);
 *     mesh.setDefaultChannel();
 * }
 *
 * void loop() {
 *     mesh.update();
 *
 *     if (mesh.available()) {
 *         libmeshtastic_leaf::MeshPacket packet;
 *         if (mesh.receive(packet) == libmeshtastic_leaf::ReceiveResult::OK) {
 *             // Process packet
 *         }
 *     }
 *
 *     mesh.sendText("Hello Mesh!");
 * }
 * @endcode
 */

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

/**
 * @brief Main Meshtastic Leaf library class
 *
 * Provides a simple interface for Meshtastic leaf node operation:
 * - Send and receive text and data messages
 * - Support for channel-based (PSK) encryption
 * - Support for PKI (public key) encryption
 * - No routing or rebroadcasting (leaf node only)
 */
class libmeshtastic_leaf {
public:
  libmeshtastic_leaf();
  ~libmeshtastic_leaf();

  // ========================================================================
  // Initialization
  // ========================================================================

  /**
   * @brief Initialize the library with a radio driver
   *
   * The caller is responsible for constructing the RadioLib radio and doing
   * the chip-specific bring-up first (pins, TCXO, current limit, RF switch,
   * and enabling LoRa CRC, which Meshtastic requires). This method then
   * applies the Meshtastic RF policy for the configured region and preset:
   * frequency, spreading factor, bandwidth, coding rate, sync word, preamble
   * length and transmit power.
   *
   * @param config Library configuration (node number, region, preset)
   * @param phy Pointer to an already-initialized RadioLib LoRa radio
   * @return true if initialization succeeded
   */
  bool begin(const MeshConfig &config, PhysicalLayer *phy);

  /**
   * @brief Shut down the library and radio
   */
  void end();

  // ========================================================================
  // Channel Configuration
  // ========================================================================

  /**
   * @brief Set the channel with PSK and name
   *
   * @param psk Pre-shared key bytes
   * @param pskLen PSK length (0=none, 1=index, 16=AES128, 32=AES256)
   * @param name Channel name (optional, used for hash calculation)
   * @return true if channel was configured
   */
  bool setChannel(const uint8_t *psk, size_t pskLen, const char *name = "");

  /**
   * @brief Set to the default public channel
   *
   * Uses the default Meshtastic PSK that all nodes share out of the box.
   */
  void setDefaultChannel();

  /**
   * @brief Get the current channel hash
   *
   * @return Channel hash (0-255)
   */
  ChannelHash getChannelHash() const;

  // ========================================================================
  // PKI Key Management
  // ========================================================================

  /**
   * @brief Generate a new Curve25519 keypair
   *
   * Creates a random keypair for PKI encryption. The keys are returned
   * to the caller for storage (the library does not persist them).
   *
   * @param pubKey Output: 32-byte public key
   * @param privKey Output: 32-byte private key
   */
  static void generateKeyPair(uint8_t pubKey[32], uint8_t privKey[32]);

  /**
   * @brief Set this node's private key for PKI operations
   *
   * Must be called before sending/receiving PKI-encrypted messages.
   *
   * @param privKey 32-byte private key
   */
  void setMyPrivateKey(const uint8_t privKey[32]);

  /**
   * @brief Set the callback for looking up remote public keys
   *
   * This callback is invoked when decrypting PKI packets to get
   * the sender's public key.
   *
   * @param callback Function that returns true if key was found
   */
  void onReceivePKI(PKIKeyLookup callback);

  // ========================================================================
  // Transmission - Channel Encrypted
  // ========================================================================

  /**
   * @brief Send a text message
   *
   * Sends a UTF-8 text message encrypted with the current channel PSK.
   *
   * @param text Text to send (null-terminated)
   * @param dest Destination node (BROADCAST_ADDR for all nodes)
   * @return Packet ID if sent successfully, 0 on error
   */
  uint32_t sendText(const char *text, NodeNum dest = BROADCAST_ADDR);

  /**
   * @brief Send a data message
   *
   * Sends arbitrary data encrypted with the current channel PSK.
   *
   * @param port Application port number
   * @param data Data to send
   * @param len Data length
   * @param dest Destination node (BROADCAST_ADDR for all nodes)
   * @param wantAck Request acknowledgment
   * @return Packet ID if sent successfully, 0 on error
   */
  uint32_t sendData(meshtastic_PortNum port, const uint8_t *data, size_t len,
                    NodeNum dest = BROADCAST_ADDR, bool wantAck = false);

  // ========================================================================
  // Transmission - PKI Encrypted
  // ========================================================================

  /**
   * @brief Send a PKI-encrypted text message
   *
   * Sends a text message encrypted with Curve25519/AES-CCM to a specific node.
   *
   * @param text Text to send
   * @param destNode Destination node number
   * @param remotePubKey Destination node's 32-byte public key
   * @return Packet ID if sent successfully, 0 on error
   */
  uint32_t sendTextPKI(const char *text, NodeNum destNode,
                       const uint8_t remotePubKey[32]);

  /**
   * @brief Send a PKI-encrypted data message
   *
   * Sends arbitrary data encrypted with Curve25519/AES-CCM.
   *
   * @param port Application port number
   * @param data Data to send
   * @param len Data length
   * @param destNode Destination node number
   * @param remotePubKey Destination node's 32-byte public key
   * @param wantAck Request acknowledgment
   * @return Packet ID if sent successfully, 0 on error
   */
  uint32_t sendDataPKI(meshtastic_PortNum port, const uint8_t *data, size_t len,
                       NodeNum destNode, const uint8_t remotePubKey[32],
                       bool wantAck = false);

  // ========================================================================
  // Reception
  // ========================================================================

  /**
   * @brief Process radio events
   *
   * Should be called regularly in the main loop to handle incoming packets.
   */
  void update();

  /**
   * @brief Check if a packet is available
   *
   * @return true if a packet has been received
   */
  bool available();

  /**
   * @brief Receive and decode a packet
   *
   * Automatically detects PKI vs channel encryption and decrypts appropriately.
   * For PKI packets, the PKIKeyLookup callback must be set.
   *
   * @param packet Output: decoded packet
   * @return Result of receive operation
   */
  ReceiveResult receive(MeshPacket &packet);

  /**
   * @brief Set callback for received packets
   *
   * Alternative to polling with available()/receive().
   *
   * @param callback Function to call when a packet is received
   */
  void onReceive(PacketCallback callback);

  // ========================================================================
  // Status
  // ========================================================================

  /**
   * @brief Check if radio is transmitting
   *
   * @return true if transmission is in progress
   */
  bool isTransmitting() const;

  /**
   * @brief Get this node's number
   *
   * @return Node number
   */
  NodeNum getNodeNum() const { return config_.nodeNum; }

  /**
   * @brief Get the underlying RadioLib radio
   *
   * @return Pointer to the radio, or nullptr if not initialized
   */
  PhysicalLayer *getRadio() { return phy_; }

  // ========================================================================
  // Advanced
  // ========================================================================

  /**
   * @brief Get the channel object for direct manipulation
   *
   * @return Reference to channel
   */
  MeshChannel &getChannel() { return channel_; }

  /**
   * @brief Get the PKI engine for direct manipulation
   *
   * @return Reference to PKI engine
   */
  MeshCryptoPKI &getPKI() { return pki_; }

private:
  MeshConfig config_;
  PhysicalLayer *phy_;
  MeshChannel channel_;
  MeshCryptoPKI pki_;
  PKIKeyLookup pkiKeyLookup_;
  PacketCallback receiveCallback_;
  bool initialized_;

  // Internal receive buffer
  uint8_t rxBuffer_[MAX_LORA_PAYLOAD_LEN];
  size_t rxLen_;
  bool rxPending_;
  int16_t lastRssi_;
  float lastSnr_;

  // Apply the Meshtastic RF policy for the configured region and preset
  bool applyRfConfig();

  // Transmission
  uint32_t sendPacket(PacketHeader &header, const uint8_t *payload, size_t len,
                      bool usePKI, const uint8_t *remotePubKey);
};

} // namespace libmeshtastic_leaf
