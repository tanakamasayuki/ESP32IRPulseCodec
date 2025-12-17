#include "ESP32IRPulseCodec.h"

namespace esp32ir {

Transmitter::Transmitter() = default;
Transmitter::Transmitter(int, bool, uint32_t) {}

bool Transmitter::setPin(int) { return false; }
bool Transmitter::setInvertOutput(bool) { return false; }
bool Transmitter::setCarrierHz(uint32_t) { return false; }
bool Transmitter::setDutyPercent(uint8_t) { return false; }
bool Transmitter::setGapUs(uint32_t) { return false; }

bool Transmitter::begin() { return false; }
void Transmitter::end() {}

bool Transmitter::send(const esp32ir::ITPSBuffer&) { return false; }
bool Transmitter::send(const esp32ir::ProtocolMessage&) { return false; }

}  // namespace esp32ir
