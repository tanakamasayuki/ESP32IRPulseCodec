#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodePioneer(const esp32ir::RxResult&, esp32ir::payload::Pioneer&) { return false; }
bool sendPioneer(const esp32ir::payload::Pioneer&) { return false; }
bool sendPioneer(uint16_t, uint16_t, uint8_t) { return false; }

}  // namespace esp32ir
