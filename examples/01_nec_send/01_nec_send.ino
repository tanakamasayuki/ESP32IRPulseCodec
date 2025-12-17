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
  bool ok = tx.sendNEC(0x00FF, 0xA2);
  Serial.printf("sendNEC result: %s\n", ok ? "success" : "failed");
}

void loop()
{
  delay(1000);
}
