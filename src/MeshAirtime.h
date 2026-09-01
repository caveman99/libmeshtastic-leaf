#pragma once

#include "MeshTypes.h"
#include <stdint.h>
#include <string.h>

namespace libmeshtastic_leaf {

// Two rolling windows: an hour of our own transmissions for duty cycle, a
// minute of everything heard for the contention window.
class MeshAirtime {
public:
  static constexpr uint8_t MINUTES_IN_HOUR = 60;
  static constexpr uint32_t MSEC_PER_MINUTE = 60UL * 1000UL;
  static constexpr uint32_t MSEC_PER_HOUR = MINUTES_IN_HOUR * MSEC_PER_MINUTE;

  MeshAirtime() { reset(0); }

  void reset(uint32_t nowMsec) {
    memset(txMsec_, 0, sizeof(txMsec_));
    memset(busyMsec_, 0, sizeof(busyMsec_));
    cursor_ = 0;
    lastRollMsec_ = nowMsec;
    secondCursor_ = 0;
    lastSecondMsec_ = nowMsec;
  }

  // Call before reading any percentage, so stale buckets are retired even
  // when nothing is being sent or received.
  void advance(uint32_t nowMsec) {
    uint32_t elapsed = nowMsec - lastRollMsec_;
    if (elapsed >= MSEC_PER_MINUTE) {
      uint32_t steps = elapsed / MSEC_PER_MINUTE;
      if (steps > MINUTES_IN_HOUR) {
        steps = MINUTES_IN_HOUR;
      }
      for (uint32_t i = 0; i < steps; i++) {
        cursor_ = (cursor_ + 1) % MINUTES_IN_HOUR;
        txMsec_[cursor_] = 0;
      }
      lastRollMsec_ += steps * MSEC_PER_MINUTE;
    }

    uint32_t elapsedSec = nowMsec - lastSecondMsec_;
    if (elapsedSec >= 1000) {
      uint32_t steps = elapsedSec / 1000;
      if (steps > SECONDS_IN_MINUTE) {
        steps = SECONDS_IN_MINUTE;
      }
      for (uint32_t i = 0; i < steps; i++) {
        secondCursor_ = (secondCursor_ + 1) % SECONDS_IN_MINUTE;
        busyMsec_[secondCursor_] = 0;
      }
      lastSecondMsec_ += steps * 1000;
    }
  }

  void logTx(uint32_t nowMsec, uint32_t msec) {
    advance(nowMsec);
    txMsec_[cursor_] += msec;
    busyMsec_[secondCursor_] += msec;
  }

  void logRx(uint32_t nowMsec, uint32_t msec) {
    advance(nowMsec);
    busyMsec_[secondCursor_] += msec;
  }

  uint32_t txMsecLastHour() const { return sum(txMsec_, MINUTES_IN_HOUR); }

  // Our own transmissions as a share of the last hour. This is the number a
  // duty cycle limit applies to.
  float txUtilizationPercent() const {
    return (float)txMsecLastHour() * 100.0f / (float)MSEC_PER_HOUR;
  }

  // Everything heard plus everything sent, over the last minute. Drives the
  // contention window, not the duty cycle.
  float channelUtilizationPercent() const {
    uint32_t busy = sum(busyMsec_, SECONDS_IN_MINUTE);
    return (float)busy * 100.0f / (float)MSEC_PER_MINUTE;
  }

private:
  static constexpr uint8_t SECONDS_IN_MINUTE = 60;

  static uint32_t sum(const uint32_t *buckets, uint8_t count) {
    uint32_t total = 0;
    for (uint8_t i = 0; i < count; i++) {
      total += buckets[i];
    }
    return total;
  }

  uint32_t txMsec_[MINUTES_IN_HOUR];
  uint32_t busyMsec_[SECONDS_IN_MINUTE];
  uint8_t cursor_;
  uint8_t secondCursor_;
  uint32_t lastRollMsec_;
  uint32_t lastSecondMsec_;
};

} // namespace libmeshtastic_leaf
