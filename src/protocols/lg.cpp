#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeLG(const esp32ir::RxResult &, esp32ir::payload::LG &) { return false; }
    bool Transmitter::sendLG(const esp32ir::payload::LG &) { return false; }
    bool Transmitter::sendLG(uint16_t, uint16_t) { return false; }

} // namespace esp32ir
