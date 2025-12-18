#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "itps_encode.h"
#include <vector>

namespace esp32ir
{

    bool decodeRC5(const esp32ir::RxResult &in, esp32ir::payload::RC5 &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::RC5, "RC5", out);
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
        return send(buf);
    }
    bool Transmitter::sendRC5(uint16_t command, bool toggle)
    {
        esp32ir::payload::RC5 p{command, toggle};
        return sendRC5(p);
    }

} // namespace esp32ir
