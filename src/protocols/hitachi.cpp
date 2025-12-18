#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeHitachi(const esp32ir::RxResult &in, esp32ir::payload::Hitachi &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::Hitachi, "Hitachi");
    }
    bool Transmitter::sendHitachi(const esp32ir::payload::Hitachi &p)
    {
        logSendStub("Hitachi");
        return send(makeProtocolMessage(esp32ir::Protocol::Hitachi, p));
    }
    bool Transmitter::sendHitachi(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Hitachi p{address, command, extra};
        return sendHitachi(p);
    }

} // namespace esp32ir
