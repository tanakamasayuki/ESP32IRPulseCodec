#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeDaikinAC(const esp32ir::RxResult &in, esp32ir::payload::DaikinAC &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::DaikinAC, "DaikinAC", out);
    }
    bool Transmitter::sendDaikinAC(const esp32ir::payload::DaikinAC &p)
    {
        logSendStub("DaikinAC");
        // AC payload structs are placeholders; still forward as-is for future encode support.
        return send(makeProtocolMessage(esp32ir::Protocol::DaikinAC, p));
    }

} // namespace esp32ir
