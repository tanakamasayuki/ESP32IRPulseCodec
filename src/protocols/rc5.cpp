#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/itps_encode.h"
#include "core/pulse_utils.h"
#include <vector>

namespace esp32ir
{

    bool decodeRC5(const esp32ir::RxResult &in, esp32ir::payload::RC5 &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::RC5, out))
        {
            return true;
        }
        std::vector<esp32ir::Pulse> pulses;
        if (!esp32ir::collectPulses(in.raw, pulses))
            return false;
        if (pulses.size() < 28) // 14 bits * 2 halves
            return false;
        uint32_t T = pulses[0].us;
        size_t idx = 0;
        auto nextHalf = [&]() -> esp32ir::Pulse
        {
            if (idx >= pulses.size())
                return esp32ir::Pulse{false, 0};
            return pulses[idx++];
        };
        auto okHalf = [&](const esp32ir::Pulse &p)
        { return esp32ir::inRange(p.us, T, 40); };
        // Start bits: 1,1 -> mark/space, mark/space
        esp32ir::Pulse a = nextHalf(), b = nextHalf();
        if (!a.mark || b.mark || !okHalf(a) || !okHalf(b))
            return false;
        esp32ir::Pulse c = nextHalf(), d = nextHalf();
        if (!c.mark || d.mark || !okHalf(c) || !okHalf(d))
            return false;
        esp32ir::Pulse t1 = nextHalf(), t2 = nextHalf();
        if (!okHalf(t1) || !okHalf(t2))
            return false;
        out.toggle = t1.mark && !t2.mark;
        uint16_t addr = 0;
        for (int i = 4; i >= 0; --i)
        {
            esp32ir::Pulse h1 = nextHalf(), h2 = nextHalf();
            if (!okHalf(h1) || !okHalf(h2))
                return false;
            bool bit = h1.mark && !h2.mark;
            if (bit)
                addr |= (1 << i);
        }
        uint16_t cmd = 0;
        for (int i = 5; i >= 0; --i)
        {
            esp32ir::Pulse h1 = nextHalf(), h2 = nextHalf();
            if (!okHalf(h1) || !okHalf(h2))
                return false;
            bool bit = h1.mark && !h2.mark;
            if (bit)
                cmd |= (1 << i);
        }
        out.command = cmd;
        return true;
    }
    namespace
    {
        constexpr uint16_t kTUs = 889;
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

    bool Transmitter::sendRC5(const esp32ir::payload::RC5 &p)
    {
        std::vector<int8_t> seq;
        seq.reserve(32);
        // Start bits 1,1 then toggle, then 5 address bits (0), 6 command bits (MSB first)
        appendBit(seq, true);
        appendBit(seq, true);
        appendBit(seq, p.toggle);
        uint8_t address = 0;
        for (int i = 4; i >= 0; --i)
        {
            appendBit(seq, (address >> i) & 0x1);
        }
        uint16_t cmd = p.command & 0x3F;
        for (int i = 5; i >= 0; --i)
        {
            appendBit(seq, (cmd >> i) & 0x1);
        }
        esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
        esp32ir::ITPSBuffer buf;
        buf.addFrame(frame);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::RC5));
    }
    bool Transmitter::sendRC5(uint16_t command, bool toggle)
    {
        esp32ir::payload::RC5 p{command, toggle};
        return sendRC5(p);
    }

} // namespace esp32ir
