#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeFujitsuAC(const esp32ir::RxResult &, esp32ir::payload::FujitsuAC &) { return false; }
    bool Transmitter::sendFujitsuAC(const esp32ir::payload::FujitsuAC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::FujitsuAC, nullptr, 0, 0};
        return send(msg);
    }

} // namespace esp32ir
