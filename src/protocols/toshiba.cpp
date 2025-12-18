#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "nec_like.h"

namespace esp32ir
{

    bool decodeToshiba(const esp32ir::RxResult &in, esp32ir::payload::Toshiba &out)
    {
        out = {};
        return decodeMessage(in, esp32ir::Protocol::Toshiba, "Toshiba", out);
    }
    bool Transmitter::sendToshiba(const esp32ir::payload::Toshiba &p)
    {
        constexpr uint16_t kTUs = 5;
        constexpr uint32_t kHdrMarkUs = 9000;
        constexpr uint32_t kHdrSpaceUs = 4500;
        constexpr uint32_t kBitMarkUs = 560;
        constexpr uint32_t kZeroSpaceUs = 560;
        constexpr uint32_t kOneSpaceUs = 1690;

        uint64_t data = static_cast<uint64_t>(p.address) |
                        (static_cast<uint64_t>(p.command) << 16) |
                        (static_cast<uint64_t>(p.extra) << 32);
        constexpr uint8_t kBits = 40;
        esp32ir::ITPSBuffer buf = nec_like::build(kTUs, kHdrMarkUs, kHdrSpaceUs, kBitMarkUs,
                                                  kZeroSpaceUs, kOneSpaceUs, data, kBits, true);
        return send(buf);
    }
    bool Transmitter::sendToshiba(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Toshiba p{address, command, extra};
        return sendToshiba(p);
    }

} // namespace esp32ir
