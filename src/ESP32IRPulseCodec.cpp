#include "ESP32IRPulseCodec.h"

namespace esp32ir {

// ITPSBuffer stubs
uint16_t ITPSBuffer::frameCount() const { return 0; }

const esp32ir::ITPSFrame& ITPSBuffer::frame(uint16_t) const {
  static const esp32ir::ITPSFrame kEmptyFrame{0, 0, nullptr, 0};
  return kEmptyFrame;
}

uint32_t ITPSBuffer::totalTimeUs() const { return 0; }

// Receiver stubs
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

// Transmitter stubs
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

// Decode helpers (stubs)
bool decodeNEC(const esp32ir::RxResult&, esp32ir::payload::NEC&) { return false; }
bool decodeSONY(const esp32ir::RxResult&, esp32ir::payload::SONY&) { return false; }
bool decodeAEHA(const esp32ir::RxResult&, esp32ir::payload::AEHA&) { return false; }
bool decodePanasonic(const esp32ir::RxResult&, esp32ir::payload::Panasonic&) { return false; }
bool decodeJVC(const esp32ir::RxResult&, esp32ir::payload::JVC&) { return false; }
bool decodeSamsung(const esp32ir::RxResult&, esp32ir::payload::Samsung&) { return false; }
bool decodeLG(const esp32ir::RxResult&, esp32ir::payload::LG&) { return false; }
bool decodeDenon(const esp32ir::RxResult&, esp32ir::payload::Denon&) { return false; }
bool decodeRC5(const esp32ir::RxResult&, esp32ir::payload::RC5&) { return false; }
bool decodeRC6(const esp32ir::RxResult&, esp32ir::payload::RC6&) { return false; }
bool decodeApple(const esp32ir::RxResult&, esp32ir::payload::Apple&) { return false; }
bool decodePioneer(const esp32ir::RxResult&, esp32ir::payload::Pioneer&) { return false; }
bool decodeToshiba(const esp32ir::RxResult&, esp32ir::payload::Toshiba&) { return false; }
bool decodeMitsubishi(const esp32ir::RxResult&, esp32ir::payload::Mitsubishi&) { return false; }
bool decodeHitachi(const esp32ir::RxResult&, esp32ir::payload::Hitachi&) { return false; }
bool decodeDaikinAC(const esp32ir::RxResult&, esp32ir::payload::DaikinAC&) { return false; }
bool decodePanasonicAC(const esp32ir::RxResult&, esp32ir::payload::PanasonicAC&) { return false; }
bool decodeMitsubishiAC(const esp32ir::RxResult&, esp32ir::payload::MitsubishiAC&) { return false; }
bool decodeToshibaAC(const esp32ir::RxResult&, esp32ir::payload::ToshibaAC&) { return false; }
bool decodeFujitsuAC(const esp32ir::RxResult&, esp32ir::payload::FujitsuAC&) { return false; }

// Send helpers (stubs)
bool sendNEC(const esp32ir::payload::NEC&) { return false; }
bool sendNEC(uint16_t, uint8_t, bool) { return false; }
bool sendSONY(const esp32ir::payload::SONY&) { return false; }
bool sendSONY(uint16_t, uint16_t, uint8_t) { return false; }
bool sendAEHA(const esp32ir::payload::AEHA&) { return false; }
bool sendAEHA(uint16_t, uint32_t, uint8_t) { return false; }
bool sendPanasonic(const esp32ir::payload::Panasonic&) { return false; }
bool sendPanasonic(uint16_t, uint32_t, uint8_t) { return false; }
bool sendJVC(const esp32ir::payload::JVC&) { return false; }
bool sendJVC(uint16_t, uint16_t) { return false; }
bool sendSamsung(const esp32ir::payload::Samsung&) { return false; }
bool sendSamsung(uint16_t, uint16_t) { return false; }
bool sendLG(const esp32ir::payload::LG&) { return false; }
bool sendLG(uint16_t, uint16_t) { return false; }
bool sendDenon(const esp32ir::payload::Denon&) { return false; }
bool sendDenon(uint16_t, uint16_t, bool) { return false; }
bool sendRC5(const esp32ir::payload::RC5&) { return false; }
bool sendRC5(uint16_t, bool) { return false; }
bool sendRC6(const esp32ir::payload::RC6&) { return false; }
bool sendRC6(uint32_t, uint8_t, bool) { return false; }
bool sendApple(const esp32ir::payload::Apple&) { return false; }
bool sendApple(uint16_t, uint8_t) { return false; }
bool sendPioneer(const esp32ir::payload::Pioneer&) { return false; }
bool sendPioneer(uint16_t, uint16_t, uint8_t) { return false; }
bool sendToshiba(const esp32ir::payload::Toshiba&) { return false; }
bool sendToshiba(uint16_t, uint16_t, uint8_t) { return false; }
bool sendMitsubishi(const esp32ir::payload::Mitsubishi&) { return false; }
bool sendMitsubishi(uint16_t, uint16_t, uint8_t) { return false; }
bool sendHitachi(const esp32ir::payload::Hitachi&) { return false; }
bool sendHitachi(uint16_t, uint16_t, uint8_t) { return false; }
bool sendDaikinAC(const esp32ir::payload::DaikinAC&) { return false; }
bool sendPanasonicAC(const esp32ir::payload::PanasonicAC&) { return false; }
bool sendMitsubishiAC(const esp32ir::payload::MitsubishiAC&) { return false; }
bool sendToshibaAC(const esp32ir::payload::ToshibaAC&) { return false; }
bool sendFujitsuAC(const esp32ir::payload::FujitsuAC&) { return false; }

}  // namespace esp32ir
