/**
 * @file MeshRadioSX127x.h
 * @brief SX127x family radio driver
 *
 * Driver for SX1276, SX1278, and compatible radio chips.
 */

#pragma once

#include "MeshRadio.h"
#include "../MeshRegion.h"
#include <SX127x.h>

namespace libmeshtastic_leaf {

/**
 * @brief SX127x family radio driver
 *
 * Supports SX1276, SX1278, and compatible chips (RFM95, etc.)
 */
class MeshRadioSX127x : public MeshRadio {
public:
    /**
     * @brief Construct a new SX127x radio driver
     *
     * @param module Pointer to RadioLib SX1276 or SX1278 module instance
     */
    MeshRadioSX127x(SX1276* module) : module_(module) {}

    virtual ~MeshRadioSX127x() = default;

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
            0  // Gain (0 = automatic)
        );

        if (state != RADIOLIB_ERR_NONE) {
            return false;
        }

        // Set CRC on
        state = module_->setCRC(true);
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
        int state = module_->startReceive();
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
        return (module_->getIRQFlags() & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_RX_DONE) != 0;
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
        int state = module_->sleep();
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
        module_->setDio0Action(handler, RISING);
    }

    /**
     * @brief Get the underlying RadioLib module
     *
     * @return Pointer to the module
     */
    SX1276* getModule() { return module_; }

private:
    SX1276* module_;
};

} // namespace libmeshtastic_leaf
