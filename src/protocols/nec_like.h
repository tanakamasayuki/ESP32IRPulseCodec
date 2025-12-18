#pragma once

#include "ESP32IRPulseCodec.h"
#include "itps_encode.h"
#include "pulse_utils.h"
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

inline bool decodeRaw(const esp32ir::RxResult &in,
                      uint32_t headerMarkUs,
                      uint32_t headerSpaceUs,
                      uint32_t bitMarkUs,
                      uint32_t zeroSpaceUs,
                      uint32_t oneSpaceUs,
                      uint8_t bits,
                      uint64_t &outData)
{
    std::vector<esp32ir::Pulse> pulses;
    if (!esp32ir::collectPulses(in.raw, pulses))
    {
        return false;
    }
    size_t idx = 0;
    auto expect = [&](bool mark, uint32_t targetUs, uint32_t tol) -> bool
    {
        if (idx >= pulses.size())
            return false;
        const auto &p = pulses[idx];
        if (p.mark != mark || !esp32ir::inRange(p.us, targetUs, tol))
            return false;
        ++idx;
        return true;
    };
    if (headerMarkUs && headerSpaceUs)
    {
        if (!expect(true, headerMarkUs, 25) || !expect(false, headerSpaceUs, 25))
        {
            return false;
        }
    }
    uint64_t data = 0;
    for (uint8_t i = 0; i < bits; ++i)
    {
        if (!expect(true, bitMarkUs, 30))
            return false;
        if (idx >= pulses.size() || pulses[idx].mark)
            return false;
        bool one = esp32ir::inRange(pulses[idx].us, oneSpaceUs, 30);
        bool zero = esp32ir::inRange(pulses[idx].us, zeroSpaceUs, 30);
        if (!one && !zero)
            return false;
        if (one)
            data |= (uint64_t{1} << i);
        ++idx;
    }
    outData = data;
    return true;
}
} // namespace nec_like
} // namespace esp32ir
