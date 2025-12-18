#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"

namespace esp32ir
{

    bool decodeMitsubishi(const esp32ir::RxResult &in, esp32ir::payload::Mitsubishi &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::Mitsubishi, "Mitsubishi");
    }
    bool Transmitter::sendMitsubishi(const esp32ir::payload::Mitsubishi &)
    {
        esp32ir::ProtocolMessage msg{esp32ir::Protocol::Mitsubishi, nullptr, 0, 0};
        return send(msg);
    }
    bool Transmitter::sendMitsubishi(uint16_t address, uint16_t command, uint8_t extra)
    {
        esp32ir::payload::Mitsubishi p{address, command, extra};
        return sendMitsubishi(p);
    }

} // namespace esp32ir
