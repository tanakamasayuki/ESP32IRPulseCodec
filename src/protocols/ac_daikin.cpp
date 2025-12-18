#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeDaikinAC(const esp32ir::RxResult &in, esp32ir::payload::DaikinAC &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::DaikinAC, "DaikinAC");
    }
    bool Transmitter::sendDaikinAC(const esp32ir::payload::DaikinAC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::DaikinAC, nullptr, 0, 0};
        return send(msg);
    }

} // namespace esp32ir
