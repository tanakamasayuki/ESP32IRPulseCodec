#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeNEC(const esp32ir::RxResult &, esp32ir::payload::NEC &) { return false; }

    bool Transmitter::sendNEC(const esp32ir::payload::NEC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::NEC, nullptr, 0, 0};
        return send(msg);
    }

    bool Transmitter::sendNEC(uint16_t address, uint8_t command, bool repeat)
    {
        esp32ir::payload::NEC p{address, command, repeat};
        return sendNEC(p);
    }

} // namespace esp32ir
