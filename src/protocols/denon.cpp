#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeDenon(const esp32ir::RxResult&, esp32ir::payload::Denon&) { return false; }
bool Transmitter::sendDenon(const esp32ir::payload::Denon&) { return false; }
bool Transmitter::sendDenon(uint16_t, uint16_t, bool) { return false; }

}  // namespace esp32ir
