#pragma once

#include "ESP32IRPulseCodec.h"
#include <esp_log.h>

namespace esp32ir
{
    template <typename T>
    inline esp32ir::ProtocolMessage makeProtocolMessage(esp32ir::Protocol protocol, const T &payload)
    {
        return esp32ir::ProtocolMessage{
            protocol,
            reinterpret_cast<const uint8_t *>(&payload),
            static_cast<uint16_t>(sizeof(T)),
            0};
    }

    inline void logSendStub(const char *name)
    {
        ESP_LOGW("ESP32IRPulseCodec", "send %s not implemented yet", name);
    }
} // namespace esp32ir
