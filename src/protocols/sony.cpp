#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "itps_encode.h"
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
        for (uint8_t bits : {12, 15, 20})
        {
            if (decodeSonyRaw(in, bits, addr, cmd))
            {
                out.address = addr;
                out.command = cmd;
                out.bits = bits;
                return true;
            }
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
