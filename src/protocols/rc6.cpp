#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeRC6(const esp32ir::RxResult &in, esp32ir::payload::RC6 &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::RC6, "RC6");
    }
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
