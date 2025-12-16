# ESP32IRPulseCodec 仕様

## 0. 前提
- ライブラリ名：**ESP32IRPulseCodec**
- namespace：`esp32ir`
- 対象：ESP32（RMT利用）
- 中間フォーマット：ITPS（正規化済み、詳細は `SPEC_ITPS.ja.md`）
- 反転（INVERT）は送受信直前（HAL）で吸収
- 受信：ポーリングのみ
- 送信：ブロッキング送信
- 受信側の量子化（`T_us` 決定）とノイズ処理は前段で済ませ、ITPSには正規化済みデータを渡す

## 1. 目的と非目的

### 1.1 目的
- ESP32のRMTを用いた、ジッタに強いIR送受信
- ITPSを入出力として、デコーダ/エンコーダの移植性を確保
- 既知プロトコルのデコードと、AC等の長大データを含むRAW取得の両立（モード切り替えで制御）
- 送受信での反転はHAL層で吸収し、ITPSは常に正のMark/負のSpaceで保持

### 1.2 非目的
- すべてのIRプロトコルの完全実装（まずは枠組み優先）
- 送受信波形の圧縮・暗号化等の付加機能
- RTOSタスク管理の自由度提供（内部で必要最低限のみ使用）

## 2. 用語
- **Mark**：搬送波（キャリア）ON（IR LED点灯）
- **Space**：搬送波OFF（IR LED消灯）
- **Frame**：Mark/Spaceの区間列（ITPSFrame）
- **HAL**：ESP32固有層（RMT、GPIO、キャリア、反転）
- **Codec**：プロトコル固有のデコード/エンコード（NECなど）

## 3. 全体アーキテクチャ

### 3.1 データフロー（受信）
Raw(RMT) → Split/Quantize → **ITPSFrame配列（ITPSBuffer）** → Decode（有効プロトコル） → 論理データ  
- `useRawOnly()` 指定時はデコードをスキップし、ITPSをRAWとして返す。  
- `useRawPlusKnown()` 指定時はデコード成功でもITPSを添付する。  
- プロトコル集合は `addProtocol/clearProtocols/useRawOnly/useRawPlusKnown` により決定する。  

### 3.2 データフロー（送信）
論理データ → Encode → **ITPSFrame配列** → HAL(RMT)送信  
反転はHALで吸収する（ITPSは変更しない）。

### 3.3 レイヤ構造
1) **Core（機種非依存）**：ITPSFrame/Normalize、ProtocolCodecインタフェース  
2) **ESP32 HAL**：RMT受信/送信、GPIO/キャリア設定、反転、バッファ管理  
3) **Arduino API**：`esp32ir::Receiver` / `esp32ir::Transmitter`

## 4. 受信モードと分割ポリシー（推奨）

- モード
  - **KNOWN_ONLY**（デフォルト）：有効プロトコルでデコード。RAWは `useRawPlusKnown()` 指定時のみ添付。
  - **RAW_ONLY**（`useRawOnly()`）：デコードせずITPSを返す。
  - **RAW_PLUS_KNOWN**（`useRawPlusKnown()`）：デコードを試み、成功してもRAWを保持。
- 分割/ロバストネス
  - `frameGapUs`/`hardGapUs` を基準にフレーム分割。`hardGapUs` を超える長大Spaceは強制分割。
  - `maxFrameUs` 超過が予測される場合は Space 境界で強制分割し、破棄を避ける。
  - `minFrameUs`/`minEdges` でノイズを前段で除去（RxResultは発行しない）。
  - `splitPolicy`：`DROP_GAP`（デコード向け、ギャップをフレームに含めない） / `KEEP_GAP_IN_FRAME`（RAW向け）。
  - `frameCountMax` 超過時は `OVERFLOW` として通知し、取得できたRAWを返す。
- `T_us` は全フレーム共通の量子化値とし、既定は 5us を想定（前段で調整）。
- 推奨プリセット（参考値）
  - デコード優先（KNOWN_ONLY 想定）：`frameGapUs=20000`, `hardGapUs=50000`, `minEdges=10`, `minFrameUs=3000`, `maxFrameUs=120000`, `splitPolicy=DROP_GAP`
  - RAW重視（AC学習など）：`frameGapUs=30000`, `hardGapUs=80000`, `minEdges=6`, `minFrameUs=2000`, `maxFrameUs=450000`, `splitPolicy=KEEP_GAP_IN_FRAME`, `frameCountMax=8`

---

## 5. 命名
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

## 6. Receiver（受信）

