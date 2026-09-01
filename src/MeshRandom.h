#pragma once

#include <stdint.h>

namespace libmeshtastic_leaf {

// xorshift32 for the contention window, seeded per instance. Not for anything
// an attacker must not predict; that is MeshEntropy.h.
class MeshRandom {
public:
  void seed(uint32_t value) { state_ = value != 0 ? value : 1; }

  uint32_t next() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return state_;
  }

  // Uniform enough for a backoff draw, in [0, limit).
  uint32_t below(uint32_t limit) { return limit == 0 ? 0 : next() % limit; }

private:
  uint32_t state_ = 1;
};

} // namespace libmeshtastic_leaf
