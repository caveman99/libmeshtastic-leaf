#pragma once

#include "MeshTypes.h"
#include <stdint.h>
#include <string.h>

namespace libmeshtastic_leaf {

// Sender and id of packets heard recently, so a flooded packet arrives once.
// Fixed capacity, oldest slot reused: the window is entries, not age.
class MeshPacketHistory {
public:
  static constexpr uint8_t CAPACITY = 32;

  MeshPacketHistory() { clear(); }

  void clear() { memset(records_, 0, sizeof(records_)); }

  // True if this sender and id were already recorded. With record set, an
  // unseen packet is added and a seen one has its timestamp refreshed.
  bool wasSeen(NodeNum from, PacketId id, uint32_t nowMsec,
               bool record = true) {
    Record *free = nullptr;
    Record *oldest = &records_[0];

    for (uint8_t i = 0; i < CAPACITY; i++) {
      Record &r = records_[i];

      if (r.from == 0 && r.id == 0) {
        if (free == nullptr) {
          free = &r;
        }
        continue;
      }

      if (r.from == from && r.id == id) {
        if (record) {
          r.rxMsec = stamp(nowMsec);
        }
        return true;
      }

      // Unsigned subtraction, so this survives the millis() rollover.
      if ((uint32_t)(nowMsec - r.rxMsec) >
          (uint32_t)(nowMsec - oldest->rxMsec)) {
        oldest = &r;
      }
    }

    if (record) {
      Record *slot = free != nullptr ? free : oldest;
      slot->from = from;
      slot->id = id;
      slot->rxMsec = stamp(nowMsec);
    }
    return false;
  }

private:
  struct Record {
    NodeNum from;
    PacketId id;
    uint32_t rxMsec;
  };

  // Zero marks an empty slot, so a genuine zero timestamp is nudged.
  static uint32_t stamp(uint32_t nowMsec) { return nowMsec == 0 ? 1 : nowMsec; }

  Record records_[CAPACITY];
};

} // namespace libmeshtastic_leaf
