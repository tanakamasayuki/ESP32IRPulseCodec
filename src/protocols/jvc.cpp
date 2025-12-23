#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

    namespace proto_const
    {
        const RxParamPreset kJVCParams{35000, 50000, 6000, 90000, 10, 0, esp32ir::RxSplitPolicy::DROP_GAP};
    }

    bool decodeJVC(const esp32ir::RxResult &in, esp32ir::payload::JVC &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::JVC, out))
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

        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::JVC, reinterpret_cast<const uint8_t *>(&p), static_cast<uint16_t>(sizeof(p)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
            return false;
        esp32ir::ITPSBuffer buf = nec_like::buildFromTxBytes(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                             kZeroSpaceUs, kOneSpaceUs, txBytes, static_cast<uint8_t>(bitCount), true);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::JVC));
    }
    bool Transmitter::sendJVC(uint16_t address, uint16_t command)
    {
        esp32ir::payload::JVC p{address, command};
        return sendJVC(p);
    }

} // namespace esp32ir
