#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeApple(const esp32ir::RxResult&, esp32ir::payload::Apple&) { return false; }
bool sendApple(const esp32ir::payload::Apple&) { return false; }
bool sendApple(uint16_t, uint8_t) { return false; }

}  // namespace esp32ir
