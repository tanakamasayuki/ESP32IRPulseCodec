#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeDenon(const esp32ir::RxResult &in, esp32ir::payload::Denon &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::Denon, "Denon");
    }
    bool Transmitter::sendDenon(const esp32ir::payload::Denon &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Denon, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendDenon(uint16_t address, uint16_t command, bool repeat)
    {
        esp32ir::payload::Denon p{address, command, repeat};
        return sendDenon(p);
    }

} // namespace esp32ir
