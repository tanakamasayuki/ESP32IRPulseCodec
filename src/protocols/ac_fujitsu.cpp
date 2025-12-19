#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/protocol_message_utils.h"
#include <esp_log.h>

namespace esp32ir
{

    bool decodeFujitsuAC(const esp32ir::RxResult &in, esp32ir::payload::FujitsuAC &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::FujitsuAC, out);
    }
    bool Transmitter::sendFujitsuAC(const esp32ir::payload::FujitsuAC &p)
    {
        ESP_LOGW("ESP32IRPulseCodec", "send FujitsuAC not implemented");
        (void)p;
        return false;
    }

} // namespace esp32ir
