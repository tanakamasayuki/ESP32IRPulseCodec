#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/protocol_message_utils.h"
#include <esp_log.h>

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
        // AC payload structs are placeholders; encoding not available yet.
        (void)p;
        return false;
    }

} // namespace esp32ir
