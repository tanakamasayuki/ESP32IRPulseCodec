#pragma once

#include "ESP32IRPulseCodec.h"
#include <esp_log.h>
#include <cstring>

namespace esp32ir
{
    template <typename T>
    inline bool decodeMessage(const esp32ir::RxResult &in, esp32ir::Protocol expected, const char *name, T &out)
    {
        if (in.status != esp32ir::RxStatus::DECODED || in.protocol != expected)
        {
            return false;
        }
        if (!in.message.data || in.message.length != sizeof(T))
        {
            ESP_LOGW("ESP32IRPulseCodec", "decode %s failed: payload missing or size mismatch (got %u expected %u)",
                     name, static_cast<unsigned>(in.message.length), static_cast<unsigned>(sizeof(T)));
            return false;
        }
        std::memcpy(&out, in.message.data, sizeof(T));
        return true;
    }
} // namespace esp32ir
