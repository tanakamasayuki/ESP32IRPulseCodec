#include "ESP32IRPulseCodec.h"
#include "core/message_utils.h"
#include "core/itps_encode.h"
#include "core/pulse_utils.h"
#include <vector>

namespace esp32ir
{

    bool buildNECTxBitstream(uint16_t address, uint8_t command, std::vector<uint8_t> &out)
    {
        out.clear();
        out.reserve(4);
        uint8_t addrLo = static_cast<uint8_t>(address & 0xFF);
        uint8_t addrHi = static_cast<uint8_t>((address >> 8) & 0xFF);
        if (address <= 0xFF)
        {
            addrHi = static_cast<uint8_t>(~addrLo); // NEC 8-bit address: fill complement
        }
        out.push_back(addrLo);
        out.push_back(addrHi);
        out.push_back(command);
        out.push_back(static_cast<uint8_t>(~command));
        return true;
    }

    namespace
    {
        constexpr uint16_t kTUs = 5; // default quantization
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;
        constexpr uint32_t kRepeatSpaceUs = 2250;
        constexpr uint32_t kRepeatGapMarkUs = 560;
        constexpr uint32_t kGapUs = 0; // use recommendedGapUs()

        void appendMark(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, true, us, kTUs);
        }
        void appendSpace(std::vector<int8_t> &seq, uint32_t us)
        {
            itps_encode::appendPulse(seq, false, us, kTUs);
        }

        void appendBit(std::vector<int8_t> &seq, bool one)
        {
            appendMark(seq, kBitMarkUs);
            appendSpace(seq, one ? kOneSpaceUs : kZeroSpaceUs);
        }

        void appendBitstream(std::vector<int8_t> &seq, const std::vector<uint8_t> &txBytes, uint16_t bitCount)
        {
            for (uint16_t bit = 0; bit < bitCount; ++bit)
            {
                uint8_t b = txBytes[bit / 8];
                appendBit(seq, (b >> (bit % 8)) & 0x1); // LSB-first per byte
            }
        }

        bool decodeNecRaw(const esp32ir::RxResult &in, bool &isRepeat, uint64_t &dataOut)
        {
            isRepeat = false;
            std::vector<esp32ir::Pulse> pulses;
            if (!esp32ir::collectPulses(in.raw, pulses))
                return false;
            size_t idx = 0;
            auto expect = [&](bool mark, uint32_t targetUs, uint32_t tol) -> bool
            {
                if (idx >= pulses.size())
                    return false;
                const auto &p = pulses[idx];
                if (p.mark != mark || !esp32ir::inRange(p.us, targetUs, tol))
                    return false;
                ++idx;
                return true;
            };
            if (!expect(true, kHdrMarkUs, 25))
                return false;
            if (idx >= pulses.size() || pulses[idx].mark)
                return false;

            bool spaceIsHdr = esp32ir::inRange(pulses[idx].us, kHdrSpaceUs, 25);
            bool spaceIsRepeatGap = esp32ir::inRange(pulses[idx].us, kRepeatSpaceUs, 30);
            if (!spaceIsHdr && !spaceIsRepeatGap)
                return false;
            ++idx;
            // NEC repeat frame: 9000 mark + 2250 space + 560 mark
            if (spaceIsRepeatGap)
            {
                if (expect(true, kRepeatGapMarkUs, 30))
                {
                    isRepeat = true;
                    return true;
                }
                return false;
            }
            // Short frame with normal header space but no data: also treat as repeat.
            if (pulses.size() - idx <= 2)
            {
                if (expect(true, kRepeatGapMarkUs, 30))
                {
                    isRepeat = true;
                    return true;
                }
                return false;
            }
            uint64_t data = 0;
            for (uint8_t i = 0; i < 32; ++i)
            {
                if (!expect(true, kBitMarkUs, 30))
                    return false;
                if (idx >= pulses.size() || pulses[idx].mark)
                    return false;
                bool one = esp32ir::inRange(pulses[idx].us, kOneSpaceUs, 30);
                bool zero = esp32ir::inRange(pulses[idx].us, kZeroSpaceUs, 30);
                if (!one && !zero)
                    return false;
                if (one)
                    data |= (uint64_t{1} << i);
                ++idx;
            }
            dataOut = data;
            return true;
        }

