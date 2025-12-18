#pragma once

#include "ESP32IRPulseCodec.h"
#include <esp_log.h>
#include <cstring>
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

    template <typename T>
    inline bool decodeMessage(const esp32ir::RxResult &in, esp32ir::Protocol expected, const char *name, T &out)
    {
        if (in.protocol == expected && in.status == esp32ir::RxStatus::DECODED && in.message.data && in.message.length == sizeof(T))
        {
            std::memcpy(&out, in.message.data, sizeof(T));
            return true;
        }
        if (in.status == esp32ir::RxStatus::DECODED || in.status == esp32ir::RxStatus::RAW_ONLY || in.status == esp32ir::RxStatus::OVERFLOW)
        {
            return false;
        }
        ESP_LOGW("ESP32IRPulseCodec", "decode %s failed: unsupported status", name);
        return false;
    }

    inline bool decodeNecLikeRaw(const esp32ir::RxResult &in,
                                 uint32_t hdrMarkUs,
                                 uint32_t hdrSpaceUs,
                                 uint32_t bitMarkUs,
                                 uint32_t zeroSpaceUs,
                                 uint32_t oneSpaceUs,
                                 uint8_t bits,
                                 uint64_t &outData)
    {
        std::vector<Pulse> pulses;
        if (!collectPulses(in.raw, pulses))
        {
            return false;
        }
        size_t idx = 0;
        auto expect = [&](bool mark, uint32_t targetUs) -> bool
        {
            if (idx >= pulses.size())
                return false;
            const auto &p = pulses[idx];
            if (p.mark != mark || !inRange(p.us, targetUs, 25))
                return false;
            ++idx;
            return true;
        };
        if (hdrMarkUs && hdrSpaceUs)
        {
            if (!expect(true, hdrMarkUs) || !expect(false, hdrSpaceUs))
            {
                return false;
            }
        }
        uint64_t data = 0;
        for (uint8_t i = 0; i < bits; ++i)
        {
            if (!expect(true, bitMarkUs))
                return false;
            if (idx >= pulses.size())
                return false;
            const auto &p = pulses[idx];
            if (p.mark)
                return false;
            bool one = inRange(p.us, oneSpaceUs, 30);
            bool zero = inRange(p.us, zeroSpaceUs, 30);
            if (!one && !zero)
                return false;
            if (one)
                data |= (uint64_t{1} << i);
            ++idx;
        }
        outData = data;
        return true;
    }

    inline bool decodeSonyRaw(const esp32ir::RxResult &in, uint8_t bits, uint16_t &address, uint16_t &command)
    {
        std::vector<Pulse> pulses;
        if (!collectPulses(in.raw, pulses))
        {
            return false;
        }
        if (pulses.size() < 2 + bits * 2)
        {
            return false;
        }
        size_t idx = 0;
        if (!inRange(pulses[idx].us, 2400, 25) || !pulses[idx].mark)
            return false;
        ++idx;
        if (!inRange(pulses[idx].us, 600, 35) || pulses[idx].mark)
            return false;
        ++idx;
        uint32_t data = 0;
        for (uint8_t i = 0; i < bits; ++i)
        {
            if (!inRange(pulses[idx].us, 600, 35) || !pulses[idx].mark)
                return false;
            ++idx;
            if (idx >= pulses.size() || !inRange(pulses[idx].us, 600, 35) || pulses[idx].mark)
                return false;
            // Mark length encodes bit
            bool one = pulses[idx - 1].us > 900; // 1200 vs 600
            if (one)
                data |= (1u << i);
            ++idx;
        }
        command = data & 0x7F;
        address = static_cast<uint16_t>(data >> 7);
        return true;
    }

    inline bool decodeAEHARaw(const esp32ir::RxResult &in, uint16_t &address, uint32_t &data, uint8_t &nbits)
    {
        std::vector<Pulse> pulses;
        if (!collectPulses(in.raw, pulses))
        {
            return false;
        }
        size_t idx = 0;
        auto expect = [&](bool mark, uint32_t target) -> bool
        {
            if (idx >= pulses.size())
                return false;
            const auto &p = pulses[idx];
            if (p.mark != mark || !inRange(p.us, target, 30))
                return false;
            ++idx;
            return true;
        };
        constexpr uint32_t unit = 425;
        if (!expect(true, unit * 8) || !expect(false, unit * 4))
            return false;
        uint64_t raw = 0;
        uint8_t bits = 0;
        while (idx + 1 < pulses.size() && bits < 48)
        {
            if (!expect(true, unit))
                return false;
            if (idx >= pulses.size() || pulses[idx].mark)
                return false;
            bool one = inRange(pulses[idx].us, unit * 3, 35);
            bool zero = inRange(pulses[idx].us, unit, 35);
            if (!one && !zero)
                break;
            if (one)
                raw |= (uint64_t{1} << bits);
            ++bits;
            ++idx;
        }
        if (bits < 24)
            return false;
        address = static_cast<uint16_t>(raw & 0xFFFF);
        data = static_cast<uint32_t>(raw >> 16);
        nbits = static_cast<uint8_t>(bits - 16);
        return true;
    }

    inline bool decodeRC5Raw(const esp32ir::RxResult &in, uint16_t &command, bool &toggle)
    {
        std::vector<Pulse> pulses;
        if (!collectPulses(in.raw, pulses))
            return false;
        if (pulses.size() < 28) // 14 bits * 2 halves
            return false;
        uint32_t T = pulses[0].us;
        size_t idx = 0;
        auto nextHalf = [&]() -> Pulse
        {
            if (idx >= pulses.size())
                return Pulse{false, 0};
            return pulses[idx++];
        };
        auto okHalf = [&](const Pulse &p)
        { return inRange(p.us, T, 40); };
        // Start bits: 1,1 -> mark/space, mark/space
        Pulse a = nextHalf(), b = nextHalf();
        if (!a.mark || b.mark || !okHalf(a) || !okHalf(b))
            return false;
        Pulse c = nextHalf(), d = nextHalf();
        if (!c.mark || d.mark || !okHalf(c) || !okHalf(d))
            return false;
        Pulse t1 = nextHalf(), t2 = nextHalf();
        if (!okHalf(t1) || !okHalf(t2))
            return false;
        toggle = t1.mark && !t2.mark;
        uint16_t addr = 0;
        for (int i = 4; i >= 0; --i)
        {
            Pulse h1 = nextHalf(), h2 = nextHalf();
            if (!okHalf(h1) || !okHalf(h2))
                return false;
            bool bit = h1.mark && !h2.mark;
            if (bit)
                addr |= (1 << i);
        }
        uint16_t cmd = 0;
        for (int i = 5; i >= 0; --i)
        {
            Pulse h1 = nextHalf(), h2 = nextHalf();
            if (!okHalf(h1) || !okHalf(h2))
                return false;
            bool bit = h1.mark && !h2.mark;
            if (bit)
                cmd |= (1 << i);
        }
        command = cmd;
        return true;
    }

    inline bool decodeRC6Raw(const esp32ir::RxResult &in, uint32_t &command, uint8_t &mode, bool &toggle)
    {
        std::vector<Pulse> pulses;
        if (!collectPulses(in.raw, pulses))
            return false;
        if (pulses.size() < 40)
            return false;
        size_t idx = 0;
        auto take = [&]() -> Pulse
        {
            if (idx >= pulses.size())
                return Pulse{false, 0};
            return pulses[idx++];
        };
        auto ok = [&](const Pulse &p, uint32_t target, uint32_t tol)
        { return inRange(p.us, target, tol); };
        // Leader 2T mark 2T space
        Pulse p1 = take(), p2 = take();
        if (!p1.mark || p2.mark || !ok(p1, p1.us, 40) || !ok(p2, p1.us, 40))
            return false;
        uint32_t T = p1.us / 2;
        // start bit double width
        Pulse s1 = take(), s2 = take();
        if (!s1.mark || s2.mark || !ok(s1, 2 * T, 40) || !ok(s2, 2 * T, 40))
            return false;
        auto decodeBit = [&](bool &bit) -> bool
        {
            Pulse h1 = take(), h2 = take();
            if (!ok(h1, T, 40) || !ok(h2, T, 40))
                return false;
            bit = h1.mark && !h2.mark;
            return true;
        };
        mode = 0;
        for (int i = 2; i >= 0; --i)
        {
            bool b;
            if (!decodeBit(b))
                return false;
            if (b)
                mode |= (1 << i);
        }
        if (!decodeBit(toggle))
            return false;
        command = 0;
        for (int i = 15; i >= 0; --i)
        {
            bool b;
            if (!decodeBit(b))
                return false;
            if (b)
                command |= (1u << i);
        }
        return true;
    }

} // namespace esp32ir
