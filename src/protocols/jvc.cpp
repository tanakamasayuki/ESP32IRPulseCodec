#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "nec_like.h"
#include <esp_log.h>

namespace esp32ir
{

    namespace proto_const
    {
        // JVC gap heuristics for splitting (matches recommended params in receiver)
        const RxParamPreset kJVCParams{
            35000, // frameGapUs
            50000, // hardGapUs
            6000,  // minFrameUs
            90000, // maxFrameUs
            10,    // minEdges
            0,     // frameCountMax
            esp32ir::RxSplitPolicy::DROP_GAP};
    }

    bool decodeJVC(const esp32ir::RxResult &in, esp32ir::payload::JVC &out)
    {
        constexpr const char *kTag = "ESP32IRPulseCodec";
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::JVC, out))
        {
            ESP_LOGD(kTag, "decodeJVC: decodeMessage fast path succeeded addr=%u cmd=%u",
                     static_cast<unsigned>(out.address), static_cast<unsigned>(out.command));
            return true;
        }
        constexpr uint32_t kHdrMarkUs = 8400;
        constexpr uint32_t kHdrSpaceUs = 4200;
        constexpr uint32_t kBitMarkUs = 525;
        constexpr uint32_t kZeroSpaceUs = 525;
        constexpr uint32_t kOneSpaceUs = 1575;

        auto tryBits = [&](uint8_t bits) -> bool
        {
            uint64_t data = 0;
            if (!nec_like::decodeRaw(in, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs, kZeroSpaceUs, kOneSpaceUs, bits, data))
                return false;
            if (bits == 32)
            {
                out.address = static_cast<uint16_t>(data & 0xFFFF);
                uint8_t cmd = static_cast<uint8_t>((data >> 16) & 0xFF);
                uint8_t inv = static_cast<uint8_t>((data >> 24) & 0xFF);
                if ((cmd ^ inv) != 0xFF)
                    return false;
                out.command = cmd;
            }
            else if (bits == 24)
            {
                out.address = static_cast<uint16_t>(data & 0xFFFF);
                out.command = static_cast<uint8_t>((data >> 16) & 0xFF);
            }
            else
            {
                return false;
            }
            out.bits = bits;
            ESP_LOGD(kTag, "decodeJVC: decoded %ubits addr=0x%04X cmd=0x%02X",
                     static_cast<unsigned>(bits),
                     static_cast<unsigned>(out.address),
                     static_cast<unsigned>(out.command));
            return true;
        };

        if (tryBits(32))
            return true;
        if (tryBits(24))
            return true;

        ESP_LOGD(kTag, "decodeJVC: nec_like::decodeRaw failed");
        return false;
    }
    bool Transmitter::sendJVC(const esp32ir::payload::JVC &p)
    {
        constexpr uint16_t kTUs = 10;
        constexpr uint32_t kHdrMarkUs = 8400;
        constexpr uint32_t kHdrSpaceUs = 4200;
        constexpr uint32_t kBitMarkUs = 525;
        constexpr uint32_t kZeroSpaceUs = 525;
        constexpr uint32_t kOneSpaceUs = 1575;

        uint8_t bits = p.bits ? p.bits : 32;
        if (bits != 24 && bits != 32)
        {
            ESP_LOGE("ESP32IRPulseCodec", "JVC bits must be 24 or 32 (got %u)", static_cast<unsigned>(bits));
            return false;
        }
        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::JVC, reinterpret_cast<const uint8_t *>(&p), static_cast<uint16_t>(sizeof(p)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
            return false;
        esp32ir::ITPSBuffer buf = nec_like::buildFromTxBytes(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                             kZeroSpaceUs, kOneSpaceUs, txBytes, static_cast<uint8_t>(bitCount), true);
        return sendWithGap(buf, recommendedGapUs(esp32ir::Protocol::JVC));
    }
    bool Transmitter::sendJVC(uint16_t address, uint8_t command, uint8_t bits)
    {
        esp32ir::payload::JVC p{address, command, bits};
        return sendJVC(p);
    }

} // namespace esp32ir
