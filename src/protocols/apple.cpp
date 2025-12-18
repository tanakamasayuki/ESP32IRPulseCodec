#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeApple(const esp32ir::RxResult &in, esp32ir::payload::Apple &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::Apple, "Apple");
    }
    bool Transmitter::sendApple(const esp32ir::payload::Apple &p)
    {
        logSendStub("Apple");
        return send(makeProtocolMessage(esp32ir::Protocol::Apple, p));
    }
    bool Transmitter::sendApple(uint16_t address, uint8_t command)
    {
        esp32ir::payload::Apple p{address, command};
        return sendApple(p);
    }

} // namespace esp32ir
