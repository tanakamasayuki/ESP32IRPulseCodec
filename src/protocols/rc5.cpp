#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeRC5(const esp32ir::RxResult &in, esp32ir::payload::RC5 &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::RC5, "RC5");
    }
    bool Transmitter::sendRC5(const esp32ir::payload::RC5 &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::RC5, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendRC5(uint16_t command, bool toggle)
    {
        esp32ir::payload::RC5 p{command, toggle};
        return sendRC5(p);
    }

} // namespace esp32ir
