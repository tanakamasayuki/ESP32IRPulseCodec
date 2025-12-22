#include <ESP32IRPulseCodec.h>

// en: Simple NEC RX example (adjust GPIO to your wiring; many IR RX modules invert output)
// ja: シンプルなNEC受信例（配線に合わせてGPIOを変更。市販IR受信モジュールは反転出力が多い）
esp32ir::Receiver rx(23, /*invert=*/true);

void setup()
{
  Serial.begin(115200);

  // en: Start receiver
  // ja: 受信開始
  rx.begin();
}

void loop()
{
  esp32ir::RxResult rxResult;
  if (rx.poll(rxResult))
  {
    // en: Try NEC decode and print result
    // ja: NECとしてデコードし、結果を表示
    esp32ir::payload::NEC nec;
    if (esp32ir::decodeNEC(rxResult, nec))
    {
      Serial.printf("Received addr=0x%04X cmd=0x%02X repeat=%s\n",
                    nec.address, nec.command, nec.repeat ? "true" : "false");
    }
    else
    {
      Serial.println("decodeNEC failed");
    }
  }

  delay(1);
}
