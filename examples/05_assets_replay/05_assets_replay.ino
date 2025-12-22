// en: Replay embedded capture assets and try to decode them.
// ja: 埋め込みキャプチャ資産を読み込み、デコードして表示するサンプル。

#include <ESP32IRPulseCodec.h>
#include "assets_embed.h"
#include <ArduinoJson.h>
#include <vector>

// en: Map protocol string to enum (minimal set; extend as needed)
// ja: プロトコル名文字列をenumに変換（必要に応じて拡張）
static esp32ir::Protocol toProtocol(const String &s)
{
  if (s == "NEC")
    return esp32ir::Protocol::NEC;
  if (s == "SONY")
    return esp32ir::Protocol::SONY;
  if (s == "AEHA")
    return esp32ir::Protocol::AEHA;
  if (s == "Panasonic")
    return esp32ir::Protocol::Panasonic;
  if (s == "JVC")
    return esp32ir::Protocol::JVC;
  if (s == "Samsung")
    return esp32ir::Protocol::Samsung;
  if (s == "LG")
    return esp32ir::Protocol::LG;
  if (s == "Denon")
    return esp32ir::Protocol::Denon;
  if (s == "RC5")
    return esp32ir::Protocol::RC5;
  if (s == "RC6")
    return esp32ir::Protocol::RC6;
  if (s == "Apple")
    return esp32ir::Protocol::Apple;
  if (s == "Pioneer")
    return esp32ir::Protocol::Pioneer;
  if (s == "Toshiba")
    return esp32ir::Protocol::Toshiba;
  if (s == "Mitsubishi")
    return esp32ir::Protocol::Mitsubishi;
  if (s == "Hitachi")
    return esp32ir::Protocol::Hitachi;
  if (s == "DaikinAC")
    return esp32ir::Protocol::DaikinAC;
  if (s == "PanasonicAC")
    return esp32ir::Protocol::PanasonicAC;
  if (s == "MitsubishiAC")
    return esp32ir::Protocol::MitsubishiAC;
  if (s == "ToshibaAC")
    return esp32ir::Protocol::ToshibaAC;
  if (s == "FujitsuAC")
    return esp32ir::Protocol::FujitsuAC;
  return esp32ir::Protocol::RAW;
}

// en: read embedded asset into RAM buffer (ESP32 stores in regular addressable memory)
// ja: 埋め込みアセットをRAMに読み込む（ESP32では通常メモリとして扱える）
static bool loadAssetToBuffer(size_t idx, std::vector<char> &out)
{
  if (idx >= assets_file_count)
    return false;
  size_t len = assets_file_sizes[idx];
  out.resize(len + 1);
  memcpy(out.data(), assets_file_data[idx], len);
  out[len] = '\0';
  return true;
}

// en: build ITPSBuffer from JSON "itps" array
// ja: JSONの"itps"配列からITPSBufferを構築
static esp32ir::ITPSBuffer buildITPS(const JsonArrayConst &frames)
{
  esp32ir::ITPSBuffer buf;
  for (JsonObjectConst f : frames)
  {
    uint16_t T_us = f["T_us"] | 0;
    uint8_t flags = f["flags"] | 0;
    JsonArrayConst seqArr = f["seq"].as<JsonArrayConst>();
    std::vector<int8_t> seq;
    seq.reserve(seqArr.size());
    for (int v : seqArr)
    {
      seq.push_back(static_cast<int8_t>(v));
    }
    esp32ir::ITPSFrame frame{T_us, static_cast<uint16_t>(seq.size()), seq.data(), flags};
    buf.addFrame(frame);
  }
  return buf;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println(F("# assets replay start"));

  for (size_t i = 0; i < assets_file_count; ++i)
  {
    Serial.print(F("## file: "));
    Serial.println(assets_file_names[i]);

    std::vector<char> jsonBuf;
    if (!loadAssetToBuffer(i, jsonBuf))
    {
      Serial.println(F("load failed"));
      continue;
    }

    // en: parse JSON (buffer is null-terminated)
    // ja: JSONをパース（バッファは終端済み）
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, jsonBuf.data());
    if (err)
    {
      Serial.print(F("JSON parse error: "));
      Serial.println(err.c_str());
      continue;
    }

    String protoStr = doc["protocol"] | "RAW";
    esp32ir::Protocol proto = toProtocol(protoStr);

    esp32ir::RxResult rx{};
    rx.status = esp32ir::RxStatus::RAW_ONLY;
    rx.protocol = proto;
    rx.message = {proto, nullptr, 0, 0};
    rx.raw = buildITPS(doc["capture"]["itps"].as<JsonArrayConst>());

    bool decoded = false;
    if (proto == esp32ir::Protocol::NEC)
    {
      esp32ir::payload::NEC nec{};
      decoded = esp32ir::decodeNEC(rx, nec);
      if (decoded)
      {
        Serial.printf("NEC decoded: addr=0x%04X cmd=0x%02X repeat=%s\n",
                      nec.address, nec.command, nec.repeat ? "true" : "false");
      }
    }
    else
    {
      Serial.println(F("decode skipped (protocol not handled in sample)"));
    }

    Serial.print(F("raw frames: "));
    Serial.println(rx.raw.frameCount());
    Serial.print(F("decoded: "));
    Serial.println(decoded ? "true" : "false");
    Serial.println();
  }
}

void loop() {}
