#pragma once

#include "ESP32IRPulseCodec.h"
#include <cstring>

namespace esp32ir
{
    template <typename T>
    inline bool decodeMessage(const esp32ir::RxResult &in, esp32ir::Protocol expected, const char * /*name*/, T &out)
    {
        if (in.protocol == expected && in.status == esp32ir::RxStatus::DECODED && in.message.data && in.message.length == sizeof(T))
        {
            std::memcpy(&out, in.message.data, sizeof(T));
            return true;
        }
        return false;
    }

} // namespace esp32ir
