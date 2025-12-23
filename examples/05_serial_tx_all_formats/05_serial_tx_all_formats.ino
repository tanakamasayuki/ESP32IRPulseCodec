#include <ESP32IRPulseCodec.h>
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>
#include <cstdio>

// en: Serial-controlled IR transmitter that can send all supported non-AC protocols using flat helpers (sendNEC, sendSONY, ...).
// ja: シリアル経由でコマンドを受け取り、sendNEC/sendSONYなどのバラ引数版ヘルパーで全対応プロトコル（AC除外）を送信するサンプル。

esp32ir::Transmitter tx(25);

struct TxConfig
{
  int pin = 25;
  bool invert = false;
  uint32_t carrierHz = 38000;
  uint8_t duty = 50;
  int32_t gapUs = -1; // en: -1 means "use recommended per protocol" / ja: -1は「プロトコル推奨値を使用」
} gCfg;

struct SendOptions
{
  uint16_t count = 1;
  uint32_t intervalMs = 80;
};

static std::string toUpper(const std::string &s)
{
  std::string out;
  out.reserve(s.size());
  for (char c : s)
  {
    out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
  }
  return out;
}

static std::vector<std::string> splitTokens(const std::string &line)
{
  std::vector<std::string> tokens;
  std::string cur;
  for (char c : line)
  {
    if (std::isspace(static_cast<unsigned char>(c)))
    {
      if (!cur.empty())
      {
        tokens.push_back(cur);
        cur.clear();
      }
    }
    else
    {
      cur.push_back(c);
    }
  }
  if (!cur.empty())
  {
    tokens.push_back(cur);
  }
  return tokens;
}

static bool parseUint32(const std::string &s, uint32_t &out)
{
  if (s.empty())
    return false;
  char *end = nullptr;
  unsigned long v = strtoul(s.c_str(), &end, 0);
  if (end == nullptr || *end != '\0')
    return false;
  out = static_cast<uint32_t>(v);
  return true;
}

static bool parseUint16(const std::string &s, uint16_t &out)
{
  uint32_t v = 0;
  if (!parseUint32(s, v) || v > 0xFFFF)
    return false;
  out = static_cast<uint16_t>(v);
  return true;
}

static bool parseUint8(const std::string &s, uint8_t &out)
{
  uint32_t v = 0;
  if (!parseUint32(s, v) || v > 0xFF)
    return false;
  out = static_cast<uint8_t>(v);
  return true;
}

static bool parseBoolToken(const std::string &s, bool &out)
{
  std::string u = toUpper(s);
  if (u == "1" || u == "TRUE" || u == "ON" || u == "YES")
  {
    out = true;
    return true;
  }
  if (u == "0" || u == "FALSE" || u == "OFF" || u == "NO")
  {
    out = false;
    return true;
  }
  return false;
}

static bool splitKeyVal(const std::string &token, std::string &keyOut, std::string &valOut)
{
  size_t pos = token.find('=');
  if (pos == std::string::npos)
    return false;
  keyOut = toUpper(token.substr(0, pos));
  valOut = token.substr(pos + 1);
  return true;
}

static std::string hex(uint32_t v, uint8_t width)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "0x%0*X", width, static_cast<unsigned>(v));
  return std::string(buf);
}

template <typename Func>
static bool sendLoop(const char *protoName, const SendOptions &opts, Func sendOnce, const std::string &detail)
{
  for (uint16_t i = 0; i < opts.count; ++i)
  {
    if (!sendOnce())
    {
      Serial.printf("ERR %s send failed (iteration %u)\n", protoName, static_cast<unsigned>(i + 1));
      return false;
    }
    if (i + 1 < opts.count)
      delay(opts.intervalMs);
  }
  Serial.print("OK ");
  Serial.print(protoName);
  if (!detail.empty())
  {
    Serial.print(" ");
    Serial.print(detail.c_str());
  }
  Serial.printf(" count=%u interval_ms=%lu\n", static_cast<unsigned>(opts.count), static_cast<unsigned long>(opts.intervalMs));
  return true;
}

