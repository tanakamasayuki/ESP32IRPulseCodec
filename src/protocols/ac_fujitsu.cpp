#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeFujitsuAC(const esp32ir::RxResult &in, esp32ir::payload::FujitsuAC &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::FujitsuAC, "FujitsuAC");
    }
    bool Transmitter::sendFujitsuAC(const esp32ir::payload::FujitsuAC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::FujitsuAC, nullptr, 0, 0};
        return send(msg);
    }

} // namespace esp32ir
