#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeJVC(const esp32ir::RxResult &, esp32ir::payload::JVC &) { return false; }
    bool Transmitter::sendJVC(const esp32ir::payload::JVC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::JVC, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendJVC(uint16_t address, uint16_t command)
    {
        esp32ir::payload::JVC p{address, command};
        return sendJVC(p);
    }

} // namespace esp32ir
