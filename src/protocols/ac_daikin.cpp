#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/protocol_message_utils.h"
#include <esp32-hal-log.h>

namespace esp32ir
{

    bool decodeDaikinAC(const esp32ir::RxResult &in, esp32ir::payload::DaikinAC &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::DaikinAC, out);
    }
    bool Transmitter::sendDaikinAC(const esp32ir::payload::DaikinAC &p)
    {
        ESP_LOGW("ESP32IRPulseCodec", "send DaikinAC not implemented");
        // AC payload structs are placeholders; forward ProtocolMessage for future encode support.
        return send(makeProtocolMessage(esp32ir::Protocol::DaikinAC, p));
    }

} // namespace esp32ir
