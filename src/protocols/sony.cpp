#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "itps_encode.h"
#include "pulse_utils.h"
#include <esp_log.h>
#include <vector>

namespace esp32ir
{

    namespace
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kStartMarkUs = 2400;
        constexpr uint32_t kStartSpaceUs = 600;
        constexpr uint32_t kBitSpaceUs = 600;
        constexpr uint32_t kBitMark0Us = 600;
        constexpr uint32_t kBitMark1Us = 1200;

        void appendMark(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, true, us, kTUs);
        }
        void appendSpace(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, false, us, kTUs);
        }

        esp32ir::ITPSBuffer buildSONY(uint16_t address, uint16_t command, uint8_t bits)
        {
            std::vector<int8_t> seq;
            seq.reserve(128);

            appendMark(seq, kStartMarkUs);
            appendSpace(seq, kStartSpaceUs);

            uint32_t payload = static_cast<uint32_t>(command) | (static_cast<uint32_t>(address) << 7);
            for (uint8_t i = 0; i < bits; ++i)
            {
                bool one = (payload >> i) & 0x1;
                appendMark(seq, one ? kBitMark1Us : kBitMark0Us);
                appendSpace(seq, kBitSpaceUs);
            }

            esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
            esp32ir::ITPSBuffer buf;
            buf.addFrame(frame);
            return buf;
        }
    } // namespace

    bool decodeSONY(const esp32ir::RxResult &in, esp32ir::payload::SONY &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::SONY, "SONY", out))
        {
            return true;
        }
        // try 12/15/20 variants
        uint16_t addr = 0;
        uint16_t cmd = 0;
        std::vector<esp32ir::Pulse> pulses;
        if (!esp32ir::collectPulses(in.raw, pulses))
        {
            return false;
        }
        for (uint8_t bits : {12, 15, 20})
        {
            if (pulses.size() < 2 + bits * 2)
            {
                continue;
            }
            size_t idx = 0;
            auto ok = [&](bool mark, uint32_t target, uint32_t tol) -> bool
            {
                if (idx >= pulses.size())
                    return false;
                const auto &p = pulses[idx];
                if (p.mark != mark || !esp32ir::inRange(p.us, target, tol))
                    return false;
                ++idx;
                return true;
            };
            idx = 0;
            if (!ok(true, 2400, 25) || !ok(false, 600, 35))
                continue;
            uint32_t data = 0;
            bool okBits = true;
            for (uint8_t i = 0; i < bits; ++i)
            {
                if (idx >= pulses.size() || !esp32ir::inRange(pulses[idx].us, 600, 35) || !pulses[idx].mark)
                {
                    okBits = false;
                    break;
                }
                uint32_t markLen = pulses[idx].us;
                ++idx;
                if (idx >= pulses.size() || pulses[idx].mark || !esp32ir::inRange(pulses[idx].us, 600, 35))
                {
                    okBits = false;
                    break;
                }
                bool one = markLen > 900;
                if (one)
                    data |= (1u << i);
                ++idx;
            }
            if (!okBits)
                continue;
            out.address = static_cast<uint16_t>(data >> 7);
            out.command = static_cast<uint16_t>(data & 0x7F);
            out.bits = bits;
            return true;
        }
        return false;
    }
    bool Transmitter::sendSONY(const esp32ir::payload::SONY &p)
    {
        uint8_t bits = p.bits;
        if (bits != 12 && bits != 15 && bits != 20)
        {
            ESP_LOGE("ESP32IRPulseCodec", "SONY bits must be 12/15/20 (got %u)", static_cast<unsigned>(bits));
            return false;
        }
        return send(buildSONY(p.address, p.command, bits));
    }
    bool Transmitter::sendSONY(uint16_t address, uint16_t command, uint8_t bits)
    {
        esp32ir::payload::SONY p{address, command, bits};
        return sendSONY(p);
    }

} // namespace esp32ir
