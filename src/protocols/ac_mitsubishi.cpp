#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeMitsubishiAC(const esp32ir::RxResult &, esp32ir::payload::MitsubishiAC &) { return false; }
    bool Transmitter::sendMitsubishiAC(const esp32ir::payload::MitsubishiAC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::MitsubishiAC, nullptr, 0, 0};
        return send(msg);
    }

} // namespace esp32ir
