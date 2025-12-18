#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeFujitsuAC(const esp32ir::RxResult &in, esp32ir::payload::FujitsuAC &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::FujitsuAC, "FujitsuAC", out);
    }
    bool Transmitter::sendFujitsuAC(const esp32ir::payload::FujitsuAC &p)
    {
        logSendStub("FujitsuAC");
        return send(makeProtocolMessage(esp32ir::Protocol::FujitsuAC, p));
    }

} // namespace esp32ir
