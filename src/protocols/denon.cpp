#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

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
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;
        if (!nec_like::decodeRaw(in, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs, kZeroSpaceUs, kOneSpaceUs, 32, data))
        {
            return false;
        }
        out.address = static_cast<uint16_t>(data & 0xFFFF);
        out.command = static_cast<uint16_t>(data >> 16);
        // Detect repeat: same header but short gap/mark (similar to NEC repeat style)
        // Here we detect by shorter overall length; leave repeat=true when length is small.
        out.repeat = in.raw.totalTimeUs() < 20000;
        return true;
    }
    bool Transmitter::sendDenon(const esp32ir::payload::Denon &p)
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;
        constexpr uint32_t kGapUs = 40000;

        uint64_t data = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.command) << 16);
        constexpr uint8_t kBits = 32;
        esp32ir::ITPSBuffer buf = nec_like::build(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                  kZeroSpaceUs, kOneSpaceUs, data, kBits, true);
        if (p.repeat)
        {
            return sendWithGap(buf, kGapUs);
        }
        return sendWithGap(buf, kGapUs);
    }
    bool Transmitter::sendDenon(uint16_t address, uint16_t command, bool repeat)
    {
        esp32ir::payload::Denon p{address, command, repeat};
        return sendDenon(p);
    }

} // namespace esp32ir
