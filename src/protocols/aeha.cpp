#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeAEHA(const esp32ir::RxResult &in, esp32ir::payload::AEHA &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::AEHA, "AEHA", out);
    }
    bool Transmitter::sendAEHA(const esp32ir::payload::AEHA &p)
    {
        logSendStub("AEHA");
        return send(makeProtocolMessage(esp32ir::Protocol::AEHA, p));
    }
    bool Transmitter::sendAEHA(uint16_t address, uint32_t data, uint8_t nbits)
    {
        esp32ir::payload::AEHA p{address, data, nbits};
        return sendAEHA(p);
    }

} // namespace esp32ir
