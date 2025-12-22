// en: Capture and dump received IR as a JSON template (fill TODO fields manually).
// ja: 受信したIRをJSONテンプレートとして出力（TODO欄はあとで手入力）。
#include <ESP32IRPulseCodec.h>

esp32ir::Receiver rx(23, /*invert=*/true); // en: Most IR RX modules are inverted / ja: 多くのIR受信モジュールは反転出力

static const char *protocolToString(esp32ir::Protocol proto)
{
  switch (proto)
  {
  case esp32ir::Protocol::RAW:
    return "RAW";
  case esp32ir::Protocol::NEC:
    return "NEC";
  case esp32ir::Protocol::SONY:
    return "SONY";
  case esp32ir::Protocol::AEHA:
    return "AEHA";
  case esp32ir::Protocol::Panasonic:
    return "Panasonic";
  case esp32ir::Protocol::JVC:
    return "JVC";
  case esp32ir::Protocol::Samsung:
    return "Samsung";
  case esp32ir::Protocol::LG:
    return "LG";
  case esp32ir::Protocol::Denon:
    return "Denon";
  case esp32ir::Protocol::RC5:
    return "RC5";
  case esp32ir::Protocol::RC6:
    return "RC6";
  case esp32ir::Protocol::Apple:
    return "Apple";
  case esp32ir::Protocol::Pioneer:
    return "Pioneer";
  case esp32ir::Protocol::Toshiba:
    return "Toshiba";
  case esp32ir::Protocol::Mitsubishi:
    return "Mitsubishi";
  case esp32ir::Protocol::Hitachi:
    return "Hitachi";
  case esp32ir::Protocol::DaikinAC:
    return "DaikinAC";
  case esp32ir::Protocol::PanasonicAC:
    return "PanasonicAC";
  case esp32ir::Protocol::MitsubishiAC:
    return "MitsubishiAC";
  case esp32ir::Protocol::ToshibaAC:
    return "ToshibaAC";
  case esp32ir::Protocol::FujitsuAC:
    return "FujitsuAC";
  default:
    return "Unknown";
  }
}

static const char *statusToString(esp32ir::RxStatus s)
{
  switch (s)
  {
  case esp32ir::RxStatus::DECODED:
    return "DECODED";
  case esp32ir::RxStatus::RAW_ONLY:
    return "RAW_ONLY";
  case esp32ir::RxStatus::OVERFLOW:
    return "OVERFLOW";
  default:
    return "UNKNOWN";
  }
}

static void printFrameBytes(const esp32ir::ProtocolMessage &msg)
{
  Serial.print("\"frameBytes\":[");
  for (uint16_t i = 0; i < msg.length; ++i)
  {
    if (i > 0)
      Serial.print(',');
    uint8_t b = msg.data ? msg.data[i] : 0;
    Serial.print(b); // en: decimal bytes / ja: 10進バイト列
  }
  Serial.print("]");
}

static void printITPS(const esp32ir::ITPSBuffer &raw)
{
  Serial.print("\"itps\":[");
  for (uint16_t fi = 0; fi < raw.frameCount(); ++fi)
  {
    const auto &f = raw.frame(fi);
    if (fi > 0)
      Serial.print(',');
    Serial.print('{');
    Serial.print("\"T_us\":");
    Serial.print(f.T_us);
    Serial.print(",\"flags\":");
    Serial.print(f.flags);
    Serial.print(",\"seq\":[");
    for (uint16_t i = 0; i < f.len; ++i)
    {
      if (i > 0)
        Serial.print(',');
      Serial.print(f.seq[i]);
    }
    Serial.print("]}");
  }
  Serial.print(']');
}

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
      Serial.print(',');
    long dur = static_cast<long>(f.seq[i]) * static_cast<long>(f.T_us);
    Serial.print(dur);
  }
  Serial.print("]");
}

void setup()
{
  Serial.begin(115200);

  const bool enableDecode = true; // en: true keeps RAW and decoded message / ja: trueでRAWとデコード結果の両方を保持
  if (enableDecode)
  {
    rx.useRawPlusKnown(); // en: decode + keep RAW / ja: デコードしつつRAWも保持
  }
  else
  {
    rx.useRawOnly(); // en: RAW only, no decode / ja: RAWのみ（デコードなし）
  }

  rx.begin();

  Serial.println(F("# Waiting for IR capture..."));
}

void loop()
{
  esp32ir::RxResult r;
  if (!rx.poll(r))
    return;

  Serial.println(F("{"));
  Serial.println(F("  \"version\":\"0.1\","));
  Serial.println(F("  \"device\":{ \"vendor\":\"TODO\", \"model\":\"TODO\", \"remote\":\"TODO\" },"));
  Serial.print(F("  \"protocol\":\""));
  Serial.print(protocolToString(r.protocol));
  Serial.println(F("\","));
  Serial.print(F("  \"status\":\""));
  Serial.print(statusToString(r.status));
  Serial.println(F("\","));
  Serial.print(F("  \"timestampMs\":"));
  Serial.println(millis());
  Serial.println(F("  ,\"capture\":{"));
  Serial.print(F("    \"durationsUs\":"));
  printDurationsUs(r.raw); // en: Mark/Space durations (us) / ja: Mark/Spaceの長さ[us]
  Serial.println(F(","));
  Serial.print(F("    \"frameBytes\":"));
  printFrameBytes(r.message); // en/ja: optional raw bytes if decoded
  Serial.println(F(","));
  Serial.print(F("    "));
  printITPS(r.raw);
  Serial.println();
  Serial.println(F("  },"));
  Serial.println(F("  \"notes\":\"Fill in capabilities/intents manually if needed.\""));
  Serial.println(F("}"));
  Serial.println();
}
