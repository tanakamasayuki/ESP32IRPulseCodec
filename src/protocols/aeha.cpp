#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeAEHA(const esp32ir::RxResult &, esp32ir::payload::AEHA &) { return false; }
    bool Transmitter::sendAEHA(const esp32ir::payload::AEHA &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::AEHA, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendAEHA(uint16_t address, uint32_t data, uint8_t nbits)
    {
        esp32ir::payload::AEHA p{address, data, nbits};
        return sendAEHA(p);
    }

} // namespace esp32ir
