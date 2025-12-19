#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

    bool decodeApple(const esp32ir::RxResult &in, esp32ir::payload::Apple &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::Apple, out))
        {
            return true;
        }
        uint64_t data = 0;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;
        if (!nec_like::decodeRaw(in, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs, kZeroSpaceUs, kOneSpaceUs, 32, data))
        {
            return false;
        }
        out.address = static_cast<uint16_t>(data & 0xFFFF);
        out.command = static_cast<uint8_t>((data >> 16) & 0xFF);
        return true;
    }
    bool Transmitter::sendApple(const esp32ir::payload::Apple &p)
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;
        uint64_t data = static_cast<uint64_t>(p.address) |
                        (static_cast<uint64_t>(p.command) << 16) |
                        (static_cast<uint64_t>(~p.command & 0xFF) << 24);
        constexpr uint8_t kBits = 32;
        esp32ir::ITPSBuffer buf = nec_like::build(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                  kZeroSpaceUs, kOneSpaceUs, data, kBits, true);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::Apple));
    }
    bool Transmitter::sendApple(uint16_t address, uint8_t command)
    {
        esp32ir::payload::Apple p{address, command};
        return sendApple(p);
    }

} // namespace esp32ir
