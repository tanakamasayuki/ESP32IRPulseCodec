#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeRC5(const esp32ir::RxResult&, esp32ir::payload::RC5&) { return false; }
bool sendRC5(const esp32ir::payload::RC5&) { return false; }
bool sendRC5(uint16_t, bool) { return false; }

}  // namespace esp32ir
