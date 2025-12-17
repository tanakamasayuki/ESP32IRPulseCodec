#include "ESP32IRPulseCodec.h"

namespace esp32ir {

bool decodeDaikinAC(const esp32ir::RxResult&, esp32ir::payload::DaikinAC&) { return false; }
bool Transmitter::sendDaikinAC(const esp32ir::payload::DaikinAC&) { return false; }

}  // namespace esp32ir
