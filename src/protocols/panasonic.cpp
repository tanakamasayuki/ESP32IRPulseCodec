#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodePanasonic(const esp32ir::RxResult &, esp32ir::payload::Panasonic &) { return false; }
    bool Transmitter::sendPanasonic(const esp32ir::payload::Panasonic &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Panasonic, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendPanasonic(uint16_t address, uint32_t data, uint8_t nbits)
    {
        esp32ir::payload::Panasonic p{address, data, nbits};
        return sendPanasonic(p);
    }

} // namespace esp32ir
