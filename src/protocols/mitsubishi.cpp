#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeMitsubishi(const esp32ir::RxResult&, esp32ir::payload::Mitsubishi&) { return false; }
bool Transmitter::sendMitsubishi(const esp32ir::payload::Mitsubishi&) { return false; }
bool Transmitter::sendMitsubishi(uint16_t, uint16_t, uint8_t) { return false; }

}  // namespace esp32ir
