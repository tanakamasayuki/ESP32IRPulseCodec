#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeSamsung(const esp32ir::RxResult &, esp32ir::payload::Samsung &) { return false; }
    bool Transmitter::sendSamsung(const esp32ir::payload::Samsung &) { return false; }
    bool Transmitter::sendSamsung(uint16_t, uint16_t) { return false; }

} // namespace esp32ir
