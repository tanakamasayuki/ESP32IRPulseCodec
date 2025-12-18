/**
 * Loopback self-test:
 *  - Connect an IR LED (or logic wire) to kTxPin, an IR receiver module (active low) or logic wire to kRxPin.
 *  - Adjust invert flags below depending on your receiver. For an active-low demodulator set kRxInvert=true.
 *  - The sketch sends one frame for each non-AC protocol, receives it, decodes, and prints whether it matched.
 */

#include <ESP32IRPulseCodec.h>

constexpr int kTxPin = 4;
constexpr int kRxPin = 5;
constexpr bool kTxInvert = false;
constexpr bool kRxInvert = false;
constexpr uint16_t kQuantizeUs = 5;

esp32ir::Transmitter tx(kTxPin, kTxInvert);
esp32ir::Receiver rx(kRxPin, kRxInvert, kQuantizeUs);

struct TestCase
{
  const char *name;
  esp32ir::Protocol protocol;
  bool (*sendFn)(esp32ir::Transmitter &);
  bool (*verifyFn)(const esp32ir::RxResult &);
};

// --- Protocol-specific senders and verifiers --------------------------------

bool sendNEC(esp32ir::Transmitter &t)
{
  return t.sendNEC(0x00FF, 0xA2, false);
}
bool checkNEC(const esp32ir::RxResult &r)
{
  esp32ir::payload::NEC out{};
  return esp32ir::decodeNEC(r, out) && out.address == 0x00FF && out.command == 0xA2 && !out.repeat;
}

bool sendSONY(esp32ir::Transmitter &t)
{
  return t.sendSONY(0x1A, 0x2C, 12);
}
bool checkSONY(const esp32ir::RxResult &r)
{
  esp32ir::payload::SONY out{};
  return esp32ir::decodeSONY(r, out) && out.address == 0x1A && out.command == 0x2C && out.bits == 12;
}

bool sendAEHA(esp32ir::Transmitter &t)
{
  return t.sendAEHA(0x1234, 0xABCDE, 20);
}
bool checkAEHA(const esp32ir::RxResult &r)
{
  esp32ir::payload::AEHA out{};
  return esp32ir::decodeAEHA(r, out) && out.address == 0x1234 && out.data == 0xABCDE && out.nbits == 20;
}

bool sendPanasonic(esp32ir::Transmitter &t)
{
  return t.sendPanasonic(0x5555, 0x89AB, 16);
}
bool checkPanasonic(const esp32ir::RxResult &r)
{
  esp32ir::payload::Panasonic out{};
  return esp32ir::decodePanasonic(r, out) && out.address == 0x5555 && out.data == 0x89AB && out.nbits == 16;
}

bool sendJVC(esp32ir::Transmitter &t)
{
  return t.sendJVC(0x1234, 0x5678);
}
bool checkJVC(const esp32ir::RxResult &r)
{
  esp32ir::payload::JVC out{};
  return esp32ir::decodeJVC(r, out) && out.address == 0x1234 && out.command == 0x5678;
}

bool sendSamsung(esp32ir::Transmitter &t)
{
  return t.sendSamsung(0x1FE2, 0xA25D);
}
bool checkSamsung(const esp32ir::RxResult &r)
{
  esp32ir::payload::Samsung out{};
  return esp32ir::decodeSamsung(r, out) && out.address == 0x1FE2 && out.command == 0xA25D;
}

bool sendLG(esp32ir::Transmitter &t)
{
  return t.sendLG(0x20DF, 0x10EF);
}
bool checkLG(const esp32ir::RxResult &r)
{
  esp32ir::payload::LG out{};
  return esp32ir::decodeLG(r, out) && out.address == 0x20DF && out.command == 0x10EF;
}

bool sendDenon(esp32ir::Transmitter &t)
{
  return t.sendDenon(0xA25D, 0xC03F, false);
}
bool checkDenon(const esp32ir::RxResult &r)
{
  esp32ir::payload::Denon out{};
  return esp32ir::decodeDenon(r, out) && out.address == 0xA25D && out.command == 0xC03F;
}

bool sendRC5(esp32ir::Transmitter &t)
{
  return t.sendRC5(0x15, true);
}
bool checkRC5(const esp32ir::RxResult &r)
{
  esp32ir::payload::RC5 out{};
  return esp32ir::decodeRC5(r, out) && out.command == 0x15 && out.toggle;
}

bool sendRC6(esp32ir::Transmitter &t)
{
  return t.sendRC6(0x1234, 0, false);
}
bool checkRC6(const esp32ir::RxResult &r)
{
  esp32ir::payload::RC6 out{};
  return esp32ir::decodeRC6(r, out) && out.command == 0x1234 && out.mode == 0 && !out.toggle;
}

bool sendApple(esp32ir::Transmitter &t)
{
  return t.sendApple(0x1EE1, 0x0F);
}
bool checkApple(const esp32ir::RxResult &r)
{
  esp32ir::payload::Apple out{};
  return esp32ir::decodeApple(r, out) && out.address == 0x1EE1 && out.command == 0x0F;
}

