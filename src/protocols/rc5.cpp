#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeRC5(const esp32ir::RxResult &in, esp32ir::payload::RC5 &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::RC5, "RC5");
    }
    bool Transmitter::sendRC5(const esp32ir::payload::RC5 &p)
    {
        logSendStub("RC5");
        return send(makeProtocolMessage(esp32ir::Protocol::RC5, p));
    }
    bool Transmitter::sendRC5(uint16_t command, bool toggle)
    {
        esp32ir::payload::RC5 p{command, toggle};
        return sendRC5(p);
    }

} // namespace esp32ir
