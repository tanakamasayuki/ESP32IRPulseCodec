
# ESP32IRPulseCodec 仕様（ユーザー公開API版）

## 0. 前提
- ライブラリ名：**ESP32IRPulseCodec**
- namespace：`esp32ir`
- 対象：ESP32（RMT利用）
- 受信：ポーリングのみ
- 送信：ブロッキング送信
- 中間フォーマット：ITPS（正規化済み）
- 反転（INVERT）は送受信直前（HAL）で吸収

---

## 1. 命名
- namespace：`esp32ir`
- クラス
  - `esp32ir::Receiver`
  - `esp32ir::Transmitter`
- データ型
  - `esp32ir::RxResult`
  - `esp32ir::LogicalPacket`
  - `esp32ir::ITPSFrame`
  - `esp32ir::ITPSBuffer`
  - `esp32ir::ProtocolId`
  - `esp32ir::TxStatus`

---

## 2. Receiver（受信）

### 2.1 コンストラクタ
```cpp
esp32ir::Receiver();
esp32ir::Receiver(int rxPin);
esp32ir::Receiver(int rxPin, bool invert);
esp32ir::Receiver(int rxPin, bool invert, uint16_t T_us_rx);
```

### 2.2 セッター（begin前のみ有効）
```cpp
bool setPin(int rxPin);
bool setInvertInput(bool invert);
bool setQuantizeT(uint16_t T_us_rx);
```

### 2.3 プロトコル指定（begin前のみ）
```cpp
bool addProtocol(esp32ir::ProtocolId id);
bool clearProtocols();
bool useRawOnly();
bool useRawPlusKnown();
```

- プロトコル未指定 → ALL_KNOWN
- 指定あり → ONLY
- RAW系指定が最優先

---

## 3. poll と所有権
```cpp
bool poll(esp32ir::RxResult& out);
```

- RxResult は呼び出し側が所有
- 次回 poll で破壊されない
- 内部 FIFO キュー（上限あり）
- 溢れた場合は古いデータを破棄し OVERFLOW 通知

---

## 4. RxResult
```cpp
struct RxResult {
  enum class Status {
    DECODED,
    RAW_ONLY,
    OVERFLOW
  } status;

  esp32ir::ProtocolId protocol;
  esp32ir::LogicalPacket logical;
  esp32ir::ITPSBuffer raw;
};
```

- RAW_PLUS 時は DECODED でも raw を必ず含む

---

## 5. LogicalPacket
```cpp
struct LogicalPacket {
  esp32ir::ProtocolId protocol;
  const uint8_t* data;
  uint16_t length;
  uint32_t flags;
};
```

- プロトコル固有構造体は公開しない
- NEC 等はヘルパーで解釈

---

## 6. ITPS

### 6.1 ITPSFrame
```cpp
struct ITPSFrame {
  uint16_t T_us;
  uint16_t len;
  const int8_t* seq;
  uint8_t flags;
};
```

### 6.2 ITPSBuffer
```cpp
class ITPSBuffer {
public:
  uint16_t frameCount() const;
  const esp32ir::ITPSFrame& frame(uint16_t i) const;
  uint32_t totalTimeUs() const;
};
```

---

## 7. Transmitter（送信）

### 7.1 コンストラクタ
```cpp
esp32ir::Transmitter();
esp32ir::Transmitter(int txPin);
esp32ir::Transmitter(int txPin, bool invert, uint32_t hz);
```

### 7.2 セッター（begin前のみ）
```cpp
bool setPin(int txPin);
bool setInvertOutput(bool invert);
bool setCarrierHz(uint32_t hz);
bool setDutyPercent(uint8_t dutyPercent);
bool setGapUs(uint32_t gapUs);
```

### 7.3 begin/end
```cpp
bool begin();
void end();
```

### 7.4 send（ブロッキング）
```cpp
esp32ir::TxStatus send(const esp32ir::ITPSBuffer& raw);
esp32ir::TxStatus send(const esp32ir::LogicalPacket& p);
esp32ir::TxStatus sendNEC(uint16_t address, uint8_t command, bool repeat=false);
```

---

## 8. TxStatus
```cpp
enum class TxStatus {
  OK,
  INVALID_ARG,
  UNSUPPORTED_PROTOCOL,
  HAL_ERROR
};
```

---

## 9. 送信ギャップ
- デフォルト 40ms
- RAW/Logical/NEC すべてに適用
- 最終 Space を最低 gapUs 確保

---

## 10. サンプル（完全修飾）

### 10.1 受信
```cpp
esp32ir::Receiver rx(23);
rx.begin();
```

### 10.2 NEC 送信
```cpp
esp32ir::Transmitter tx(25);
tx.begin();
tx.sendNEC(0x00FF, 0xA2);
```

---
