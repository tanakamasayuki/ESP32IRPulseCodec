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
    Serial.printf("Received IR frame:\n");
    Serial.printf(" status=%u\n", static_cast<uint8_t>(rxResult.status));
    Serial.printf(" protocol=%u\n", static_cast<uint16_t>(rxResult.protocol));
    Serial.printf(" payloadStorage=%u bytes ", static_cast<unsigned>(rxResult.payloadStorage.size()));
    for (size_t i = 0; i < rxResult.payloadStorage.size(); ++i)
    {
      Serial.printf(" %02X", rxResult.payloadStorage[i]);
    }
    Serial.println();
    Serial.printf(" raw frames=%u totalUs=%lu\n",
                  rxResult.raw.frameCount(), static_cast<unsigned long>(rxResult.raw.totalTimeUs()));
    for (uint16_t i = 0; i < rxResult.raw.frameCount(); ++i)
    {
      const esp32ir::ITPSFrame &frame = rxResult.raw.frame(i);
      Serial.printf("  Frame %u: T_us=%u edges=%u\n   ",
                    i, frame.T_us, frame.len);
      for (uint16_t j = 0; j < frame.len; ++j)
      {
        Serial.printf("%d ", frame.seq[j]);
      }
      Serial.printf("\n");

      Serial.println("  us timings: ");
      Serial.print("   ");
      uint32_t accumUs = 0;
      int prevSign = 0;
      for (uint16_t j = 0; j < frame.len; ++j)
      {
        int sign = (frame.seq[j] >= 0) ? 1 : -1;
        uint32_t durUs = static_cast<uint32_t>((frame.seq[j] < 0 ? -frame.seq[j] : frame.seq[j]) * frame.T_us);
        if (prevSign == 0 || sign == prevSign)
        {
          accumUs += durUs;
        }
        else
        {
          Serial.print(accumUs);
          Serial.print(", ");
          accumUs = durUs;
        }
        prevSign = sign;
      }
      Serial.print(accumUs);
      Serial.println();
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
