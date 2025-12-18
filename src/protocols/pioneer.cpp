#include "ESP32IRPulseCodec.h"
#include "message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

    bool decodePioneer(const esp32ir::RxResult &in, esp32ir::payload::Pioneer &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::Pioneer, "Pioneer", out))
        {
            return true;
        }
        uint64_t data = 0;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;
        if (!nec_like::decodeRaw(in, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs, kZeroSpaceUs, kOneSpaceUs, 40, data))
        {
            return false;
        }
        out.address = static_cast<uint16_t>(data & 0xFFFF);
        out.command = static_cast<uint16_t>((data >> 16) & 0xFFFF);
        out.extra = static_cast<uint8_t>((data >> 32) & 0xFF);
        return true;
    }
    bool Transmitter::sendPioneer(const esp32ir::payload::Pioneer &p)
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;

        uint64_t data = static_cast<uint64_t>(p.address) |
                        (static_cast<uint64_t>(p.command) << 16) |
                        (static_cast<uint64_t>(p.extra) << 32);
        constexpr uint8_t kBits = 40;
        esp32ir::ITPSBuffer buf = nec_like::build(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                  kZeroSpaceUs, kOneSpaceUs, data, kBits, true);
        return send(buf);
    }
    bool Transmitter::sendPioneer(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Pioneer p{address, command, extra};
        return sendPioneer(p);
    }

} // namespace esp32ir
