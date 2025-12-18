#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeMitsubishi(const esp32ir::RxResult &in, esp32ir::payload::Mitsubishi &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::Mitsubishi, "Mitsubishi", out);
    }
    bool Transmitter::sendMitsubishi(const esp32ir::payload::Mitsubishi &p)
    {
        logSendStub("Mitsubishi");
        return send(makeProtocolMessage(esp32ir::Protocol::Mitsubishi, p));
    }
    bool Transmitter::sendMitsubishi(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Mitsubishi p{address, command, extra};
        return sendMitsubishi(p);
    }

} // namespace esp32ir