bool sendPioneer(esp32ir::Transmitter &t)
{
  return t.sendPioneer(0xABCD, 0x2468, 0x55);
}
bool checkPioneer(const esp32ir::RxResult &r)
{
  esp32ir::payload::Pioneer out{};
  return esp32ir::decodePioneer(r, out) && out.address == 0xABCD && out.command == 0x2468 && out.extra == 0x55;
}

bool sendToshiba(esp32ir::Transmitter &t)
{
  return t.sendToshiba(0x34D2, 0x120F, 0xCC);
}
bool checkToshiba(const esp32ir::RxResult &r)
{
  esp32ir::payload::Toshiba out{};
  return esp32ir::decodeToshiba(r, out) && out.address == 0x34D2 && out.command == 0x120F && out.extra == 0xCC;
}

bool sendMitsubishi(esp32ir::Transmitter &t)
{
  return t.sendMitsubishi(0xAAAA, 0x5555, 0x33);
}
bool checkMitsubishi(const esp32ir::RxResult &r)
{
  esp32ir::payload::Mitsubishi out{};
  return esp32ir::decodeMitsubishi(r, out) && out.address == 0xAAAA && out.command == 0x5555 && out.extra == 0x33;
}

bool sendHitachi(esp32ir::Transmitter &t)
{
  return t.sendHitachi(0x0F0F, 0xF00F, 0x11);
}
bool checkHitachi(const esp32ir::RxResult &r)
{
  esp32ir::payload::Hitachi out{};
  return esp32ir::decodeHitachi(r, out) && out.address == 0x0F0F && out.command == 0xF00F && out.extra == 0x11;
}

// ---------------------------------------------------------------------------

TestCase kTests[] = {
    {"NEC", esp32ir::Protocol::NEC, sendNEC, checkNEC},
    {"SONY", esp32ir::Protocol::SONY, sendSONY, checkSONY},
    {"AEHA", esp32ir::Protocol::AEHA, sendAEHA, checkAEHA},
    {"Panasonic", esp32ir::Protocol::Panasonic, sendPanasonic, checkPanasonic},
    {"JVC", esp32ir::Protocol::JVC, sendJVC, checkJVC},
    {"Samsung", esp32ir::Protocol::Samsung, sendSamsung, checkSamsung},
    {"LG", esp32ir::Protocol::LG, sendLG, checkLG},
    {"Denon", esp32ir::Protocol::Denon, sendDenon, checkDenon},
    {"RC5", esp32ir::Protocol::RC5, sendRC5, checkRC5},
    {"RC6", esp32ir::Protocol::RC6, sendRC6, checkRC6},
    {"Apple", esp32ir::Protocol::Apple, sendApple, checkApple},
    {"Pioneer", esp32ir::Protocol::Pioneer, sendPioneer, checkPioneer},
    {"Toshiba", esp32ir::Protocol::Toshiba, sendToshiba, checkToshiba},
    {"Mitsubishi", esp32ir::Protocol::Mitsubishi, sendMitsubishi, checkMitsubishi},
    {"Hitachi", esp32ir::Protocol::Hitachi, sendHitachi, checkHitachi},
};

void flushReceiver()
{
  esp32ir::RxResult dummy;
  // Drain any pending frames to avoid mixing with the next test.
  while (rx.poll(dummy))
  {
  }
}

bool waitForDecode(const TestCase &tc, uint32_t timeoutMs)
{
  esp32ir::RxResult res;
  const unsigned long start = millis();
  while (millis() - start < timeoutMs)
  {
    if (rx.poll(res))
    {
      if (tc.verifyFn(res))
      {
        return true;
      }
    }
    delay(5);
  }
  return false;
}

void runTest(const TestCase &tc)
{
  Serial.printf("Running %s... ", tc.name);
  flushReceiver();
  if (!tc.sendFn(tx))
  {
    Serial.println("send failed");
    return;
  }
  if (waitForDecode(tc, 800))
  {
    Serial.println("OK");
  }
  else
  {
    Serial.println("FAIL (timeout or decode mismatch)");
  }
  delay(60);
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("\nESP32 IR Pulse Codec loopback self-test");
  Serial.printf("TX pin: %d (invert=%s), RX pin: %d (invert=%s)\n",
                kTxPin, kTxInvert ? "true" : "false", kRxPin, kRxInvert ? "true" : "false");
  Serial.println("Connect TX to RX via IR transmitter/receiver or direct wire.");

  rx.useRawPlusKnown();
  rx.useKnownWithoutAC();
  if (!tx.begin() || !rx.begin())
  {
    Serial.println("Failed to init TX/RX. Check pins and RMT driver.");
    while (true)
    {
      delay(1000);
    }
  }
}

void loop()
{
  for (const auto &tc : kTests)
  {
    runTest(tc);
  }
  Serial.println("---- cycle done ----");
  delay(1000);
}
