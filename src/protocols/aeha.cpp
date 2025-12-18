#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/itps_encode.h"
#include "core/pulse_utils.h"
#include <vector>
#include <esp_log.h>

namespace esp32ir
{

    namespace
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kUnitUs = 425; // AEHA base unit
        constexpr uint32_t kHdrMarkUs = kUnitUs * 8;
        constexpr uint32_t kHdrSpaceUs = kUnitUs * 4;
        constexpr uint32_t kBitMarkUs = kUnitUs;
        constexpr uint32_t kZeroSpaceUs = kUnitUs;
        constexpr uint32_t kOneSpaceUs = kUnitUs * 3;

        void appendMark(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, true, us, kTUs);
        }
        void appendSpace(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, false, us, kTUs);
        }

        esp32ir::ITPSBuffer buildAEHA(uint16_t address, uint32_t data, uint8_t nbits)
        {
            std::vector<int8_t> seq;
            seq.reserve(256);

            appendMark(seq, kHdrMarkUs);
            appendSpace(seq, kHdrSpaceUs);

            uint64_t payload = static_cast<uint64_t>(address) | (static_cast<uint64_t>(data) << 16);
            uint8_t totalBits = static_cast<uint8_t>(16 + nbits);
            for (uint8_t i = 0; i < totalBits; ++i)
            {
                bool one = (payload >> i) & 0x1;
                appendMark(seq, kBitMarkUs);
                appendSpace(seq, one ? kOneSpaceUs : kZeroSpaceUs);
            }
            // trailing mark
            appendMark(seq, kBitMarkUs);

            esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
            esp32ir::ITPSBuffer buf;
            buf.addFrame(frame);
            return buf;
        }
    } // namespace

    bool decodeAEHA(const esp32ir::RxResult &in, esp32ir::payload::AEHA &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::AEHA, "AEHA", out))
        {
            return true;
        }
        std::vector<esp32ir::Pulse> pulses;
        if (!esp32ir::collectPulses(in.raw, pulses))
        {
            return false;
        }
        size_t idx = 0;
        auto expect = [&](bool mark, uint32_t target) -> bool
        {
            if (idx >= pulses.size())
                return false;
            const auto &p = pulses[idx];
            if (p.mark != mark || !esp32ir::inRange(p.us, target, 30))
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
            bool one = esp32ir::inRange(pulses[idx].us, unit * 3, 35);
            bool zero = esp32ir::inRange(pulses[idx].us, unit, 35);
            if (!one && !zero)
                break;
            if (one)
                raw |= (uint64_t{1} << bits);
            ++bits;
            ++idx;
        }
        if (bits < 24)
            return false;
        out.address = static_cast<uint16_t>(raw & 0xFFFF);
        out.data = static_cast<uint32_t>(raw >> 16);
        out.nbits = static_cast<uint8_t>(bits - 16);
        return true;
    }
    bool Transmitter::sendAEHA(const esp32ir::payload::AEHA &p)
    {
        if (p.nbits == 0 || p.nbits > 32)
        {
            ESP_LOGE("ESP32IRPulseCodec", "AEHA nbits must be 1..32 (got %u)", static_cast<unsigned>(p.nbits));
            return false;
        }
        return send(buildAEHA(p.address, p.data, p.nbits));
    }
    bool Transmitter::sendAEHA(uint16_t address, uint32_t data, uint8_t nbits)
    {
        esp32ir::payload::AEHA p{address, data, nbits};
        return sendAEHA(p);
    }

} // namespace esp32ir
