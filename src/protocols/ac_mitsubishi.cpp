#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodeMitsubishiAC(const esp32ir::RxResult &in, esp32ir::payload::MitsubishiAC &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::MitsubishiAC, "MitsubishiAC");
    }
    bool Transmitter::sendMitsubishiAC(const esp32ir::payload::MitsubishiAC &p)
    {
        logSendStub("MitsubishiAC");
        return send(makeProtocolMessage(esp32ir::Protocol::MitsubishiAC, p));
    }

} // namespace esp32ir
