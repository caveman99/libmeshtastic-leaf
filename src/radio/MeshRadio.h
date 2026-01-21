/**
 * @file MeshRadio.h
 * @brief Abstract radio interface for Meshtastic Leaf
 *
 * This defines the abstract interface that all radio drivers must implement.
 * It provides a common API for sending and receiving packets regardless of
 * the underlying radio hardware.
 */

#pragma once

#include "../MeshTypes.h"

// RadioLib base module
#include <RadioLib.h>

namespace libmeshtastic_leaf {

/**
 * @brief Radio state machine states
 */
enum class RadioState {
  IDLE, ///< Radio is idle
  RX,   ///< Receiving mode
  TX,   ///< Transmitting
  CAD,  ///< Channel activity detection
  SLEEP ///< Low power sleep
};

/**
 * @brief Abstract radio interface
 *
 * Base class for all radio drivers. Provides common functionality and
 * defines the interface that specific radio implementations must provide.
 */
class MeshRadio {
public:
  virtual ~MeshRadio() = default;

  /**
   * @brief Initialize the radio hardware
   *
   * Must be called before any other radio operations.
   *
   * @param config Radio configuration
   * @return true if initialization succeeded
   */
  virtual bool begin(const RadioConfig &config) = 0;

  /**
   * @brief Apply modem preset settings
   *
   * Configures spreading factor, bandwidth, and coding rate
   * based on the preset.
   *
   * @param preset Modem preset to use
   * @return true if configuration succeeded
   */
  virtual bool setModemPreset(ModemPreset preset) = 0;

  /**
   * @brief Set the operating frequency
   *
   * @param freqMHz Frequency in MHz
   * @return true if frequency was set successfully
   */
  virtual bool setFrequency(float freqMHz) = 0;

  /**
   * @brief Set the transmit power
   *
   * @param powerDbm Power in dBm
   * @return true if power was set successfully
   */
  virtual bool setTxPower(int8_t powerDbm) = 0;

  /**
   * @brief Set the sync word
   *
   * @param syncWord Sync word value
   * @return true if sync word was set successfully
   */
  virtual bool setSyncWord(uint8_t syncWord) = 0;

  /**
   * @brief Start receiving mode
   *
   * Puts the radio into continuous receive mode.
   *
   * @return true if receive mode started successfully
   */
  virtual bool startReceive() = 0;

  /**
   * @brief Transmit a packet
   *
   * Sends the provided data. This function may block until
   * transmission is complete or return immediately depending
   * on the implementation.
   *
   * @param data Packet data to send
   * @param len Data length
   * @return true if transmission started/completed successfully
   */
  virtual bool transmit(const uint8_t *data, size_t len) = 0;

  /**
   * @brief Check if a packet is available
   *
   * @return true if a packet has been received and is ready to read
   */
  virtual bool available() = 0;

  /**
   * @brief Read a received packet
   *
   * Reads the last received packet into the provided buffer.
   *
   * @param buffer Output buffer for packet data
   * @param maxLen Maximum buffer size
   * @param actualLen Output: actual packet length
   * @return true if packet was read successfully
   */
  virtual bool readPacket(uint8_t *buffer, size_t maxLen,
                          size_t &actualLen) = 0;

  /**
   * @brief Perform channel activity detection
   *
   * Checks if there is activity on the channel.
   *
   * @return true if channel activity was detected
   */
  virtual bool isChannelActive() = 0;

  /**
   * @brief Check if the radio is currently transmitting
   *
   * @return true if transmission is in progress
   */
  virtual bool isTransmitting() const = 0;

  /**
   * @brief Get the RSSI of the last received packet
   *
   * @return RSSI in dBm
   */
  virtual int16_t getRSSI() const = 0;

  /**
   * @brief Get the SNR of the last received packet
   *
   * @return SNR in dB
   */
  virtual float getSNR() const = 0;

  /**
   * @brief Put the radio into sleep mode
   *
   * @return true if sleep mode was entered successfully
   */
  virtual bool sleep() = 0;

  /**
   * @brief Wake the radio from sleep
   *
   * @return true if wake was successful
   */
  virtual bool wake() = 0;

  /**
   * @brief Get the current radio state
   *
   * @return Current state
   */
  RadioState getState() const { return state_; }

  /**
   * @brief Set interrupt handler for packet reception
   *
   * @param handler Function to call when a packet is received
   */
  virtual void setReceiveHandler(void (*handler)()) = 0;

protected:
  RadioState state_ = RadioState::IDLE;
  RadioConfig config_;
  int16_t lastRSSI_ = 0;
  float lastSNR_ = 0.0f;
};

} // namespace libmeshtastic_leaf
