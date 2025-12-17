#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeToshibaAC(const esp32ir::RxResult&, esp32ir::payload::ToshibaAC&) { return false; }
bool sendToshibaAC(const esp32ir::payload::ToshibaAC&) { return false; }

}  // namespace esp32ir
