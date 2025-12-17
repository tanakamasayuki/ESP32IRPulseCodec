#include "ESP32IRPulseCodec.h"

namespace esp32ir {

Receiver::Receiver() = default;
Receiver::Receiver(int, bool, uint16_t) {}

bool Receiver::setPin(int) { return false; }
bool Receiver::setInvertInput(bool) { return false; }
bool Receiver::setQuantizeT(uint16_t) { return false; }

bool Receiver::begin() { return false; }
void Receiver::end() {}

bool Receiver::addProtocol(esp32ir::Protocol) { return false; }
bool Receiver::clearProtocols() { return false; }
bool Receiver::useRawOnly() { return false; }
bool Receiver::useRawPlusKnown() { return false; }
bool Receiver::useKnownWithoutAC() { return false; }

bool Receiver::poll(esp32ir::RxResult&) { return false; }

}  // namespace esp32ir