static bool parseSendOptions(const std::vector<std::string> &tokens, size_t start, SendOptions &opts, std::string &err, bool *repeatOut = nullptr, bool *toggleOut = nullptr)
{
  for (size_t i = start; i < tokens.size(); ++i)
  {
    std::string key, val;
    if (!splitKeyVal(tokens[i], key, val))
    {
      err = "invalid option: " + tokens[i];
      return false;
    }
    if (key == "COUNT")
    {
      uint32_t v = 0;
      if (!parseUint32(val, v) || v == 0 || v > 65535)
      {
        err = "COUNT must be 1..65535";
        return false;
      }
      opts.count = static_cast<uint16_t>(v);
    }
    else if (key == "INTERVAL_MS")
    {
      uint32_t v = 0;
      if (!parseUint32(val, v))
      {
        err = "INTERVAL_MS must be a number";
        return false;
      }
      opts.intervalMs = v;
    }
    else if (key == "REPEAT" && repeatOut)
    {
      bool b = false;
      if (!parseBoolToken(val, b))
      {
        err = "REPEAT must be true/false or 1/0";
        return false;
      }
      *repeatOut = b;
    }
    else if (key == "TOGGLE" && toggleOut)
    {
      bool b = false;
      if (!parseBoolToken(val, b))
      {
        err = "TOGGLE must be true/false or 1/0";
        return false;
      }
      *toggleOut = b;
    }
    else
    {
      err = "unknown option: " + key;
      return false;
    }
  }
  return true;
}

static void printHelp()
{
  Serial.println("Commands: HELP/? LIST CFG GAP <ms> CARRIER <hz> DUTY <percent> INVERT <0|1>");
  Serial.println("Protocols: NEC addr cmd [repeat] [count=<n> interval_ms=<m> repeat=<0|1>]");
  Serial.println("           SONY addr cmd bits");
  Serial.println("           AEHA addr data nbits");
  Serial.println("           PANA|PANASONIC addr data nbits");
  Serial.println("           JVC|SAMSUNG|LG|DENON addr cmd [repeat]");
  Serial.println("           RC5 cmd [toggle] | RC6 cmd mode [toggle]");
  Serial.println("           APPLE addr cmd | PIONEER/TOSHIBA/MITSUBISHI/HITACHI addr cmd [extra]");
}

static void printList()
{
  Serial.println("NEC, SONY, AEHA, PANA, JVC, SAMSUNG, LG, DENON, RC5, RC6, APPLE, PIONEER, TOSHIBA, MITSUBISHI, HITACHI");
}

static void printConfig()
{
  char gapBuf[32];
  const char *gapStr = nullptr;
  if (gCfg.gapUs < 0)
  {
    gapStr = "recommended";
  }
  else
  {
    snprintf(gapBuf, sizeof(gapBuf), "set(%dus)", static_cast<int>(gCfg.gapUs));
    gapStr = gapBuf;
  }
  Serial.printf("CFG pin=%d invert=%s carrierHz=%lu duty=%u%% gap=%s\n",
                gCfg.pin,
                gCfg.invert ? "true" : "false",
                static_cast<unsigned long>(gCfg.carrierHz),
                static_cast<unsigned>(gCfg.duty),
                gapStr);
}

static bool handleControl(const std::vector<std::string> &toks)
{
  if (toks.empty())
    return true;
  std::string cmd = toUpper(toks[0]);
  if (cmd == "HELP" || cmd == "?")
  {
    printHelp();
    return true;
  }
  if (cmd == "LIST")
  {
    printList();
    return true;
  }
  if (cmd == "CFG")
  {
    printConfig();
    return true;
  }
  if (cmd == "GAP")
  {
    if (toks.size() < 2)
    {
      Serial.println("ERR GAP <ms>");
      return true;
    }
    uint32_t ms = 0;
    if (!parseUint32(toks[1], ms))
    {
      Serial.println("ERR GAP must be a number (ms)");
      return true;
    }
    gCfg.gapUs = static_cast<int32_t>(ms * 1000UL);
    tx.setGapUs(gCfg.gapUs);
    Serial.printf("OK GAP set to %lu ms\n", static_cast<unsigned long>(ms));
    return true;
  }
  if (cmd == "CARRIER")
  {
    if (toks.size() < 2)
    {
      Serial.println("ERR CARRIER <hz>");
      return true;
    }
    uint32_t hz = 0;
    if (!parseUint32(toks[1], hz) || hz == 0)
    {
      Serial.println("ERR CARRIER must be >0");
      return true;
    }
    if (!tx.setCarrierHz(hz))
    {
      Serial.println("ERR setCarrierHz failed");
      return true;
    }
    gCfg.carrierHz = hz;
    Serial.printf("OK CARRIER %lu Hz\n", static_cast<unsigned long>(hz));
    return true;
  }
  if (cmd == "DUTY")
  {
    if (toks.size() < 2)
    {
      Serial.println("ERR DUTY <percent>");
      return true;
    }
    uint32_t duty = 0;
    if (!parseUint32(toks[1], duty) || duty > 100)
    {
      Serial.println("ERR DUTY must be 0-100");
      return true;
    }
    if (!tx.setDutyPercent(static_cast<uint8_t>(duty)))
    {
      Serial.println("ERR setDutyPercent failed");
      return true;
    }
    gCfg.duty = static_cast<uint8_t>(duty);
    Serial.printf("OK DUTY %u%%\n", static_cast<unsigned>(gCfg.duty));
    return true;
  }
  if (cmd == "INVERT")
  {
    if (toks.size() < 2)
    {
      Serial.println("ERR INVERT <0|1>");
      return true;
    }
    bool inv = false;
    if (!parseBoolToken(toks[1], inv))
    {
      Serial.println("ERR INVERT must be true/false or 0/1");
      return true;
    }
    if (!tx.setInvertOutput(inv))
    {
      Serial.println("ERR setInvertOutput failed");
      return true;
    }
    gCfg.invert = inv;
    Serial.printf("OK INVERT %s\n", inv ? "true" : "false");
    return true;
  }
  return false; // not a control command
}

