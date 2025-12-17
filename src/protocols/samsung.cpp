#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeSamsung(const esp32ir::RxResult &, esp32ir::payload::Samsung &) { return false; }
    bool Transmitter::sendSamsung(const esp32ir::payload::Samsung &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Samsung, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendSamsung(uint16_t address, uint16_t command)
    {
        esp32ir::payload::Samsung p{address, command};
        return sendSamsung(p);
    }

} // namespace esp32ir
