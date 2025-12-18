#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeHitachi(const esp32ir::RxResult &in, esp32ir::payload::Hitachi &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::Hitachi, "Hitachi");
    }
    bool Transmitter::sendHitachi(const esp32ir::payload::Hitachi &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Hitachi, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendHitachi(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Hitachi p{address, command, extra};
        return sendHitachi(p);
    }

} // namespace esp32ir
