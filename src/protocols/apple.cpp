#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeApple(const esp32ir::RxResult &in, esp32ir::payload::Apple &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::Apple, "Apple");
    }
    bool Transmitter::sendApple(const esp32ir::payload::Apple &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Apple, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendApple(uint16_t address, uint8_t command)
    {
        esp32ir::payload::Apple p{address, command};
        return sendApple(p);
    }

} // namespace esp32ir
