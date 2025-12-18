#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "itps_encode.h"
#include <vector>

namespace esp32ir
{

    bool decodeRC6(const esp32ir::RxResult &in, esp32ir::payload::RC6 &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::RC6, "RC6", out))
        {
            return true;
        }
        uint32_t cmd = 0;
        uint8_t mode = 0;
        bool tog = false;
        if (!decodeRC6Raw(in, cmd, mode, tog))
        {
            return false;
        }
        out.command = cmd;
        out.mode = mode;
        out.toggle = tog;
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
        std::vector<int8_t> seq;
        seq.reserve(64);
        // Leader 2T mark, 2T space
        itps_encode::appendPulse(seq, true, kTUs * 2, kTUs);
        itps_encode::appendPulse(seq, false, kTUs * 2, kTUs);
        // Start bit (always 1) with double width Manchester
        itps_encode::appendPulse(seq, true, kTUs * 2, kTUs);
        itps_encode::appendPulse(seq, false, kTUs * 2, kTUs);

        uint32_t mode = p.mode & 0x7; // 3 bits
        for (int i = 2; i >= 0; --i)
        {
            appendBit(seq, (mode >> i) & 0x1);
        }
        appendBit(seq, p.toggle);
        uint32_t cmd = p.command & 0xFFFF;
        for (int i = 15; i >= 0; --i)
        {
            appendBit(seq, (cmd >> i) & 0x1);
        }
        esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
        esp32ir::ITPSBuffer buf;
        buf.addFrame(frame);
        return send(buf);
    }
    bool Transmitter::sendRC6(uint32_t command, uint8_t mode, bool toggle)
    {
        esp32ir::payload::RC6 p{command, mode, toggle};
        return sendRC6(p);
    }

} // namespace esp32ir
