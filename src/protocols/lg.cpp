#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeLG(const esp32ir::RxResult &in, esp32ir::payload::LG &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::LG, "LG");
    }
    bool Transmitter::sendLG(const esp32ir::payload::LG &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::LG, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendLG(uint16_t address, uint16_t command)
    {
        esp32ir::payload::LG p{address, command};
        return sendLG(p);
    }

} // namespace esp32ir
