#pragma once

#include "ESP32IRPulseCodec.h"
#include "itps_encode.h"
#include <vector>

namespace esp32ir
{
namespace nec_like
{
inline esp32ir::ITPSBuffer build(uint16_t T_us,
                                 uint32_t headerMarkUs,
                                 uint32_t headerSpaceUs,
                                 uint32_t bitMarkUs,
                                 uint32_t zeroSpaceUs,
                                 uint32_t oneSpaceUs,
                                 uint64_t data,
                                 uint8_t bits,
                                 bool trailingMark = true)
{
    std::vector<int8_t> seq;
    seq.reserve(static_cast<size_t>(bits) * 2 + 6);
    if (headerMarkUs)
    {
        itps_encode::appendPulse(seq, true, headerMarkUs, T_us);
    }
    if (headerSpaceUs)
    {
        itps_encode::appendPulse(seq, false, headerSpaceUs, T_us);
    }
    for (uint8_t i = 0; i < bits; ++i)
    {
        bool one = (data >> i) & 0x1;
        itps_encode::appendPulse(seq, true, bitMarkUs, T_us);
        itps_encode::appendPulse(seq, false, one ? oneSpaceUs : zeroSpaceUs, T_us);
    }
    if (trailingMark)
    {
        itps_encode::appendPulse(seq, true, bitMarkUs, T_us);
    }
    esp32ir::ITPSFrame frame{T_us, static_cast<uint16_t>(seq.size()), seq.data(), 0};
    esp32ir::ITPSBuffer buf;
    buf.addFrame(frame);
    return buf;
}
} // namespace nec_like
} // namespace esp32ir

