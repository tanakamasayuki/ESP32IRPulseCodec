#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

    namespace proto_const
    {
        // LG/Denon/Toshiba/Mitsubishi/Hitachi/Pioneer shared gap heuristics
        const RxParamPreset kLgGroupParams{
            40000,  // frameGapUs
            50000,  // hardGapUs
            8000,   // minFrameUs
            120000, // maxFrameUs
            10,     // minEdges
            0,      // frameCountMax
            esp32ir::RxSplitPolicy::DROP_GAP};
    }

    bool decodeLG(const esp32ir::RxResult &in, esp32ir::payload::LG &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::LG, out))
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
        return true;
    }
    bool Transmitter::sendLG(const esp32ir::payload::LG &p)
    {
        constexpr uint16_t kTUs = 10;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;

        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::LG, reinterpret_cast<const uint8_t *>(&p), static_cast<uint16_t>(sizeof(p)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
            return false;
        esp32ir::ITPSBuffer buf = nec_like::buildFromTxBytes(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                             kZeroSpaceUs, kOneSpaceUs, txBytes, static_cast<uint8_t>(bitCount), true);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::LG));
    }
    bool Transmitter::sendLG(uint16_t address, uint16_t command)
    {
        esp32ir::payload::LG p{address, command};
        return sendLG(p);
    }

} // namespace esp32ir
