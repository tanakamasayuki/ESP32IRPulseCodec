#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeToshiba(const esp32ir::RxResult &, esp32ir::payload::Toshiba &) { return false; }
    bool Transmitter::sendToshiba(const esp32ir::payload::Toshiba &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Toshiba, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendToshiba(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Toshiba p{address, command, extra};
        return sendToshiba(p);
    }

} // namespace esp32ir
