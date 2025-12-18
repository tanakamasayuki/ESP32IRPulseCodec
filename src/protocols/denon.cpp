#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeDenon(const esp32ir::RxResult &in, esp32ir::payload::Denon &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::Denon, "Denon", out);
    }
    bool Transmitter::sendDenon(const esp32ir::payload::Denon &p)
    {
        logSendStub("Denon");
        return send(makeProtocolMessage(esp32ir::Protocol::Denon, p));
    }
    bool Transmitter::sendDenon(uint16_t address, uint16_t command, bool repeat)
    {
        esp32ir::payload::Denon p{address, command, repeat};
        return sendDenon(p);
    }

} // namespace esp32ir
