#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeRC6(const esp32ir::RxResult&, esp32ir::payload::RC6&) { return false; }
bool Transmitter::sendRC6(const esp32ir::payload::RC6&) { return false; }
bool Transmitter::sendRC6(uint32_t, uint8_t, bool) { return false; }

}  // namespace esp32ir
