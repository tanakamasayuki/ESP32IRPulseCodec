#pragma once

#include "ESP32IRPulseCodec.h"
#include <esp_log.h>

namespace esp32ir
{
    inline bool decodeStub(const esp32ir::RxResult &in, esp32ir::Protocol expected, const char *name)
    {
        if (in.status != esp32ir::RxStatus::DECODED || in.protocol != expected)
        {
            return false;
        }
        ESP_LOGW("ESP32IRPulseCodec", "decode %s not implemented yet", name);
        return false;
    }
} // namespace esp32ir
