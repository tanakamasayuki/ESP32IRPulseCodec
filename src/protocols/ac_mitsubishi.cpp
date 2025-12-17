#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeMitsubishiAC(const esp32ir::RxResult&, esp32ir::payload::MitsubishiAC&) { return false; }
bool sendMitsubishiAC(const esp32ir::payload::MitsubishiAC&) { return false; }

}  // namespace esp32ir
