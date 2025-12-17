#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeFujitsuAC(const esp32ir::RxResult&, esp32ir::payload::FujitsuAC&) { return false; }
bool sendFujitsuAC(const esp32ir::payload::FujitsuAC&) { return false; }

}  // namespace esp32ir
