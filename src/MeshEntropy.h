#pragma once

#include <RNG.h>
#include <stddef.h>
#include <stdint.h>

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace libmeshtastic_leaf {

// Everything an attacker must not predict. Never randomSeed(): on ESP32 that
// downgrades the core random(). See ARCHITECTURE.md.
inline void ensureEntropy() {
  static bool started = false;
  if (!started) {
    started = true;
    CryptRNG.begin("libmeshtastic_leaf");
  }
}

inline void fillRandom(uint8_t *out, size_t len) {
  ensureEntropy();
  CryptRNG.rand(out, len);
}

// Extra entropy from a source this header has no access to, such as radio
// noise. Additive only.
inline void stirEntropy(const uint8_t *in, size_t len) {
  ensureEntropy();
  CryptRNG.stir(in, len);
}

inline uint32_t randomUint32() {
  uint32_t value = 0;
  fillRandom(reinterpret_cast<uint8_t *>(&value), sizeof(value));
  return value;
}

} // namespace libmeshtastic_leaf
