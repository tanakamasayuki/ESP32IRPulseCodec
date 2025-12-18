#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeNEC(const esp32ir::RxResult &in, esp32ir::payload::NEC &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::NEC, "NEC");
    }

    bool Transmitter::sendNEC(const esp32ir::payload::NEC &p)
    {
        logSendStub("NEC");
        return send(makeProtocolMessage(esp32ir::Protocol::NEC, p));
    }

    bool Transmitter::sendNEC(uint16_t address, uint8_t command, bool repeat)
    {
        esp32ir::payload::NEC p{address, command, repeat};
        return sendNEC(p);
    }

} // namespace esp32ir
