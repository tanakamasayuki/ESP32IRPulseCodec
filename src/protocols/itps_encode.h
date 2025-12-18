#pragma once

#include <stdint.h>
#include <vector>

namespace esp32ir
{
    namespace itps_encode
    {
        inline void appendPulse(std::vector<int8_t> &seq, bool mark, uint32_t durationUs, uint16_t T_us)
        {
            if (T_us == 0 || durationUs == 0)
            {
                return;
            }
            uint32_t counts = (durationUs + (T_us / 2)) / T_us; // round to nearest
            if (counts == 0)
            {
                counts = 1;
            }
            while (counts > 127)
            {
                seq.push_back(mark ? 127 : -127);
                counts -= 127;
            }
            seq.push_back(static_cast<int8_t>(mark ? counts : -static_cast<int>(counts)));
        }
    } // namespace itps_encode
} // namespace esp32ir
