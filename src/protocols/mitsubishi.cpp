#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

    namespace proto_const
    {
        // AC family gap heuristics for splitting
        const RxParamPreset kACParams{
            60000,  // frameGapUs
            80000,  // hardGapUs
            8000,   // minFrameUs
            200000, // maxFrameUs
            10,     // minEdges
            4,      // frameCountMax
            esp32ir::RxSplitPolicy::KEEP_GAP_IN_FRAME};
    }

    bool decodeMitsubishi(const esp32ir::RxResult &in, esp32ir::payload::Mitsubishi &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::Mitsubishi, out))
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
    bool Transmitter::sendMitsubishi(const esp32ir::payload::Mitsubishi &p)
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;
        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Mitsubishi, reinterpret_cast<const uint8_t *>(&p), static_cast<uint16_t>(sizeof(p)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
            return false;
        esp32ir::ITPSBuffer buf = nec_like::buildFromTxBytes(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                             kZeroSpaceUs, kOneSpaceUs, txBytes, static_cast<uint8_t>(bitCount), true);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::Mitsubishi));
    }
    bool Transmitter::sendMitsubishi(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Mitsubishi p{address, command, extra};
        return sendMitsubishi(p);
    }

} // namespace esp32ir
