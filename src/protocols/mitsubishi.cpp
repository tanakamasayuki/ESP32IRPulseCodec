#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeMitsubishi(const esp32ir::RxResult &, esp32ir::payload::Mitsubishi &) { return false; }
    bool Transmitter::sendMitsubishi(const esp32ir::payload::Mitsubishi &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Mitsubishi, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendMitsubishi(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Mitsubishi p{address, command, extra};
        return sendMitsubishi(p);
    }

} // namespace esp32ir
