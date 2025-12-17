#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeNEC(const esp32ir::RxResult&, esp32ir::payload::NEC&) { return false; }
bool sendNEC(const esp32ir::payload::NEC&) { return false; }
bool sendNEC(uint16_t, uint8_t, bool) { return false; }

}  // namespace esp32ir
