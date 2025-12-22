#include <ESP32IRPulseCodec.h>

// en: Simple NEC RX example
// ja: NEC受信のシンプルな例
// en: Adjust GPIO numbers to your board wiring.
// ja: GPIO番号はご利用環境の配線に合わせて変更してください。
esp32ir::Receiver rx(23, /*invert=*/true); // 市販IR受信モジュールは反転出力が多い

void setup()
{
  Serial.begin(115200);

  // en: Keep RAW even when decode fails so you can inspect captures
  // ja: デコードに失敗してもRAWを保持して、キャプチャ内容を確認できるようにする
  rx.useRawPlusKnown();

  // en: Start receiver
  // ja: 受信開始
  rx.begin();
}

void loop()
{
  esp32ir::RxResult rxResult;
  if (rx.poll(rxResult))
  {
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
