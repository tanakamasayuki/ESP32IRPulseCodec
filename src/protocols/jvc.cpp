#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeJVC(const esp32ir::RxResult &in, esp32ir::payload::JVC &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::JVC, "JVC");
    }
    bool Transmitter::sendJVC(const esp32ir::payload::JVC &p)
    {
        logSendStub("JVC");
        return send(makeProtocolMessage(esp32ir::Protocol::JVC, p));
    }
    bool Transmitter::sendJVC(uint16_t address, uint16_t command)
    {
        esp32ir::payload::JVC p{address, command};
        return sendJVC(p);
    }

} // namespace esp32ir
