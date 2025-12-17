#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeSONY(const esp32ir::RxResult &, esp32ir::payload::SONY &) { return false; }
    bool Transmitter::sendSONY(const esp32ir::payload::SONY &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::SONY, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendSONY(uint16_t address, uint16_t command, uint8_t bits)
    {
        esp32ir::payload::SONY p{address, command, bits};
        return sendSONY(p);
    }

} // namespace esp32ir
