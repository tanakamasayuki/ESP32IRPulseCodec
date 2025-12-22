// en: Capture and dump received IR as a JSON template (fill TODO fields manually). Handy as a "dump" to view raw details + decoded info, and as the JSON format needed when adding support for unknown/unsupported protocols.
// ja: 受信したIRをJSONテンプレートとして出力（TODO欄はあとで手入力）。RAWの詳細とデコード結果を確認するダンプ用途、および未対応フォーマット追加時のJSONひな型として利用可能。
#include <ESP32IRPulseCodec.h>

// en: Receiver instance (many IR RX modules output inverted signals)
// ja: 受信インスタンス（市販IR受信モジュールは反転出力が多い）
esp32ir::Receiver rx(23, /*invert=*/true);

// en: Print decoded frame bytes as decimal array
// ja: デコード済みバイト列を10進配列で出力
static void printFrameBytes(const esp32ir::ProtocolMessage &msg)
{
  Serial.print("[");
  for (uint16_t i = 0; i < msg.length; ++i)
  {
    if (i > 0)
      Serial.print(", ");
    uint8_t b = msg.data ? msg.data[i] : 0;
    Serial.print(b); // en: decimal bytes / ja: 10進バイト列
  }
  Serial.print("]");
}

// en: Dump ITPS frames as JSON array
// ja: ITPSフレームをJSON配列として出力
static void printITPS(const esp32ir::ITPSBuffer &raw)
{
  Serial.print("\"itps\":[");
  for (uint16_t fi = 0; fi < raw.frameCount(); ++fi)
  {
    const auto &f = raw.frame(fi);
    if (fi > 0)
      Serial.print(", ");
    Serial.print('{');
    Serial.print("\"T_us\":");
    Serial.print(f.T_us);
    Serial.print(",\"flags\":");
    Serial.print(f.flags);
    Serial.print(",\"seq\":[");
    for (uint16_t i = 0; i < f.len; ++i)
    {
      if (i > 0)
        Serial.print(", ");
      Serial.print(f.seq[i]);
    }
    Serial.print("]}");
  }
  Serial.print(']');
}

// en: Emit durationsUs (first frame) reconstructed from ITPS
// ja: ITPSから復元したdurationsUs（最初のフレーム）を出力
static void printDurationsUs(const esp32ir::ITPSBuffer &raw)
{
  if (raw.frameCount() == 0)
  {
    Serial.print("[]");
    return;
  }
  const auto &f = raw.frame(0);
  Serial.print("[");
  for (uint16_t i = 0; i < f.len; ++i)
  {
    if (i > 0)
      Serial.print(", ");
    long dur = static_cast<long>(f.seq[i]) * static_cast<long>(f.T_us);
    Serial.print(dur);
  }
  Serial.print("]");
}

// en: Initialize receiver and wait for captures
// ja: 受信初期化とキャプチャ待機
void setup()
{
  Serial.begin(115200);
  delay(1000);

  rx.useRawPlusKnown(); // en: decode + keep RAW / ja: デコードしつつRAWも保持
  rx.begin();

  Serial.println("# Waiting for IR capture...");
}

