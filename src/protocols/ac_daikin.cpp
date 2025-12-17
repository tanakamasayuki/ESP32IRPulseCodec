#include "ESP32IRPulseCodec.h"

namespace esp32ir
{

    bool decodeDaikinAC(const esp32ir::RxResult &, esp32ir::payload::DaikinAC &) { return false; }
    bool Transmitter::sendDaikinAC(const esp32ir::payload::DaikinAC &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::DaikinAC, nullptr, 0, 0};
        return send(msg);
    }

} // namespace esp32ir
