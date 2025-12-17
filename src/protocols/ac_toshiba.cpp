#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeToshibaAC(const esp32ir::RxResult &, esp32ir::payload::ToshibaAC &) { return false; }
    bool Transmitter::sendToshibaAC(const esp32ir::payload::ToshibaAC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::ToshibaAC, nullptr, 0, 0};
        return send(msg);
    }

} // namespace esp32ir
