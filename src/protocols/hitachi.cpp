#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeHitachi(const esp32ir::RxResult &, esp32ir::payload::Hitachi &) { return false; }
    bool Transmitter::sendHitachi(const esp32ir::payload::Hitachi &) { return false; }
    bool Transmitter::sendHitachi(uint16_t, uint16_t, uint8_t) { return false; }

} // namespace esp32ir
