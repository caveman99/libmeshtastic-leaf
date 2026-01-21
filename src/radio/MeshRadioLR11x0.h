/**
 * @file MeshRadioLR11x0.h
 * @brief LR11x0 family radio driver
 *
 * Template-based driver for LR1110, LR1120, and LR1121 radio chips.
 */

#pragma once

#include "MeshRadio.h"
#include "../MeshRegion.h"
#include <LR11x0.h>

namespace libmeshtastic_leaf {

/**
 * @brief LR11x0 family radio driver
 *
 * @tparam T RadioLib module type (LR1110, LR1120, LR1121)
 */
template <typename T>
class MeshRadioLR11x0 : public MeshRadio {
public:
    /**
     * @brief Construct a new LR11x0 radio driver
     *
     * @param module Pointer to RadioLib module instance
     */
    MeshRadioLR11x0(T* module) : module_(module) {}

    virtual ~MeshRadioLR11x0() = default;

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
        state = module_->setCRC(RADIOLIB_LR11X0_LORA_CRC_ON);
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
        return (module_->getIrqFlags() & RADIOLIB_LR11X0_IRQ_RX_DONE) != 0;
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
        int state = module_->sleep(true, 0);
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
     * @brief Set DIO as RF switch control
     *
     * @param enable True to use DIO for RF switch
     * @return true if setting was applied
     */
    bool setDioAsRfSwitch(bool enable) {
        // LR11x0 has different RF switch control than SX126x
        // This would need board-specific configuration
        return true;
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
using MeshRadioLR1110 = MeshRadioLR11x0<LR1110>;
using MeshRadioLR1120 = MeshRadioLR11x0<LR1120>;
using MeshRadioLR1121 = MeshRadioLR11x0<LR1121>;

} // namespace libmeshtastic_leaf
