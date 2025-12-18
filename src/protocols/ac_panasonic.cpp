#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodePanasonicAC(const esp32ir::RxResult &in, esp32ir::payload::PanasonicAC &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::PanasonicAC, "PanasonicAC");
    }
    bool Transmitter::sendPanasonicAC(const esp32ir::payload::PanasonicAC &p)
    {
        logSendStub("PanasonicAC");
        return send(makeProtocolMessage(esp32ir::Protocol::PanasonicAC, p));
    }

} // namespace esp32ir
