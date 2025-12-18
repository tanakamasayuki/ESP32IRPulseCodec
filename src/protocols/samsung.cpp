#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeSamsung(const esp32ir::RxResult &in, esp32ir::payload::Samsung &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::Samsung, "Samsung", out);
    }
    bool Transmitter::sendSamsung(const esp32ir::payload::Samsung &p)
    {
        logSendStub("Samsung");
        return send(makeProtocolMessage(esp32ir::Protocol::Samsung, p));
    }
    bool Transmitter::sendSamsung(uint16_t address, uint16_t command)
    {
        esp32ir::payload::Samsung p{address, command};
        return sendSamsung(p);
    }

} // namespace esp32ir
