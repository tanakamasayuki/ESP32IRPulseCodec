#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/protocol_message_utils.h"
#include <esp_log.h>

namespace esp32ir
{

    bool decodeMitsubishiAC(const esp32ir::RxResult &in, esp32ir::payload::MitsubishiAC &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::MitsubishiAC, "MitsubishiAC", out);
    }
    bool Transmitter::sendMitsubishiAC(const esp32ir::payload::MitsubishiAC &p)
    {
        ESP_LOGW("ESP32IRPulseCodec", "send MitsubishiAC not implemented");
        return send(makeProtocolMessage(esp32ir::Protocol::MitsubishiAC, p));
    }

} // namespace esp32ir
