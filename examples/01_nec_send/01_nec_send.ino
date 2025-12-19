#include <ESP32IRPulseCodec.h>

// en: Simple NEC TX example
// ja: NEC送信のシンプルな例
// en: Adjust GPIO numbers to your board wiring.
// ja: GPIO番号はご利用環境の配線に合わせて変更してください。
esp32ir::Transmitter tx(25);

void setup()
{
  Serial.begin(115200);
  tx.begin();
}

void loop()
{
  // en: send helper is a Transmitter method.
  // ja: 送信ヘルパは Transmitter のメンバ関数です。
  static uint16_t address = 0x0021;
  static uint8_t command = 0xA2;
  bool ok = tx.sendNEC(address, command);
  Serial.printf("sendNEC address=0x%04X, command=0x%02X, result=%s\n", address, command, ok ? "success" : "failed");

  // en: Change address and command.
  // ja: アドレスとコマンドを変えていきます。
  address += 100;
  command += 5;

  // en: Wait a second before next send.
  // ja: 次の送信まで1秒待ちます。
  delay(1000);
}
