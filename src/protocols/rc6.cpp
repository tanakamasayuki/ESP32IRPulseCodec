#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeRC6(const esp32ir::RxResult &, esp32ir::payload::RC6 &) { return false; }
    bool Transmitter::sendRC6(const esp32ir::payload::RC6 &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::RC6, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendRC6(uint32_t command, uint8_t mode, bool toggle)
    {
        esp32ir::payload::RC6 p{command, mode, toggle};
        return sendRC6(p);
    }

} // namespace esp32ir
