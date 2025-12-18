#include "ESP32IRPulseCodec.h"
#include "decoder_stub.h"
#include "send_stub.h"

namespace esp32ir
{

    bool decodePanasonic(const esp32ir::RxResult &in, esp32ir::payload::Panasonic &out)
    {
        out = {};
        return decodeStub(in, esp32ir::Protocol::Panasonic, "Panasonic");
    }
    bool Transmitter::sendPanasonic(const esp32ir::payload::Panasonic &p)
    {
        logSendStub("Panasonic");
        return send(makeProtocolMessage(esp32ir::Protocol::Panasonic, p));
    }
    bool Transmitter::sendPanasonic(uint16_t address, uint32_t data, uint8_t nbits)
    {
        esp32ir::payload::Panasonic p{address, data, nbits};
        return sendPanasonic(p);
    }

} // namespace esp32ir
