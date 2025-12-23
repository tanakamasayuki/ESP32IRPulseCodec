#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/itps_encode.h"
#include "core/pulse_utils.h"
#include <esp_log.h>
#include <vector>

namespace esp32ir
{

    namespace proto_const
    {
        // SONY gap heuristics for splitting (matches recommended params in receiver)
        const RxParamPreset kSonyParams{
            30000, // frameGapUs
            50000, // hardGapUs
            4000,  // minFrameUs
            80000, // maxFrameUs
            8,     // minEdges
            0,     // frameCountMax
            esp32ir::RxSplitPolicy::DROP_GAP};
    }

    namespace
    {
        constexpr uint16_t kTUs = 10;
        constexpr const char *kTag = "ESP32IRPulseCodec";
        constexpr uint32_t kStartMarkUs = 2400;
        constexpr uint32_t kStartSpaceUs = 600;
        constexpr uint32_t kBitSpaceUs = 600;
        constexpr uint32_t kBitMark0Us = 600;
        constexpr uint32_t kBitMark1Us = 1200;
        constexpr uint32_t kBitThresholdUs = (kBitMark0Us + kBitMark1Us) / 2;

        void appendMark(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, true, us, kTUs);
        }
        void appendSpace(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, false, us, kTUs);
        }

        esp32ir::ITPSBuffer buildSONYFromTxBits(const std::vector<uint8_t> &txBytes, uint16_t bitCount)
        {
            std::vector<int8_t> seq;
            seq.reserve(128);

            appendMark(seq, kStartMarkUs);
            appendSpace(seq, kStartSpaceUs);

            for (uint16_t i = 0; i < bitCount; ++i)
            {
                bool one = (txBytes[i / 8] >> (i % 8)) & 0x1;
                appendMark(seq, one ? kBitMark1Us : kBitMark0Us);
                appendSpace(seq, kBitSpaceUs);
            }

            esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
            esp32ir::ITPSBuffer buf;
            buf.addFrame(frame);
            return buf;
        }
    } // namespace

    bool decodeSONY(const esp32ir::RxResult &in, esp32ir::payload::SONY &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::SONY, out))
        {
            ESP_LOGD(kTag, "decodeSONY: decodeMessage fast path succeeded (bits=%u addr=%u cmd=%u)",
                     static_cast<unsigned>(out.bits), static_cast<unsigned>(out.address), static_cast<unsigned>(out.command));
            return true;
        }
        // try 12/15/20 variants
        std::vector<esp32ir::Pulse> pulses;
        if (!esp32ir::collectPulses(in.raw, pulses))
        {
            ESP_LOGD(kTag, "decodeSONY: collectPulses failed");
            return false;
        }
        // Try longer formats first to avoid misclassifying 15/20-bit frames as 12-bit.
        for (uint8_t bits : {20, 15, 12})
        {
            if (pulses.size() < 2 + bits * 2)
            {
                ESP_LOGD(kTag, "decodeSONY: bits=%u insufficient pulses (%u needed, got %u)",
                         static_cast<unsigned>(bits), static_cast<unsigned>(2 + bits * 2), static_cast<unsigned>(pulses.size()));
                continue;
            }
            size_t idx = 0;
            auto ok = [&](bool mark, uint32_t target, uint32_t tol) -> bool
            {
                if (idx >= pulses.size())
                    return false;
                const auto &p = pulses[idx];
                if (p.mark != mark || !esp32ir::inRange(p.us, target, tol))
                    return false;
                ++idx;
                return true;
            };
            idx = 0;
            if (!ok(true, kStartMarkUs, 25) || !ok(false, kStartSpaceUs, 35))
            {
                ESP_LOGD(kTag, "decodeSONY: bits=%u start mismatch", static_cast<unsigned>(bits));
                continue;
            }
            uint32_t data = 0;
            bool okBits = true;
            auto markOk = [&](uint32_t us) {
                return esp32ir::inRange(us, kBitMark0Us, 35) || esp32ir::inRange(us, kBitMark1Us, 35);
            };
            for (uint8_t i = 0; i < bits; ++i)
            {
                if (idx >= pulses.size() || !markOk(pulses[idx].us) || !pulses[idx].mark)
                {
                    ESP_LOGD(kTag, "decodeSONY: bits=%u bit%u mark invalid (idx=%u len=%u mark=%d)",
                             static_cast<unsigned>(bits), static_cast<unsigned>(i), static_cast<unsigned>(idx),
                             idx < pulses.size() ? static_cast<unsigned>(pulses[idx].us) : 0,
                             idx < pulses.size() ? static_cast<int>(pulses[idx].mark) : -1);
                    okBits = false;
                    break;
                }
                uint32_t markLen = pulses[idx].us;
                ++idx;
                if (idx >= pulses.size())
                {
                    if (i != bits - 1)
                    {
                        ESP_LOGD(kTag, "decodeSONY: bits=%u bit%u space missing (idx=%u)", static_cast<unsigned>(bits),
                                 static_cast<unsigned>(i), static_cast<unsigned>(idx));
                        okBits = false;
                        break;
                    }
                    // Last bit without trailing space; accept end-of-frame.
                    continue;
                }
                const auto &spacePulse = pulses[idx];
                bool spaceOk = esp32ir::inRange(spacePulse.us, kBitSpaceUs, 35);
                bool finalGapOk = (i == bits - 1) && !spacePulse.mark && spacePulse.us >= kBitSpaceUs;
                if (spacePulse.mark || (!spaceOk && !finalGapOk))
                {
                    ESP_LOGD(kTag, "decodeSONY: bits=%u bit%u space invalid (idx=%u len=%u mark=%d)",
                             static_cast<unsigned>(bits), static_cast<unsigned>(i), static_cast<unsigned>(idx),
                             static_cast<unsigned>(spacePulse.us), static_cast<int>(spacePulse.mark));
                    okBits = false;
                    break;
                }
                bool one = markLen > kBitThresholdUs;
                if (one)
                    data |= (1u << i);
                ++idx;
            }
            if (!okBits)
            {
                ESP_LOGD(kTag, "decodeSONY: bits=%u failed mid-bits", static_cast<unsigned>(bits));
                continue;
            }
            out.address = static_cast<uint16_t>(data >> 7);
            out.command = static_cast<uint16_t>(data & 0x7F);
            out.bits = bits;
            ESP_LOGD(kTag, "decodeSONY: matched bits=%u addr=%u cmd=%u", static_cast<unsigned>(bits),
                     static_cast<unsigned>(out.address), static_cast<unsigned>(out.command));
            return true;
        }
        ESP_LOGD(kTag, "decodeSONY: no variant matched");
        return false;
    }
    bool Transmitter::sendSONY(const esp32ir::payload::SONY &p)
    {
        uint8_t bits = p.bits;
        if (bits != 12 && bits != 15 && bits != 20)
        {
            ESP_LOGE("ESP32IRPulseCodec", "SONY bits must be 12/15/20 (got %u)", static_cast<unsigned>(bits));
            return false;
        }
        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::SONY, reinterpret_cast<const uint8_t *>(&p), static_cast<uint16_t>(sizeof(p)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
            return false;
        return sendWithGap(buildSONYFromTxBits(txBytes, bitCount), recommendedGapUs(esp32ir::Protocol::SONY));
    }
    bool Transmitter::sendSONY(uint16_t address, uint16_t command, uint8_t bits)
    {
        esp32ir::payload::SONY p{address, command, bits};
        return sendSONY(p);
    }

} // namespace esp32ir
