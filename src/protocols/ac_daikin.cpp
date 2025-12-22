#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/protocol_message_utils.h"

namespace esp32ir
{

    bool decodeDaikinAC(const esp32ir::RxResult &in, const esp32ir::ac::Capabilities &, esp32ir::ac::DeviceState &)
    {
        ESP_LOGW("ESP32IRPulseCodec", "decodeDaikinAC (common AC API) not implemented");
        return false;
    }

    bool Transmitter::sendDaikinAC(const esp32ir::ac::DeviceState &, const esp32ir::ac::Capabilities &)
    {
        ESP_LOGW("ESP32IRPulseCodec", "send DaikinAC (common AC API) not implemented");
        return false;
    }

} // namespace esp32ir
