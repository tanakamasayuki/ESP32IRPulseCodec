#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeMitsubishiAC(const esp32ir::RxResult &in, esp32ir::payload::MitsubishiAC &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::MitsubishiAC, "MitsubishiAC");
    }
    bool Transmitter::sendMitsubishiAC(const esp32ir::payload::MitsubishiAC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::MitsubishiAC, nullptr, 0, 0};
        return send(msg);
    }

} // namespace esp32ir
