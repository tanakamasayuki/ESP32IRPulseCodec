#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/itps_encode.h"
#include "core/pulse_utils.h"
#include <vector>

namespace esp32ir
{

    namespace
    {
        constexpr uint16_t kTUs = 5; // default quantization
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;
        constexpr uint32_t kRepeatSpaceUs = 2250;
        constexpr uint32_t kRepeatGapMarkUs = 560;
        constexpr uint32_t kGapUs = 40000;

        void appendMark(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, true, us, kTUs);
        }
        void appendSpace(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, false, us, kTUs);
        }

        void appendBit(std::vector<int8_t> &seq, bool one)
        {
            appendMark(seq, kBitMarkUs);
            appendSpace(seq, one ? kOneSpaceUs : kZeroSpaceUs);
        }

        bool decodeNecRaw(const esp32ir::RxResult &in, uint64_t &dataOut)
        {
            std::vector<esp32ir::Pulse> pulses;
            if (!esp32ir::collectPulses(in.raw, pulses))
                return false;
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
            if (!expect(true, kHdrMarkUs, 25) || !expect(false, kHdrSpaceUs, 25))
                return false;
            uint64_t data = 0;
            for (uint8_t i = 0; i < 32; ++i)
            {
                if (!expect(true, kBitMarkUs, 30))
                    return false;
                if (idx >= pulses.size() || pulses[idx].mark)
                    return false;
                bool one = esp32ir::inRange(pulses[idx].us, kOneSpaceUs, 30);
                bool zero = esp32ir::inRange(pulses[idx].us, kZeroSpaceUs, 30);
                if (!one && !zero)
                    return false;
                if (one)
                    data |= (uint64_t{1} << i);
                ++idx;
            }
            dataOut = data;
            return true;
        }

        esp32ir::ITPSBuffer buildNECFrame(uint16_t address, uint8_t command)
        {
            std::vector<int8_t> seq;
            seq.reserve(256);

            appendMark(seq, kHdrMarkUs);
            appendSpace(seq, kHdrSpaceUs);

            // NEC32: addr low, addr high, cmd, ~cmd (LSB first)
            uint32_t data = static_cast<uint32_t>(address);
            data |= static_cast<uint32_t>(command) << 16;
            data |= static_cast<uint32_t>(~command & 0xFF) << 24;

            for (uint8_t bit = 0; bit < 32; ++bit)
            {
                appendBit(seq, (data >> bit) & 0x1);
            }
            appendMark(seq, kBitMarkUs); // stop bit

            esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
            esp32ir::ITPSBuffer buf;
            buf.addFrame(frame);
            return buf;
        }

        esp32ir::ITPSBuffer buildNECRepeat()
        {
            std::vector<int8_t> seq;
            seq.reserve(64);

            appendMark(seq, kHdrMarkUs);
            appendSpace(seq, kRepeatSpaceUs);
            appendMark(seq, kRepeatGapMarkUs);

            esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
            esp32ir::ITPSBuffer buf;
            buf.addFrame(frame);
            return buf;
        }
    } // namespace

    bool decodeNEC(const esp32ir::RxResult &in, esp32ir::payload::NEC &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::NEC, out))
        {
            return true;
        }
        uint64_t data = 0;
        if (!decodeNecRaw(in, data))
        {
            return false;
        }
        out.address = static_cast<uint16_t>(data & 0xFFFF);
        out.command = static_cast<uint8_t>((data >> 16) & 0xFF);
        out.repeat = false;
        return true;
    }

    bool Transmitter::sendNEC(const esp32ir::payload::NEC &p)
    {
        // If repeat=true, send the NEC repeat code; otherwise full 32-bit frame.
        if (p.repeat)
        {
            return sendWithGap(buildNECRepeat(), kGapUs);
        }
        return sendWithGap(buildNECFrame(p.address, p.command), kGapUs);
    }

    bool Transmitter::sendNEC(uint16_t address, uint8_t command, bool repeat)
    {
        esp32ir::payload::NEC p{address, command, repeat};
        return sendNEC(p);
    }

} // namespace esp32ir
