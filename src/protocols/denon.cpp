#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

    namespace proto_const
    {
        // RC5 (used by Denon) gap heuristics for splitting
        const RxParamPreset kRC5Params{
            25000, // frameGapUs
            40000, // hardGapUs
            3000,  // minFrameUs
            60000, // maxFrameUs
            12,    // minEdges
            0,     // frameCountMax
            esp32ir::RxSplitPolicy::DROP_GAP};
    }

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
        constexpr uint16_t kTUs = 10;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;

        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Denon, reinterpret_cast<const uint8_t *>(&p), static_cast<uint16_t>(sizeof(p)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
            return false;
        esp32ir::ITPSBuffer buf = nec_like::buildFromTxBytes(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                             kZeroSpaceUs, kOneSpaceUs, txBytes, static_cast<uint8_t>(bitCount), true);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::Denon));
    }
    bool Transmitter::sendDenon(uint16_t address, uint16_t command, bool repeat)
    {
        esp32ir::payload::Denon p{address, command, repeat};
        return sendDenon(p);
    }

} // namespace esp32ir
