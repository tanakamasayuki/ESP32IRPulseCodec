#include <ESP32IRPulseCodec.h>

// en: Simple NEC RX example
// ja: NEC受信のシンプルな例
// en: Adjust GPIO numbers to your board wiring.
// ja: GPIO番号はご利用環境の配線に合わせて変更してください。
esp32ir::Receiver rx(32);

void setup()
{
  Serial.begin(115200);

  // en: Invert if your IR demod output is active-low (most modules are)
  // ja: 受信モジュールの出力がアクティブローの場合は反転させる（多くのモジュールはそうなっている）
  rx.setInvertInput(true);

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
    Serial.printf("Received IR frame:\n");
    Serial.printf(" status=%u\n", static_cast<uint8_t>(rxResult.status));
    Serial.printf(" protocol=%u\n", static_cast<uint16_t>(rxResult.protocol));
    Serial.printf(" raw frames=%u totalUs=%lu\n",
                  rxResult.raw.frameCount(), static_cast<unsigned long>(rxResult.raw.totalTimeUs()));
    for (uint16_t i = 0; i < rxResult.raw.frameCount(); ++i)
    {
      const esp32ir::ITPSFrame &frame = rxResult.raw.frame(i);
      Serial.printf("  Frame %u: T_us=%u edges=%u\n    ",
                    i, frame.T_us, frame.len);
      for (uint16_t j = 0; j < frame.len; ++j)
      {
        Serial.printf("%d ", frame.seq[j]);
      }
      Serial.printf("\n");
    }

    esp32ir::payload::NEC nec;
    if (esp32ir::decodeNEC(rxResult, nec))
    {
      Serial.printf("addr=0x%04X cmd=0x%02X repeat=%s\n",
                    nec.address, nec.command, nec.repeat ? "true" : "false");
    }
    else
    {
      Serial.println("decodeNEC failed");
    }
  }
  delay(1);
}
