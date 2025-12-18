#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeSONY(const esp32ir::RxResult &in, esp32ir::payload::SONY &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::SONY, "SONY");
    }
    bool Transmitter::sendSONY(const esp32ir::payload::SONY &p)
    {
        logSendStub("SONY");
        return send(makeProtocolMessage(esp32ir::Protocol::SONY, p));
    }
    bool Transmitter::sendSONY(uint16_t address, uint16_t command, uint8_t bits)
    {
        esp32ir::payload::SONY p{address, command, bits};
        return sendSONY(p);
    }

} // namespace esp32ir