static bool handleProtocol(const std::vector<std::string> &toks)
{
  if (toks.empty())
    return false;
  std::string cmd = toUpper(toks[0]);
  // NEC
  if (cmd == "NEC")
  {
    if (toks.size() < 3)
    {
      Serial.println("ERR NEC <address16> <command8> [repeat]");
      return true;
    }
    uint16_t addr = 0;
    uint8_t cmd8 = 0;
    if (!parseUint16(toks[1], addr) || !parseUint8(toks[2], cmd8))
    {
      Serial.println("ERR NEC address/command parse failed");
      return true;
    }
    bool repeat = false;
    size_t idx = 3;
    if (idx < toks.size() && toks[idx].find('=') == std::string::npos)
    {
      if (!parseBoolToken(toks[idx], repeat))
      {
        Serial.println("ERR repeat must be true/false or 0/1");
        return true;
      }
      ++idx;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, idx, opts, err, &repeat, nullptr))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() { return tx.sendNEC(addr, cmd8, repeat); };
    std::string detail = "addr=" + hex(addr, 4) + " cmd=" + hex(cmd8, 2) + " repeat=" + (repeat ? "true" : "false");
    sendLoop("NEC", opts, fn, detail);
    return true;
  }
  // SONY
  if (cmd == "SONY")
  {
    if (toks.size() < 4)
    {
      Serial.println("ERR SONY <address> <command> <bits>");
      return true;
    }
    uint16_t addr = 0;
    uint16_t command = 0;
    uint32_t bits = 0;
    if (!parseUint16(toks[1], addr) || !parseUint16(toks[2], command) || !parseUint32(toks[3], bits))
    {
      Serial.println("ERR SONY parse failed");
      return true;
    }
    if (bits != 12 && bits != 15 && bits != 20)
    {
      Serial.println("ERR SONY bits must be 12/15/20");
      return true;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, 4, opts, err))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() { return tx.sendSONY(addr, command, static_cast<uint8_t>(bits)); };
    std::string detail = "addr=" + hex(addr, 4) + " cmd=" + hex(command, 4) + " bits=" + std::to_string(bits);
    sendLoop("SONY", opts, fn, detail);
    return true;
  }
  // AEHA
  if (cmd == "AEHA")
  {
    if (toks.size() < 4)
    {
      Serial.println("ERR AEHA <address16> <data> <nbits>");
      return true;
    }
    uint16_t addr = 0;
    uint32_t data = 0;
    uint32_t nbits = 0;
    if (!parseUint16(toks[1], addr) || !parseUint32(toks[2], data) || !parseUint32(toks[3], nbits))
    {
      Serial.println("ERR AEHA parse failed");
      return true;
    }
    if (nbits == 0 || nbits > 32)
    {
      Serial.println("ERR AEHA nbits must be 1..32");
      return true;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, 4, opts, err))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() { return tx.sendAEHA(addr, data, static_cast<uint8_t>(nbits)); };
    std::string detail = "addr=" + hex(addr, 4) + " data=" + hex(data, 8) + " nbits=" + std::to_string(nbits);
    sendLoop("AEHA", opts, fn, detail);
    return true;
  }
  // Panasonic
  if (cmd == "PANA" || cmd == "PANASONIC")
  {
    if (toks.size() < 4)
    {
      Serial.println("ERR PANA <address16> <data> <nbits>");
      return true;
    }
    uint16_t addr = 0;
    uint32_t data = 0;
    uint32_t nbits = 0;
    if (!parseUint16(toks[1], addr) || !parseUint32(toks[2], data) || !parseUint32(toks[3], nbits))
    {
      Serial.println("ERR PANA parse failed");
      return true;
    }
    if (nbits == 0 || nbits > 32)
    {
      Serial.println("ERR PANA nbits must be 1..32");
      return true;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, 4, opts, err))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() { return tx.sendPanasonic(addr, data, static_cast<uint8_t>(nbits)); };
    std::string detail = "addr=" + hex(addr, 4) + " data=" + hex(data, 8) + " nbits=" + std::to_string(nbits);
    sendLoop("PANA", opts, fn, detail);
    return true;
  }
  // JVC / Samsung / LG common signature
  if (cmd == "JVC" || cmd == "SAMSUNG" || cmd == "LG")
  {
    if (toks.size() < 3)
    {
      Serial.printf("ERR %s <address16> <command16>\n", cmd.c_str());
      return true;
    }
    uint16_t addr = 0;
    uint16_t command = 0;
    if (!parseUint16(toks[1], addr) || !parseUint16(toks[2], command))
    {
      Serial.printf("ERR %s parse failed\n", cmd.c_str());
      return true;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, 3, opts, err))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() -> bool {
      if (cmd == "JVC")
        return tx.sendJVC(addr, command);
      if (cmd == "SAMSUNG")
        return tx.sendSamsung(addr, command);
      return tx.sendLG(addr, command);
    };
    std::string detail = "addr=" + hex(addr, 4) + " cmd=" + hex(command, 4);
    sendLoop(cmd.c_str(), opts, fn, detail);
    return true;
  }
  // Denon
  if (cmd == "DENON")
  {
    if (toks.size() < 3)
    {
      Serial.println("ERR DENON <address16> <command16> [repeat]");
      return true;
    }
    uint16_t addr = 0;
    uint16_t command = 0;
    if (!parseUint16(toks[1], addr) || !parseUint16(toks[2], command))
    {
      Serial.println("ERR DENON parse failed");
      return true;
    }
    bool repeat = false;
    size_t idx = 3;
    if (idx < toks.size() && toks[idx].find('=') == std::string::npos)
    {
      if (!parseBoolToken(toks[idx], repeat))
      {
        Serial.println("ERR repeat must be true/false or 0/1");
        return true;
      }
      ++idx;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, idx, opts, err, &repeat, nullptr))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() { return tx.sendDenon(addr, command, repeat); };
    std::string detail = "addr=" + hex(addr, 4) + " cmd=" + hex(command, 4) + " repeat=" + (repeat ? "true" : "false");
    sendLoop("DENON", opts, fn, detail);
    return true;
  }
  // RC5
  if (cmd == "RC5")
  {
    if (toks.size() < 2)
    {
      Serial.println("ERR RC5 <command> [toggle]");
      return true;
    }
    uint16_t command = 0;
    if (!parseUint16(toks[1], command))
    {
      Serial.println("ERR RC5 command parse failed");
      return true;
    }
    bool toggle = false;
    size_t idx = 2;
    if (idx < toks.size() && toks[idx].find('=') == std::string::npos)
    {
      if (!parseBoolToken(toks[idx], toggle))
      {
        Serial.println("ERR toggle must be true/false or 0/1");
        return true;
      }
      ++idx;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, idx, opts, err, nullptr, &toggle))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() { return tx.sendRC5(command, toggle); };
    std::string detail = "cmd=" + hex(command & 0x3F, 2) + " toggle=" + (toggle ? "true" : "false");
    sendLoop("RC5", opts, fn, detail);
    return true;
  }
  // RC6
  if (cmd == "RC6")
  {
    if (toks.size() < 3)
    {
      Serial.println("ERR RC6 <command16> <mode> [toggle]");
      return true;
    }
    uint16_t command = 0;
    uint32_t mode = 0;
    if (!parseUint16(toks[1], command) || !parseUint32(toks[2], mode))
    {
      Serial.println("ERR RC6 parse failed");
      return true;
    }
    if (mode > 7)
    {
      Serial.println("ERR RC6 mode must be 0-7");
      return true;
    }
    bool toggle = false;
    size_t idx = 3;
    if (idx < toks.size() && toks[idx].find('=') == std::string::npos)
    {
      if (!parseBoolToken(toks[idx], toggle))
      {
        Serial.println("ERR toggle must be true/false or 0/1");
        return true;
      }
      ++idx;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, idx, opts, err, nullptr, &toggle))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() { return tx.sendRC6(command, static_cast<uint8_t>(mode), toggle); };
    std::string detail = "cmd=" + hex(command, 4) + " mode=" + std::to_string(mode) + " toggle=" + (toggle ? "true" : "false");
    sendLoop("RC6", opts, fn, detail);
    return true;
  }
  // Apple
  if (cmd == "APPLE")
  {
    if (toks.size() < 3)
    {
      Serial.println("ERR APPLE <address16> <command8>");
      return true;
    }
    uint16_t addr = 0;
    uint8_t command = 0;
    if (!parseUint16(toks[1], addr) || !parseUint8(toks[2], command))
    {
      Serial.println("ERR APPLE parse failed");
      return true;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, 3, opts, err))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() { return tx.sendApple(addr, command); };
    std::string detail = "addr=" + hex(addr, 4) + " cmd=" + hex(command, 2);
    sendLoop("APPLE", opts, fn, detail);
    return true;
  }
  // Pioneer / Toshiba / Mitsubishi / Hitachi (same shape)
  if (cmd == "PIONEER" || cmd == "TOSHIBA" || cmd == "MITSUBISHI" || cmd == "HITACHI")
  {
    if (toks.size() < 3)
    {
      Serial.printf("ERR %s <address16> <command16> [extra]\n", cmd.c_str());
      return true;
    }
    uint16_t addr = 0;
    uint16_t command = 0;
    uint8_t extra = 0;
    if (!parseUint16(toks[1], addr) || !parseUint16(toks[2], command))
    {
      Serial.printf("ERR %s parse failed\n", cmd.c_str());
      return true;
    }
    size_t idx = 3;
    if (idx < toks.size() && toks[idx].find('=') == std::string::npos)
    {
      if (!parseUint8(toks[idx], extra))
      {
        Serial.println("ERR extra must be 0..255");
        return true;
      }
      ++idx;
    }
    SendOptions opts;
    std::string err;
    if (!parseSendOptions(toks, idx, opts, err))
    {
      Serial.printf("ERR %s\n", err.c_str());
      return true;
    }
    auto fn = [&]() -> bool {
      if (cmd == "PIONEER")
        return tx.sendPioneer(addr, command, extra);
      if (cmd == "TOSHIBA")
        return tx.sendToshiba(addr, command, extra);
      if (cmd == "MITSUBISHI")
        return tx.sendMitsubishi(addr, command, extra);
      return tx.sendHitachi(addr, command, extra);
    };
    std::string detail = "addr=" + hex(addr, 4) + " cmd=" + hex(command, 4) + " extra=" + hex(extra, 2);
    sendLoop(cmd.c_str(), opts, fn, detail);
    return true;
  }
  return false;
}

static void handleLine(const std::string &line)
{
  if (line.empty())
    return;
  std::vector<std::string> toks = splitTokens(line);
  if (toks.empty())
    return;
  if (handleControl(toks))
    return;
  if (handleProtocol(toks))
    return;
  Serial.println("ERR unknown command (HELP to list)");
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("# IR TX console ready");
  Serial.println("# Commands: HELP or ? to list basics.");

  // en: Initialize transmitter with default pin/carrier/duty.
  // ja: 送信機をデフォルトのピン/キャリア/dutyで初期化します。
  tx.begin();
}

void loop()
{
  static std::string line;
  while (Serial.available() > 0)
  {
    char c = static_cast<char>(Serial.read());
    if (c == '\r')
      continue;
    if (c == '\n')
    {
      handleLine(line);
      line.clear();
    }
    else
    {
      if (line.size() < 240) // en: avoid unbounded growth / ja: バッファ肥大を防ぐ
        line.push_back(c);
    }
  }
}
