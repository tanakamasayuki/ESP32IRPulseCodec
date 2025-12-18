#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodePanasonicAC(const esp32ir::RxResult &in, esp32ir::payload::PanasonicAC &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::PanasonicAC, "PanasonicAC");
    }
    bool Transmitter::sendPanasonicAC(const esp32ir::payload::PanasonicAC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::PanasonicAC, nullptr, 0, 0};
        return send(msg);
    }

} // namespace esp32ir
