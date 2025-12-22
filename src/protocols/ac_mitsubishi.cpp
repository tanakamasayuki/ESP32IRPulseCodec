#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/protocol_message_utils.h"

namespace esp32ir
{

    bool decodeMitsubishiAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &, esp32ir::ac::DeviceState &)
    {
        ESP_LOGW("ESP32IRPulseCodec", "decodeMitsubishiAC (common AC API) not implemented");
        return false;
    }

    bool Transmitter::sendMitsubishiAC(const esp32ir::ac::DeviceState &, const esp32ir::ac::Capabilities &)
    {
        ESP_LOGW("ESP32IRPulseCodec", "send MitsubishiAC (common AC API) not implemented");
        return false;
    }

} // namespace esp32ir
