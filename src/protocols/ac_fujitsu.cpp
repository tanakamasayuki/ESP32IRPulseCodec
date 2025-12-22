#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/protocol_message_utils.h"

namespace esp32ir
{

    bool decodeFujitsuAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &, esp32ir::ac::DeviceState &)
    {
        ESP_LOGW("ESP32IRPulseCodec", "decodeFujitsuAC (common AC API) not implemented");
        return false;
    }

    bool Transmitter::sendFujitsuAC(const esp32ir::ac::DeviceState &, const esp32ir::ac::Capabilities &)
    {
        ESP_LOGW("ESP32IRPulseCodec", "send FujitsuAC (common AC API) not implemented");
        return false;
    }

} // namespace esp32ir
