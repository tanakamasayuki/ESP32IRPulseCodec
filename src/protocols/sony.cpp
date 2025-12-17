#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeSONY(const esp32ir::RxResult&, esp32ir::payload::SONY&) { return false; }
bool sendSONY(const esp32ir::payload::SONY&) { return false; }
bool sendSONY(uint16_t, uint16_t, uint8_t) { return false; }

}  // namespace esp32ir
