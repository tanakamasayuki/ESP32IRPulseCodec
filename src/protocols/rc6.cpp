#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/itps_encode.h"
#include "core/pulse_utils.h"
#include <vector>

namespace esp32ir
{

    namespace proto_const
    {
        // RC6 gap heuristics for splitting
        const RxParamPreset kRC6Params{
            25000, // frameGapUs
            40000, // hardGapUs
            3000,  // minFrameUs
            70000, // maxFrameUs
            12,    // minEdges
            0,     // frameCountMax
            esp32ir::RxSplitPolicy::DROP_GAP};
    }

    bool decodeRC6(const esp32ir::RxResult &in, esp32ir::payload::RC6 &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::RC6, out))
        {
            return true;
        }
        std::vector<esp32ir::Pulse> pulses;
        if (!esp32ir::collectPulses(in.raw, pulses))
            return false;
        if (pulses.size() < 40)
            return false;
        size_t idx = 0;
        auto take = [&]() -> esp32ir::Pulse
        {
            if (idx >= pulses.size())
                return esp32ir::Pulse{false, 0};
            return pulses[idx++];
        };
        auto ok = [&](const esp32ir::Pulse &p, uint32_t target, uint32_t tol)
        { return esp32ir::inRange(p.us, target, tol); };
        // Leader 2T mark 2T space
        esp32ir::Pulse p1 = take(), p2 = take();
        if (!p1.mark || p2.mark || !ok(p1, p1.us, 40) || !ok(p2, p1.us, 40))
            return false;
        uint32_t T = p1.us / 2;
        // start bit double width
        esp32ir::Pulse s1 = take(), s2 = take();
        if (!s1.mark || s2.mark || !ok(s1, 2 * T, 40) || !ok(s2, 2 * T, 40))
            return false;
        auto decodeBit = [&](bool &bit) -> bool
        {
            esp32ir::Pulse h1 = take(), h2 = take();
            if (!ok(h1, T, 40) || !ok(h2, T, 40))
                return false;
            bit = h1.mark && !h2.mark;
            return true;
        };
        out.mode = 0;
        for (int i = 2; i >= 0; --i)
        {
            bool b;
            if (!decodeBit(b))
                return false;
            if (b)
                out.mode |= (1 << i);
        }
        if (!decodeBit(out.toggle))
            return false;
        out.command = 0;
        for (int i = 15; i >= 0; --i)
        {
            bool b;
            if (!decodeBit(b))
                return false;
            if (b)
                out.command |= (1u << i);
        }
        return true;
    }
    namespace
    {
        constexpr uint16_t kTUs = 444;
        void appendHalf(std::vector<int8_t> &seq, bool mark)
        {
            itps_encode::appendPulse(seq, mark, kTUs, kTUs);
        }
        void appendBit(std::vector<int8_t> &seq, bool bit)
        {
            if (bit)
            {
                appendHalf(seq, true);
                appendHalf(seq, false);
            }
            else
            {
                appendHalf(seq, false);
                appendHalf(seq, true);
            }
        }
    } // namespace

    bool Transmitter::sendRC6(const esp32ir::payload::RC6 &p)
    {
        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::RC6, reinterpret_cast<const uint8_t *>(&p), static_cast<uint16_t>(sizeof(p)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
            return false;

        std::vector<int8_t> seq;
        seq.reserve(64);
        // Leader 2T mark, 2T space
        itps_encode::appendPulse(seq, true, kTUs * 2, kTUs);
        itps_encode::appendPulse(seq, false, kTUs * 2, kTUs);
        // Start bit (always 1) with double width Manchester
        // TxBitstream bit0 is the start bit
        for (uint16_t i = 0; i < bitCount; ++i)
        {
            bool bit = (txBytes[i / 8] >> (i % 8)) & 0x1;
            if (i == 0)
            {
                itps_encode::appendPulse(seq, true, kTUs * 2, kTUs);
                itps_encode::appendPulse(seq, false, kTUs * 2, kTUs);
            }
            else
            {
                appendBit(seq, bit);
            }
        }
        esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
        esp32ir::ITPSBuffer buf;
        buf.addFrame(frame);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::RC6));
    }
    bool Transmitter::sendRC6(uint32_t command, uint8_t mode, bool toggle)
    {
        esp32ir::payload::RC6 p{command, mode, toggle};
        return sendRC6(p);
    }

} // namespace esp32ir