// en: Poll RX, decode, and print a JSON record
// ja: 受信をポーリングし、デコードしてJSONレコードを出力
void loop()
{
  esp32ir::RxResult r;
  if (!rx.poll(r))
  {
    delay(1);
    return;
  }

  // en: decode payload (flat) if available / ja: デコード可能ならペイロードをフラット形式で取得
  bool decoded = (r.status == esp32ir::RxStatus::DECODED);
  esp32ir::payload::NEC nec{};
  esp32ir::payload::SONY sony{};
  esp32ir::payload::AEHA aeha{};
  esp32ir::payload::Panasonic pana{};
  esp32ir::payload::JVC jvc{};
  esp32ir::payload::Samsung samsung{};
  esp32ir::payload::LG lg{};
  esp32ir::payload::Denon denon{};
  esp32ir::payload::RC5 rc5{};
  esp32ir::payload::RC6 rc6{};
  esp32ir::payload::Apple apple{};
  esp32ir::payload::Pioneer pioneer{};
  esp32ir::payload::Toshiba toshiba{};
  esp32ir::payload::Mitsubishi mitsu{};
  esp32ir::payload::Hitachi hitachi{};

  if (decoded)
  {
    // en: Decode payload into flat structs by protocol
    // ja: プロトコルごとにペイロードをフラット構造体へデコード
    switch (r.protocol)
    {
    case esp32ir::Protocol::NEC:
      decoded = esp32ir::decodeNEC(r, nec);
      break;
    case esp32ir::Protocol::SONY:
      decoded = esp32ir::decodeSONY(r, sony);
      break;
    case esp32ir::Protocol::AEHA:
      decoded = esp32ir::decodeAEHA(r, aeha);
      break;
    case esp32ir::Protocol::Panasonic:
      decoded = esp32ir::decodePanasonic(r, pana);
      break;
    case esp32ir::Protocol::JVC:
      decoded = esp32ir::decodeJVC(r, jvc);
      break;
    case esp32ir::Protocol::Samsung:
      decoded = esp32ir::decodeSamsung(r, samsung);
      break;
    case esp32ir::Protocol::LG:
      decoded = esp32ir::decodeLG(r, lg);
      break;
    case esp32ir::Protocol::Denon:
      decoded = esp32ir::decodeDenon(r, denon);
      break;
    case esp32ir::Protocol::RC5:
      decoded = esp32ir::decodeRC5(r, rc5);
      break;
    case esp32ir::Protocol::RC6:
      decoded = esp32ir::decodeRC6(r, rc6);
      break;
    case esp32ir::Protocol::Apple:
      decoded = esp32ir::decodeApple(r, apple);
      break;
    case esp32ir::Protocol::Pioneer:
      decoded = esp32ir::decodePioneer(r, pioneer);
      break;
    case esp32ir::Protocol::Toshiba:
      decoded = esp32ir::decodeToshiba(r, toshiba);
      break;
    case esp32ir::Protocol::Mitsubishi:
      decoded = esp32ir::decodeMitsubishi(r, mitsu);
      break;
    case esp32ir::Protocol::Hitachi:
      decoded = esp32ir::decodeHitachi(r, hitachi);
      break;
    default:
      break;
    }
  }

  Serial.println("{");
  Serial.println("  \"version\":\"0.2\",");
  Serial.println("  \"device\":{ \"vendor\":\"TODO\", \"model\":\"TODO\", \"remote\":\"TODO\" },");
  Serial.print("  \"protocol\":\"");
  Serial.print(esp32ir::util::protocolToString(r.protocol));
  Serial.println("\",");
  Serial.print("  \"status\":\"");
  Serial.print(esp32ir::util::rxStatusToString(r.status));
  Serial.println("\",");
  Serial.print("  \"timestampMs\":");
  Serial.println(millis());
  Serial.println("  ,\"capture\":{");
  Serial.print("    \"durationsUs\":");
  printDurationsUs(r.raw); // en: Mark/Space durations (us) / ja: Mark/Spaceの長さ[us]
  Serial.println(",");  // en: keep ITPS (quantized) as full RAW / ja: ITPS（量子化RAW）を保持
  Serial.print("    ");
  printITPS(r.raw);
  Serial.println();
  Serial.println("  },");
  if (decoded)
  {
    Serial.println("  \"expected\":{");
    Serial.print("    \"protocol\":\"");
    Serial.print(esp32ir::util::protocolToString(r.protocol));
    Serial.println("\", ");
    Serial.print("    \"frameBytes\":");
    printFrameBytes(r.message);
    Serial.println(", ");
    Serial.print("    \"payload\":{");
    switch (r.protocol)
    {
    case esp32ir::Protocol::NEC:
      Serial.printf("\"address\":%u,\"command\":%u,\"repeat\":%s", nec.address, nec.command, nec.repeat ? "true" : "false");
      break;
    case esp32ir::Protocol::SONY:
      Serial.printf("\"address\":%u,\"command\":%u,\"bits\":%u", sony.address, sony.command, sony.bits);
      break;
    case esp32ir::Protocol::AEHA:
      Serial.printf("\"address\":%u,\"data\":%lu,\"nbits\":%u", aeha.address, static_cast<unsigned long>(aeha.data), aeha.nbits);
      break;
    case esp32ir::Protocol::Panasonic:
      Serial.printf("\"address\":%u,\"data\":%lu,\"nbits\":%u", pana.address, static_cast<unsigned long>(pana.data), pana.nbits);
      break;
    case esp32ir::Protocol::JVC:
      Serial.printf("\"address\":%u,\"command\":%u", jvc.address, jvc.command);
      break;
    case esp32ir::Protocol::Samsung:
      Serial.printf("\"address\":%u,\"command\":%u", samsung.address, samsung.command);
      break;
    case esp32ir::Protocol::LG:
      Serial.printf("\"address\":%u,\"command\":%u", lg.address, lg.command);
      break;
    case esp32ir::Protocol::Denon:
      Serial.printf("\"address\":%u,\"command\":%u,\"repeat\":%s", denon.address, denon.command, denon.repeat ? "true" : "false");
      break;
    case esp32ir::Protocol::RC5:
      Serial.printf("\"command\":%u,\"toggle\":%s", rc5.command, rc5.toggle ? "true" : "false");
      break;
    case esp32ir::Protocol::RC6:
      Serial.printf("\"command\":%lu,\"mode\":%u,\"toggle\":%s", static_cast<unsigned long>(rc6.command), rc6.mode, rc6.toggle ? "true" : "false");
      break;
    case esp32ir::Protocol::Apple:
      Serial.printf("\"address\":%u,\"command\":%u", apple.address, apple.command);
      break;
    case esp32ir::Protocol::Pioneer:
      Serial.printf("\"address\":%u,\"command\":%u,\"extra\":%u", pioneer.address, pioneer.command, pioneer.extra);
      break;
    case esp32ir::Protocol::Toshiba:
      Serial.printf("\"address\":%u,\"command\":%u,\"extra\":%u", toshiba.address, toshiba.command, toshiba.extra);
      break;
    case esp32ir::Protocol::Mitsubishi:
      Serial.printf("\"address\":%u,\"command\":%u,\"extra\":%u", mitsu.address, mitsu.command, mitsu.extra);
      break;
    case esp32ir::Protocol::Hitachi:
      Serial.printf("\"address\":%u,\"command\":%u,\"extra\":%u", hitachi.address, hitachi.command, hitachi.extra);
      break;
    default:
      Serial.print("\"_note\":\"payload decode not supported\"");
      break;
    }
    Serial.println("}");
    Serial.println("  },");
  }
  Serial.println("  \"notes\":\"Fill in capabilities/intents manually if needed.\"");
  Serial.println("}");
  Serial.println();
}
