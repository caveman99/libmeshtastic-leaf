/**
 * @file MeshRadioSX126x.h
 * @brief SX126x family radio driver
 *
 * Template-based driver for SX1262, SX1268, and LLCC68 radio chips.
 */

#pragma once

#include "MeshRadio.h"
#include "../MeshRegion.h"
#include <SX126x.h>

namespace libmeshtastic_leaf {

/**
 * @brief SX126x family radio driver
 *
 * @tparam T RadioLib module type (SX1262, SX1268, LLCC68)
 */
template <typename T>
class MeshRadioSX126x : public MeshRadio {
public:
    /**
     * @brief Construct a new SX126x radio driver
     *
     * @param module Pointer to RadioLib module instance
     */
    MeshRadioSX126x(T* module) : module_(module) {}

    virtual ~MeshRadioSX126x() = default;

    bool begin(const RadioConfig& config) override {
        config_ = config;

        // Get modem parameters for the preset
        ModemParams params = MeshRegion::getModemParams(config.preset);

        // Initialize the radio
        int state = module_->begin(
            config.frequency,
            params.bw,
            params.sf,
            params.cr,
            MESHTASTIC_SYNC_WORD,
            config.txPower,
            DEFAULT_PREAMBLE_LENGTH,
            config.tcxoVoltage
        );

        if (state != RADIOLIB_ERR_NONE) {
            return false;
        }

        // Set CRC on
        state = module_->setCRC(RADIOLIB_SX126X_LORA_CRC_ON);
        if (state != RADIOLIB_ERR_NONE) {
            return false;
        }

        // Set current limit
        state = module_->setCurrentLimit(140.0f);
        if (state != RADIOLIB_ERR_NONE) {
            return false;
        }

        state_ = RadioState::IDLE;
        return true;
    }

    bool setModemPreset(ModemPreset preset) override {
        ModemParams params = MeshRegion::getModemParams(preset);

        int state = module_->setSpreadingFactor(params.sf);
        if (state != RADIOLIB_ERR_NONE) return false;

        state = module_->setBandwidth(params.bw);
        if (state != RADIOLIB_ERR_NONE) return false;

        state = module_->setCodingRate(params.cr);
        if (state != RADIOLIB_ERR_NONE) return false;

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
        int state = module_->setOutputPower(powerDbm);
        if (state == RADIOLIB_ERR_NONE) {
            config_.txPower = powerDbm;
            return true;
        }
        return false;
    }

    bool setSyncWord(uint8_t syncWord) override {
        int state = module_->setSyncWord(syncWord);
        return (state == RADIOLIB_ERR_NONE);
    }

    bool startReceive() override {
        // Use duty cycle receive for power efficiency
        int state = module_->startReceiveDutyCycleAuto(
            DEFAULT_PREAMBLE_LENGTH, 8,
            RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_TIMEOUT |
            RADIOLIB_SX126X_IRQ_CRC_ERR | RADIOLIB_SX126X_IRQ_HEADER_ERR
        );

        if (state == RADIOLIB_ERR_NONE) {
            state_ = RadioState::RX;
            return true;
        }
        return false;
    }

    bool transmit(const uint8_t* data, size_t len) override {
        state_ = RadioState::TX;

        int state = module_->transmit(const_cast<uint8_t*>(data), len);

        state_ = RadioState::IDLE;
        return (state == RADIOLIB_ERR_NONE);
    }

    bool available() override {
        return (module_->getIrqFlags() & RADIOLIB_SX126X_IRQ_RX_DONE) != 0;
    }

    bool readPacket(uint8_t* buffer, size_t maxLen, size_t& actualLen) override {
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

        // Perform CAD with default parameters
        int state = module_->scanChannel();

        state_ = RadioState::IDLE;
        return (state == RADIOLIB_LORA_DETECTED);
    }

    bool isTransmitting() const override {
        return (state_ == RadioState::TX);
    }

    int16_t getRSSI() const override {
        return lastRSSI_;
    }

    float getSNR() const override {
        return lastSNR_;
    }

    bool sleep() override {
        int state = module_->sleep(true);  // Keep config
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
     * @brief Set DIO2 as RF switch control
     *
     * @param enable True to use DIO2 for RF switch
     * @return true if setting was applied
     */
    bool setDio2AsRfSwitch(bool enable) {
        int state = module_->setDio2AsRfSwitch(enable);
        return (state == RADIOLIB_ERR_NONE);
    }

    /**
     * @brief Set RF switch pins
     *
     * @param rxEn RX enable pin
     * @param txEn TX enable pin
     */
    void setRfSwitchPins(int rxEn, int txEn) {
        module_->setRfSwitchPins(rxEn, txEn);
    }

    /**
     * @brief Enable RX boosted gain mode
     *
     * @param enable True to enable boosted gain
     * @return true if setting was applied
     */
    bool setRxBoostedGainMode(bool enable) {
        int state = module_->setRxBoostedGainMode(enable);
        return (state == RADIOLIB_ERR_NONE);
    }

    /**
     * @brief Get the underlying RadioLib module
     *
     * @return Pointer to the module
     */
    T* getModule() { return module_; }

private:
    T* module_;
};

// Convenience type aliases
using MeshRadioSX1262 = MeshRadioSX126x<SX1262>;
using MeshRadioSX1268 = MeshRadioSX126x<SX1268>;
using MeshRadioLLCC68 = MeshRadioSX126x<LLCC68>;

} // namespace libmeshtastic_leaf
