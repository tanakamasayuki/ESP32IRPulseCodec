#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"

namespace esp32ir
{

    bool decodeSamsung(const esp32ir::RxResult &in, esp32ir::payload::Samsung &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::Samsung, out))
        {
            return true;
        }
        uint64_t data = 0;
        constexpr uint32_t kHdrMarkUs = 4500;
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
    bool Transmitter::sendSamsung(const esp32ir::payload::Samsung &p)
    {
        constexpr uint16_t kTUs = 10;
        constexpr uint32_t kHdrMarkUs = 4500;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;

        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Samsung, reinterpret_cast<const uint8_t *>(&p), static_cast<uint16_t>(sizeof(p)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
            return false;
        esp32ir::ITPSBuffer buf = nec_like::buildFromTxBytes(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                             kZeroSpaceUs, kOneSpaceUs, txBytes, static_cast<uint8_t>(bitCount), true);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::Samsung));
    }
    bool Transmitter::sendSamsung(uint16_t address, uint16_t command)
    {
        esp32ir::payload::Samsung p{address, command};
        return sendSamsung(p);
    }

    bool decodeSamsung36(const esp32ir::RxResult &in, esp32ir::payload::Samsung36 &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::Samsung36, out))
        {
            return true;
        }
        uint64_t data = 0;
        constexpr uint32_t kHdrMarkUs = 4500;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;
        if (!nec_like::decodeRaw(in, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs, kZeroSpaceUs, kOneSpaceUs, 36, data))
        {
            return false;
        }
        out.raw = data & ((1ULL << 36) - 1);
        out.bits = 36;
        return true;
    }
    bool Transmitter::sendSamsung36(const esp32ir::payload::Samsung36 &p)
    {
        constexpr uint16_t kTUs = 10;
        constexpr uint32_t kHdrMarkUs = 4500;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;

        esp32ir::payload::Samsung36 fixed = p;
        uint8_t bits = fixed.bits ? fixed.bits : 36;
        if (bits != 36)
            return false;
        fixed.bits = bits;
        fixed.raw &= ((1ULL << bits) - 1);

        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Samsung36, reinterpret_cast<const uint8_t *>(&fixed), static_cast<uint16_t>(sizeof(fixed)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
            return false;
        esp32ir::ITPSBuffer buf = nec_like::buildFromTxBytes(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                             kZeroSpaceUs, kOneSpaceUs, txBytes, static_cast<uint8_t>(bitCount), true);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::Samsung36));
    }
    bool Transmitter::sendSamsung36(uint64_t raw, uint8_t bits)
    {
        esp32ir::payload::Samsung36 p{raw, bits};
        return sendSamsung36(p);
    }

} // namespace esp32ir