        esp32ir::ITPSBuffer buildNECFrame(const std::vector<uint8_t> &txBytes, uint16_t bitCount)
        {
            std::vector<int8_t> seq;
            seq.reserve(256);

            appendMark(seq, kHdrMarkUs);
            appendSpace(seq, kHdrSpaceUs);

            appendBitstream(seq, txBytes, bitCount);
            appendMark(seq, kBitMarkUs); // stop bit

            esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
            esp32ir::ITPSBuffer buf;
            buf.addFrame(frame);
            return buf;
        }

        esp32ir::ITPSBuffer buildNECRepeat()
        {
            std::vector<int8_t> seq;
            seq.reserve(64);

            appendMark(seq, kHdrMarkUs);
            appendSpace(seq, kRepeatSpaceUs);
            appendMark(seq, kRepeatGapMarkUs);

            esp32ir::ITPSFrame frame{kTUs, static_cast<uint16_t>(seq.size()), seq.data(), 0};
            esp32ir::ITPSBuffer buf;
            buf.addFrame(frame);
            return buf;
        }
    } // namespace

    bool decodeNEC(const esp32ir::RxResult &in, esp32ir::payload::NEC &out)
    {
        out = {};
        if (decodeMessage(in, esp32ir::Protocol::NEC, out))
        {
            return true;
        }
        uint64_t data = 0;
        bool isRepeat = false;
        if (!decodeNecRaw(in, isRepeat, data))
        {
            return false;
        }
        if (isRepeat)
        {
            // Repeat uses previous address/command; keep whatever caller had.
            out.repeat = true;
            return true;
        }
        uint8_t addrLo = static_cast<uint8_t>(data & 0xFF);
        uint8_t addrHi = static_cast<uint8_t>((data >> 8) & 0xFF);
        uint8_t cmd = static_cast<uint8_t>((data >> 16) & 0xFF);
        uint8_t cmdInv = static_cast<uint8_t>((data >> 24) & 0xFF);
        // Reject if command inverse mismatches (avoids garbage decode).
        if (cmdInv != static_cast<uint8_t>(~cmd))
        {
            return false;
        }
        bool has8bitAddress = (addrHi == static_cast<uint8_t>(~addrLo));
        out.address = has8bitAddress ? static_cast<uint16_t>(addrLo)
                                     : static_cast<uint16_t>((addrHi << 8) | addrLo);
        out.command = cmd;
        out.repeat = false;
        return true;
    }

    bool Transmitter::sendNEC(const esp32ir::payload::NEC &p)
    {
        // If repeat=true, send the NEC repeat code; otherwise full 32-bit frame.
        if (p.repeat)
        {
            return sendWithGap(buildNECRepeat(), recommendedGapUs(esp32ir::Protocol::NEC));
        }
        std::vector<uint8_t> txBytes;
        uint16_t bitCount = 0;
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::NEC, reinterpret_cast<const uint8_t *>(&p), static_cast<uint16_t>(sizeof(p)), 0};
        if (!esp32ir::buildTxBitstream(msg, txBytes, bitCount) || bitCount == 0)
        {
            ESP_LOGE("ESP32IRPulseCodec", "NEC tx bitstream build failed");
            return false;
        }
        return sendWithGap(buildNECFrame(txBytes, bitCount), recommendedGapUs(esp32ir::Protocol::NEC));
    }

    bool Transmitter::sendNEC(uint16_t address, uint8_t command, bool repeat)
    {
        esp32ir::payload::NEC p{address, command, repeat};
        return sendNEC(p);
    }

} // namespace esp32ir
