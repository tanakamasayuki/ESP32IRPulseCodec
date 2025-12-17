#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodePanasonic(const esp32ir::RxResult&, esp32ir::payload::Panasonic&) { return false; }
bool sendPanasonic(const esp32ir::payload::Panasonic&) { return false; }
bool sendPanasonic(uint16_t, uint32_t, uint8_t) { return false; }

}  // namespace esp32ir
