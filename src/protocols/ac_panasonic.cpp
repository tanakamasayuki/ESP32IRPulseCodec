#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodePanasonicAC(const esp32ir::RxResult&, esp32ir::payload::PanasonicAC&) { return false; }
bool sendPanasonicAC(const esp32ir::payload::PanasonicAC&) { return false; }

}  // namespace esp32ir
