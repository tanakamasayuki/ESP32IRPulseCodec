#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"
#include <vector>

namespace esp32ir
{

    bool decodePanasonic(const esp32ir::RxResult &in, esp32ir::payload::Panasonic &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::Panasonic, out))
        {
            return true;
        }
        uint64_t data = 0;
        constexpr uint32_t kHdrMarkUs = 3500;
        constexpr uint32_t kHdrSpaceUs = 1750;
        constexpr uint32_t kBitMarkUs = 502;
        constexpr uint32_t kZeroSpaceUs = 424;
        constexpr uint32_t kOneSpaceUs = 1244;
        uint8_t bits = 32;
        if (!nec_like::decodeRaw(in, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs, kZeroSpaceUs, kOneSpaceUs, bits, data))
        {
            return false;
        }
        out.address = static_cast<uint16_t>(data & 0xFFFF);
        out.data = static_cast<uint32_t>(data >> 16);
        out.nbits = 16;
        return true;
    }
    bool Transmitter::sendPanasonic(const esp32ir::payload::Panasonic &p)
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kHdrMarkUs = 3500;
        constexpr uint32_t kHdrSpaceUs = 1750;
        constexpr uint32_t kBitMarkUs = 502;
        constexpr uint32_t kZeroSpaceUs = 424;
        constexpr uint32_t kOneSpaceUs = 1244;

        if (p.nbits == 0 || p.nbits > 32)
        {
            return false;
        }
        uint64_t data = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.data) << 16);
        uint8_t totalBits = static_cast<uint8_t>(16 + p.nbits);
        esp32ir::ITPSBuffer buf = nec_like::build(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                  kZeroSpaceUs, kOneSpaceUs, data, totalBits, true);
        return send(buf);
    }
    bool Transmitter::sendPanasonic(uint16_t address, uint32_t data, uint8_t nbits)
    {
        esp32ir::payload::Panasonic p{address, data, nbits};
        return sendPanasonic(p);
    }

} // namespace esp32ir
