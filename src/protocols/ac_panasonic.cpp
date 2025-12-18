#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "protocol_message_utils.h"
#include <esp_log.h>

namespace esp32ir
{

    bool decodePanasonicAC(const esp32ir::RxResult &in, esp32ir::payload::PanasonicAC &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::PanasonicAC, "PanasonicAC", out);
    }
    bool Transmitter::sendPanasonicAC(const esp32ir::payload::PanasonicAC &p)
    {
        ESP_LOGW("ESP32IRPulseCodec", "send PanasonicAC not implemented");
        return send(makeProtocolMessage(esp32ir::Protocol::PanasonicAC, p));
    }

} // namespace esp32ir
