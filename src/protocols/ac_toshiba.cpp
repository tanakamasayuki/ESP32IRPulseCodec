#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeToshibaAC(const esp32ir::RxResult &in, esp32ir::payload::ToshibaAC &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::ToshibaAC, "ToshibaAC", out);
    }
    bool Transmitter::sendToshibaAC(const esp32ir::payload::ToshibaAC &p)
    {
        logSendStub("ToshibaAC");
        return send(makeProtocolMessage(esp32ir::Protocol::ToshibaAC, p));
    }

} // namespace esp32ir
