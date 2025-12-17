#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeAEHA(const esp32ir::RxResult &, esp32ir::payload::AEHA &) { return false; }
    bool Transmitter::sendAEHA(const esp32ir::payload::AEHA &) { return false; }
    bool Transmitter::sendAEHA(uint16_t, uint32_t, uint8_t) { return false; }

} // namespace esp32ir
