#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeDaikinAC(const esp32ir::RxResult &in, esp32ir::payload::DaikinAC &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::DaikinAC, "DaikinAC");
    }
    bool Transmitter::sendDaikinAC(const esp32ir::payload::DaikinAC &p)
    {
        logSendStub("DaikinAC");
        return send(makeProtocolMessage(esp32ir::Protocol::DaikinAC, p));
    }

} // namespace esp32ir
