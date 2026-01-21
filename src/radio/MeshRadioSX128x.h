/**
 * @file MeshRadioSX128x.h
 * @brief SX128x family radio driver (2.4GHz)
 *
 * Driver for SX1280 and SX1281 2.4GHz radio chips.
 */

#pragma once

#include "../MeshRegion.h"
#include "MeshRadio.h"
#include <SX128x.h>

namespace libmeshtastic_leaf {

/**
 * @brief SX128x family radio driver (2.4GHz)
 *
 * Supports SX1280 and SX1281 chips for 2.4GHz operation.
 */
class MeshRadioSX128x : public MeshRadio {
public:
  /**
   * @brief Construct a new SX128x radio driver
   *
   * @param module Pointer to RadioLib SX1280 module instance
   */
  MeshRadioSX128x(SX1280 *module) : module_(module) {}

  virtual ~MeshRadioSX128x() = default;

  bool begin(const RadioConfig &config) override {
    config_ = config;

    // Get 2.4GHz modem parameters (wideLora=true)
    ModemParams params = MeshRegion::getModemParams(config.preset, true);

    // Initialize the radio (2.4GHz band)
    int state = module_->begin(
        config.frequency > 0 ? config.frequency : 2450.0f, // Default to 2.4GHz
        params.bw, params.sf, params.cr, MESHTASTIC_SYNC_WORD, config.txPower,
        DEFAULT_PREAMBLE_LENGTH);

    if (state != RADIOLIB_ERR_NONE) {
      return false;
    }

    // Set CRC on
    state = module_->setCRC(2); // 2-byte CRC for SX1280
    if (state != RADIOLIB_ERR_NONE) {
      return false;
    }

    state_ = RadioState::IDLE;
    return true;
  }

  bool setModemPreset(ModemPreset preset) override {
    // Get 2.4GHz modem parameters (wideLora=true)
    ModemParams params = MeshRegion::getModemParams(preset, true);

    int state = module_->setSpreadingFactor(params.sf);
    if (state != RADIOLIB_ERR_NONE)
      return false;

    state = module_->setBandwidth(params.bw);
    if (state != RADIOLIB_ERR_NONE)
      return false;

    state = module_->setCodingRate(params.cr);
    if (state != RADIOLIB_ERR_NONE)
      return false;

    return true;
  }

  bool setFrequency(float freqMHz) override {
    int state = module_->setFrequency(freqMHz);
    if (state == RADIOLIB_ERR_NONE) {
      config_.frequency = freqMHz;
      return true;
    }
    return false;
  }

  bool setTxPower(int8_t powerDbm) override {
    // SX1280 max power is +13dBm
    if (powerDbm > 13)
      powerDbm = 13;
    int state = module_->setOutputPower(powerDbm);
    if (state == RADIOLIB_ERR_NONE) {
      config_.txPower = powerDbm;
      return true;
    }
    return false;
  }

  bool setSyncWord(uint8_t syncWord) override {
    // SX1280 uses a 2-byte sync word, expand single byte
    uint8_t syncArr[2] = {syncWord, syncWord};
    int state = module_->setSyncWord(syncArr, 2);
    return (state == RADIOLIB_ERR_NONE);
  }

  bool startReceive() override {
    int state = module_->startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      state_ = RadioState::RX;
      return true;
    }
    return false;
  }

  bool transmit(const uint8_t *data, size_t len) override {
    state_ = RadioState::TX;

    int state = module_->transmit(const_cast<uint8_t *>(data), len);

    state_ = RadioState::IDLE;
    return (state == RADIOLIB_ERR_NONE);
  }

  bool available() override {
    return (module_->getIrqFlags() & RADIOLIB_SX128X_IRQ_RX_DONE) != 0;
  }

  bool readPacket(uint8_t *buffer, size_t maxLen, size_t &actualLen) override {
    actualLen = module_->getPacketLength();
    if (actualLen > maxLen) {
      actualLen = maxLen;
    }

    int state = module_->readData(buffer, actualLen);
    if (state == RADIOLIB_ERR_NONE) {
      lastRSSI_ = lround(module_->getRSSI());
      lastSNR_ = module_->getSNR();
      return true;
    }
    return false;
  }

  bool isChannelActive() override {
    state_ = RadioState::CAD;

    // SX1280 CAD with 4 symbols (recommended for 2.4GHz)
    int state = module_->scanChannel();

    state_ = RadioState::IDLE;
    return (state == RADIOLIB_LORA_DETECTED);
  }

  bool isTransmitting() const override { return (state_ == RadioState::TX); }

  int16_t getRSSI() const override { return lastRSSI_; }

  float getSNR() const override { return lastSNR_; }

  bool sleep() override {
    int state = module_->sleep(true);
    if (state == RADIOLIB_ERR_NONE) {
      state_ = RadioState::SLEEP;
      return true;
    }
    return false;
  }

  bool wake() override {
    int state = module_->standby();
    if (state == RADIOLIB_ERR_NONE) {
      state_ = RadioState::IDLE;
      return true;
    }
    return false;
  }

  void setReceiveHandler(void (*handler)()) override {
    module_->setDio1Action(handler);
  }

  /**
   * @brief Get the underlying RadioLib module
   *
   * @return Pointer to the module
   */
  SX1280 *getModule() { return module_; }

private:
  SX1280 *module_;
};

} // namespace libmeshtastic_leaf
