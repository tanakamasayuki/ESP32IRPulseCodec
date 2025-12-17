#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeToshiba(const esp32ir::RxResult &, esp32ir::payload::Toshiba &) { return false; }
    bool Transmitter::sendToshiba(const esp32ir::payload::Toshiba &) { return false; }
    bool Transmitter::sendToshiba(uint16_t, uint16_t, uint8_t) { return false; }

} // namespace esp32ir
