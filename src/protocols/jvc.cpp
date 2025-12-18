#include "ESP32IRPulseCodec.h"
#include "message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

    bool decodeJVC(const esp32ir::RxResult &in, esp32ir::payload::JVC &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::JVC, "JVC", out))
        {
            return true;
        }
        uint64_t data = 0;
        constexpr uint32_t kHdrMarkUs = 8400;
        constexpr uint32_t kHdrSpaceUs = 4200;
        constexpr uint32_t kBitMarkUs = 525;
        constexpr uint32_t kZeroSpaceUs = 525;
        constexpr uint32_t kOneSpaceUs = 1575;
        if (!nec_like::decodeRaw(in, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs, kZeroSpaceUs, kOneSpaceUs, 32, data))
        {
            return false;
        }
        out.address = static_cast<uint16_t>(data & 0xFFFF);
        out.command = static_cast<uint16_t>(data >> 16);
        return true;
    }
    bool Transmitter::sendJVC(const esp32ir::payload::JVC &p)
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kHdrMarkUs = 8400;
        constexpr uint32_t kHdrSpaceUs = 4200;
        constexpr uint32_t kBitMarkUs = 525;
        constexpr uint32_t kZeroSpaceUs = 525;
        constexpr uint32_t kOneSpaceUs = 1575;

        uint64_t data = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.command) << 16);
        constexpr uint8_t kBits = 32;
        esp32ir::ITPSBuffer buf = nec_like::build(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                  kZeroSpaceUs, kOneSpaceUs, data, kBits, true);
        return send(buf);
    }
    bool Transmitter::sendJVC(uint16_t address, uint16_t command)
    {
        esp32ir::payload::JVC p{address, command};
        return sendJVC(p);
    }

} // namespace esp32ir
