#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "nec_like.h"

namespace esp32ir
{

    bool decodeDenon(const esp32ir::RxResult &in, esp32ir::payload::Denon &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::Denon, "Denon", out);
    }
    bool Transmitter::sendDenon(const esp32ir::payload::Denon &p)
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;

        uint64_t data = static_cast<uint64_t>(p.address) | (static_cast<uint64_t>(p.command) << 16);
        constexpr uint8_t kBits = 32;
        esp32ir::ITPSBuffer buf = nec_like::build(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                  kZeroSpaceUs, kOneSpaceUs, data, kBits, true);
        return send(buf);
    }
    bool Transmitter::sendDenon(uint16_t address, uint16_t command, bool repeat)
    {
        esp32ir::payload::Denon p{address, command, repeat};
        return sendDenon(p);
    }

} // namespace esp32ir
