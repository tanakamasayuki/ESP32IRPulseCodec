#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodePanasonicAC(const esp32ir::RxResult &, esp32ir::payload::PanasonicAC &) { return false; }
    bool Transmitter::sendPanasonicAC(const esp32ir::payload::PanasonicAC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::PanasonicAC, nullptr, 0, 0};
        return send(msg);
    }

} // namespace esp32ir
