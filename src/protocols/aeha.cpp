#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "itps_encode.h"
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
        uint16_t addr = 0;
        uint32_t data = 0;
        uint8_t bits = 0;
        if (!decodeAEHARaw(in, addr, data, bits))
        {
            return false;
        }
        out.address = addr;
        out.data = data;
        out.nbits = bits;
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