### 6.1 コンストラクタ
```cpp
esp32ir::Receiver();
esp32ir::Receiver(int rxPin);
esp32ir::Receiver(int rxPin, bool invert);
esp32ir::Receiver(int rxPin, bool invert, uint16_t T_us_rx);
```

### 6.2 セッター（begin前のみ有効）
```cpp
bool setPin(int rxPin);
bool setInvertInput(bool invert);
bool setQuantizeT(uint16_t T_us_rx);
```

### 6.3 プロトコル指定（begin前のみ）
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

## 7. poll と所有権
```cpp
bool poll(esp32ir::RxResult& out);
```

- RxResult は呼び出し側が所有
- 次回 poll で破壊されない
- 内部 FIFO キュー（上限あり）
- 溢れた場合は古いデータを破棄し OVERFLOW 通知
- ノイズ判定で破棄した場合は RxResult を発行しない

---

## 8. RxResult
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
- RAW_ONLY 時は protocol/logical は未使用
- OVERFLOW 時も取得できた範囲の raw を返す

---

## 9. LogicalPacket
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

## 10. ITPS

### 10.1 ITPSFrame
```cpp
struct ITPSFrame {
  uint16_t T_us;
  uint16_t len;
  const int8_t* seq;
  uint8_t flags;
};
```

### 10.2 ITPSBuffer
```cpp
class ITPSBuffer {
public:
  uint16_t frameCount() const;
  const esp32ir::ITPSFrame& frame(uint16_t i) const;
  uint32_t totalTimeUs() const;
};
```

- `T_us` は全フレームで共通とし、正規化済み ITPS を受け渡す

---

## 11. Transmitter（送信）

### 11.1 コンストラクタ
```cpp
esp32ir::Transmitter();
esp32ir::Transmitter(int txPin);
esp32ir::Transmitter(int txPin, bool invert, uint32_t hz);
```

### 11.2 セッター（begin前のみ）
```cpp
bool setPin(int txPin);
bool setInvertOutput(bool invert);
bool setCarrierHz(uint32_t hz);
bool setDutyPercent(uint8_t dutyPercent);
bool setGapUs(uint32_t gapUs);
```

### 11.3 begin/end
```cpp
bool begin();
void end();
```

### 11.4 send（ブロッキング）
```cpp
esp32ir::TxStatus send(const esp32ir::ITPSBuffer& raw);
esp32ir::TxStatus send(const esp32ir::LogicalPacket& p);
esp32ir::TxStatus sendNEC(uint16_t address, uint8_t command, bool repeat=false);
```

---

## 12. TxStatus
```cpp
enum class TxStatus {
  OK,
  INVALID_ARG,
  UNSUPPORTED_PROTOCOL,
  HAL_ERROR
};
```

---

## 13. 送信ギャップ
- デフォルト 40ms
- RAW/Logical/NEC すべてに適用
- 最終 Space を最低 gapUs 確保

---

## 14. ESP32 HAL（RMT利用）
- 受信：RMTのMark/Space dur列を取得し、量子化・分割後にITPSへ変換。`invertInput` はRMT設定または受信後の解釈で吸収。バッファ不足時は `OVERFLOW` として通知。
- 送信：ITPSFrame配列をMark/Space dur列へ展開しRMTへ投入。キャリア周波数・デューティ比・反転（`invertOutput`）はRMT設定で吸収。

---

## 15. 互換性・拡張性ポリシー
- ITPSFrameの `flags` は予約ビットを持ち、将来拡張しても旧版が無視できること
- 新規プロトコルはCodec登録のみで追加可能とする
- Arduino APIは破壊的変更を避け、設定項目は追加方向で拡張する

---

## 16. サンプルと期待挙動

### 16.1 コード例（完全修飾）
受信
```cpp
esp32ir::Receiver rx(23);
rx.begin();
```

NEC 送信
```cpp
esp32ir::Transmitter tx(25);
tx.begin();
tx.sendNEC(0x00FF, 0xA2);
```

### 16.2 期待挙動（ユースケース）
- NEC受信（デフォルト設定）：デコード成功時は `status=DECODED` と論理データを返す。`useRawPlusKnown()` していれば raw も添付。デコード不能で RAW モードの場合は `status=RAW_ONLY` として ITPS を返す。
- AC学習（RAW重視プリセット）：長大データでも `maxFrameUs` 超過前に Space 境界で分割し、可能な限り RAW を返す。`frameCountMax` 超過で `OVERFLOW`。
- RAW再送信：受信で得た ITPSBuffer をそのまま `send(raw)` で送信可能。反転が必要な環境でも `invertOutput` で吸収し ITPS は変更しない。
