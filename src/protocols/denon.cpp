#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

    bool decodeDenon(const esp32ir::RxResult &in, esp32ir::payload::Denon &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::Denon, out))
        {
            return true;
        }
        uint64_t data = 0;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kRepeatSpaceUs = 2250;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;

        // Strict repeat detection: 9000/2250/560 pattern only.
        std::vector<esp32ir::Pulse> pulses;
        if (esp32ir::collectPulses(in.raw, pulses))
        {
            auto inTol = [&](const esp32ir::Pulse &p, bool mark, uint32_t target, uint32_t tol)
            {
                return p.mark == mark && esp32ir::inRange(p.us, target, tol);
            };
            if (pulses.size() >= 3 && inTol(pulses[0], true, kHdrMarkUs, 25) && inTol(pulses[1], false, kRepeatSpaceUs, 25) && inTol(pulses[2], true, kBitMarkUs, 30))
            {
                out.address = 0;
                out.command = 0;
                out.repeat = true;
                return true;
            }
        }

        if (nec_like::decodeRaw(in, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs, kZeroSpaceUs, kOneSpaceUs, 32, data))
        {
            out.address = static_cast<uint16_t>(data & 0xFFFF);
            out.command = static_cast<uint16_t>(data >> 16);
            out.repeat = false;
            return true;
        }
        return false;
    }
    bool Transmitter::sendDenon(const esp32ir::payload::Denon &p)
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;

        uint64_t data = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.command) << 16);
        constexpr uint8_t kBits = 32;
        esp32ir::ITPSBuffer buf = nec_like::build(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                  kZeroSpaceUs, kOneSpaceUs, data, kBits, true);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::Denon));
    }
    bool Transmitter::sendDenon(uint16_t address, uint16_t command, bool repeat)
    {
        esp32ir::payload::Denon p{address, command, repeat};
        return sendDenon(p);
    }

} // namespace esp32ir
