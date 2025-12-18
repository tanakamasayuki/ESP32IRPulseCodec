#pragma once

#include "ESP32IRPulseCodec.h"
#include <vector>

namespace esp32ir
{
    struct Pulse
    {
        bool mark;
        uint32_t us;
    };

    inline bool inRange(uint32_t v, uint32_t target, uint32_t tolPercent)
    {
        uint32_t lo = target - target * tolPercent / 100;
        uint32_t hi = target + target * tolPercent / 100;
        return v >= lo && v <= hi;
    }

    inline bool collectPulses(const esp32ir::ITPSBuffer &raw, std::vector<Pulse> &out)
    {
        out.clear();
        if (raw.frameCount() == 0)
        {
            return false;
        }
        const auto &f = raw.frame(0);
        if (!f.seq || f.len == 0 || f.T_us == 0)
        {
            return false;
        }
        out.reserve(f.len);
        for (uint16_t i = 0; i < f.len; ++i)
        {
            int v = f.seq[i];
            if (v == 0)
                continue;
            Pulse p{v > 0, static_cast<uint32_t>((v < 0 ? -v : v) * f.T_us)};
            out.push_back(p);
        }
        return !out.empty();
    }
} // namespace esp32ir
