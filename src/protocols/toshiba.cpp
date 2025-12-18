#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeToshiba(const esp32ir::RxResult &in, esp32ir::payload::Toshiba &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::Toshiba, "Toshiba");
    }
    bool Transmitter::sendToshiba(const esp32ir::payload::Toshiba &p)
    {
        logSendStub("Toshiba");
        return send(makeProtocolMessage(esp32ir::Protocol::Toshiba, p));
    }
    bool Transmitter::sendToshiba(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Toshiba p{address, command, extra};
        return sendToshiba(p);
    }

} // namespace esp32ir
