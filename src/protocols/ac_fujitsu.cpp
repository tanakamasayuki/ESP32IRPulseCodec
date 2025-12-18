#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/protocol_message_utils.h"
#include <esp_log.h>

namespace esp32ir
{

    bool decodeFujitsuAC(const esp32ir::RxResult &in, esp32ir::payload::FujitsuAC &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::FujitsuAC, "FujitsuAC", out);
    }
    bool Transmitter::sendFujitsuAC(const esp32ir::payload::FujitsuAC &p)
    {
        ESP_LOGW("ESP32IRPulseCodec", "send FujitsuAC not implemented");
        return send(makeProtocolMessage(esp32ir::Protocol::FujitsuAC, p));
    }

} // namespace esp32ir
