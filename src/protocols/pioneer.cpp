#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodePioneer(const esp32ir::RxResult &in, esp32ir::payload::Pioneer &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::Pioneer, "Pioneer");
    }
    bool Transmitter::sendPioneer(const esp32ir::payload::Pioneer &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Pioneer, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendPioneer(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Pioneer p{address, command, extra};
        return sendPioneer(p);
    }

} // namespace esp32ir
