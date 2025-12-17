#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeJVC(const esp32ir::RxResult&, esp32ir::payload::JVC&) { return false; }
bool sendJVC(const esp32ir::payload::JVC&) { return false; }
bool sendJVC(uint16_t, uint16_t) { return false; }

}  // namespace esp32ir
