# ESP32IRPulseCodec 仕様

## 0. 前提
- ライブラリ名：**ESP32IRPulseCodec**
- namespace：`esp32ir`
- Arduino環境でESP32（RMT利用）のハードウェア支援を使い、安定したIR送受信を提供する
- 対象：ESP32（RMT利用）
- 中間フォーマット：ITPS（正規化済み、詳細は `SPEC_ITPS.ja.md`）
- 反転（INVERT）は送受信直前（HAL）で吸収
- 受信：ポーリングのみ
- 送信：ブロッキング送信
- 受信側の量子化（`T_us` 決定）とノイズ処理は前段で済ませ、ITPSには正規化済みデータを渡す
- 仕様記述はユーザーの使い方（API/期待挙動）を中心とし、実装詳細は最小限に留める
- `using namespace` は使わず、常に `esp32ir::` で完全修飾する（ネームスペースのネストは最小限）

## 1. 目的と非目的

### 1.1 目的
- Arduinoユーザーが ESP32 のハードウェア支援を活用して安定したIR送受信を手軽に使えるようにする
- ESP32のRMTを用いた、ジッタに強いIR送受信
- ITPSを入出力として、デコーダ/エンコーダの移植性を確保
- コーデックはITPSベースで移植性を保ちつつ、本ライブラリはESP32特化とし、必要な実装のみ（MIT等の既存実装も必要部分だけを取り込む前提）で構成する
- 既知プロトコルのデコードと、AC等の長大データを含むRAW取得の両立（モード切り替えで制御）
- 送受信での反転はHAL層で吸収し、ITPSは常に正のMark/負のSpaceで保持

### 1.2 非目的
- ESP32以外のHAL実装を本リポジトリで提供すること
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
Raw(RMT) → Split/Quantize → **ITPSFrame配列** → Decode（有効プロトコル） → 論理データ  
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

## 4. 受信モードと分割ポリシー

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
  - ITPS 化では SPEC_ITPS 準拠で正規化（Mark開始・`seq[i]` は0禁止かつ `1..127/-1..-127` の範囲、長区間は ±127 分割、不要分割は除去）し、反転は扱わない。
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
  - `esp32ir::Protocol`

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
bool addProtocol(esp32ir::Protocol id);
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
- 戻り値はポーリング成功/失敗のみ。シンプルな利用例（ネスト抑制）：
  ```cpp
  esp32ir::RxResult r;
  if (rx.poll(r)) {  // 取得があるときだけ分岐
    switch (r.status) {
      case esp32ir::RxStatus::DECODED:
        // r.logical を処理
        break;
      case esp32ir::RxStatus::RAW_ONLY:
      case esp32ir::RxStatus::OVERFLOW:
        // r.raw を処理（必要なら保存/再送）
        break;
    }
  }
  ```

---

## 8. RxResult
```cpp
struct RxResult {
  esp32ir::RxStatus status;
  esp32ir::Protocol protocol;
  esp32ir::LogicalPacket logical;
  esp32ir::ITPSBuffer raw;
};
```

- RAW_PLUS 時は DECODED でも raw を必ず含む
- RAW_ONLY 時は protocol/logical は未使用
- OVERFLOW 時も取得できた範囲の raw を返す
- NEC などのデコードヘルパは RxResult 受け取り版を用意し、`status==DECODED` かつ対象プロトコルのときだけ true を返す（例：`bool esp32ir::decodeNEC(const esp32ir::RxResult& in, esp32ir::NECDecoded& out);`）。
- デコード結果構造体を送信ヘルパでも受け取れるようにし、バラ引数版は構造体版に委譲して実装重複を避ける。

---

## 9. LogicalPacket
```cpp
struct LogicalPacket {
  esp32ir::Protocol protocol;
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

- 正規化済み ITPS（`SPEC_ITPS.ja.md` 準拠）を受け渡す前提：
  - `T_us` はフレーム配列全体で共通。0や欠落、不一致は無効。
  - `seq` は読み取り専用で `seq[0] > 0`、`seq[i] != 0`、`1 <= abs(seq[i]) <= 127`。長区間は ±127 分割し、127 未満同士の不要分割はマージ済み。
  - 量子化（`T_us` 決定）と微小ノイズ除去は ITPS 化前段で完了している。
  - flags は拡張用。反転の有無は ITPS に持たせず HAL で吸収し、時間計算（`totalTimeUs` 等）は 32bit 以上で扱う。

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
bool send(const esp32ir::ITPSBuffer& raw);
bool send(const esp32ir::LogicalPacket& p);
bool sendNEC(const esp32ir::NECDecoded& p);
bool sendNEC(uint16_t address, uint8_t command, bool repeat=false);
bool sendSONY(const esp32ir::SONYDecoded& p);
bool sendSONY(uint16_t address, uint16_t command, uint8_t bits=12);
```

---

## 12. 対応プロトコルとヘルパー
- 方針：プロトコルごとにデコード/送信ヘルパを用意し、構造体版＋バラ引数版を揃える。`addProtocol` を呼ばなければ既知プロトコル全対応＋RAW。初期実装は NEC/SONY/RAW のみで、その他はAPI予約（当初は未対応で false を返す想定）。
- プロトコル一覧とヘルパー設計
  - 初期サポート
    - NEC  
      - `struct esp32ir::NECDecoded { uint16_t address; uint8_t command; bool repeat; };`  
      - `bool esp32ir::decodeNEC(const esp32ir::RxResult& in, esp32ir::NECDecoded& out);`  
      - `bool sendNEC(const esp32ir::NECDecoded& p);` / `bool sendNEC(uint16_t address, uint8_t command, bool repeat=false);`（ギャップ40ms既定）
    - SONY（SIRC 12/15/20bit）  
      - `struct esp32ir::SONYDecoded { uint16_t address; uint16_t command; uint8_t bits; };`  
      - `bool esp32ir::decodeSONY(const esp32ir::RxResult& in, esp32ir::SONYDecoded& out);`（`bits`は12/15/20のみ）  
      - `bool sendSONY(const esp32ir::SONYDecoded& p);` / `bool sendSONY(uint16_t address, uint16_t command, uint8_t bits=12);`（bitsは12/15/20のみ）
    - RAW  
      - ITPSBuffer をそのまま扱う。`useRawOnly`/`useRawPlusKnown` で有効化。  
      - `send(const esp32ir::ITPSBuffer& raw);` で再送可能。
  - 追加候補（短〜中尺、APIのみ予約予定・当初は未実装で false）
    - AEHA(家電協)  
      - `struct esp32ir::AEHADecoded { uint16_t address; uint32_t data; uint8_t nbits; };`  
      - `bool decodeAEHA(const esp32ir::RxResult&, esp32ir::AEHADecoded&);`  
      - `bool sendAEHA(const esp32ir::AEHADecoded&);` / `bool sendAEHA(uint16_t address, uint32_t data, uint8_t nbits);`
    - Panasonic/Kaseikaden  
      - `struct esp32ir::PanasonicDecoded { uint16_t address; uint32_t data; uint8_t nbits; };`  
      - `bool decodePanasonic(const esp32ir::RxResult&, esp32ir::PanasonicDecoded&);`  
      - `bool sendPanasonic(const esp32ir::PanasonicDecoded&);` / `bool sendPanasonic(uint16_t address, uint32_t data, uint8_t nbits);`
    - JVC  
      - `struct esp32ir::JVCDecoded { uint16_t address; uint16_t command; };`  
      - `bool decodeJVC(const esp32ir::RxResult&, esp32ir::JVCDecoded&);`  
      - `bool sendJVC(const esp32ir::JVCDecoded&);` / `bool sendJVC(uint16_t address, uint16_t command);`
    - Samsung  
      - `struct esp32ir::SamsungDecoded { uint16_t address; uint16_t command; };`  
      - `bool decodeSamsung(const esp32ir::RxResult&, esp32ir::SamsungDecoded&);`  
      - `bool sendSamsung(const esp32ir::SamsungDecoded&);` / `bool sendSamsung(uint16_t address, uint16_t command);`
    - LG  
      - `struct esp32ir::LGDecoded { uint16_t address; uint16_t command; };`  
      - `bool decodeLG(const esp32ir::RxResult&, esp32ir::LGDecoded&);`  
      - `bool sendLG(const esp32ir::LGDecoded&);` / `bool sendLG(uint16_t address, uint16_t command);`
    - Denon/Sharp  
      - `struct esp32ir::DenonDecoded { uint16_t address; uint16_t command; bool repeat; };`  
      - `bool decodeDenon(const esp32ir::RxResult&, esp32ir::DenonDecoded&);`  
      - `bool sendDenon(const esp32ir::DenonDecoded&);` / `bool sendDenon(uint16_t address, uint16_t command, bool repeat=false);`
    - RC5  
      - `struct esp32ir::RC5Decoded { uint16_t command; bool toggle; };`  
      - `bool decodeRC5(const esp32ir::RxResult&, esp32ir::RC5Decoded&);`  
      - `bool sendRC5(const esp32ir::RC5Decoded&);` / バラ引数版
    - RC6  
      - `struct esp32ir::RC6Decoded { uint32_t command; uint8_t mode; bool toggle; };`  
      - `bool decodeRC6(const esp32ir::RxResult&, esp32ir::RC6Decoded&);`  
      - `bool sendRC6(const esp32ir::RC6Decoded&);` / バラ引数版
    - Apple(NEC拡張系)  
      - `struct esp32ir::AppleDecoded { uint16_t address; uint8_t command; };`  
      - `bool decodeApple(const esp32ir::RxResult&, esp32ir::AppleDecoded&);`  
      - `bool sendApple(const esp32ir::AppleDecoded&);` / `bool sendApple(uint16_t address, uint8_t command);`
    - Pioneer / Toshiba / Mitsubishi / Hitachi  
      - 各 `struct esp32ir::XxxDecoded { uint16_t address; uint16_t command; uint8_t extra; };`（必要フィールドはプロトコル仕様に合わせて決定）  
      - `bool decodeXxx(const esp32ir::RxResult&, esp32ir::XxxDecoded&);`  
      - `bool sendXxx(const esp32ir::XxxDecoded&);` / バラ引数版
  - 追加候補（長尺/AC系、まずはRAW取得。必要に応じ個別ヘルパ追加。APIは将来予約）
    - Daikin AC、Panasonic AC、Mitsubishi AC、Toshiba AC、Fujitsu AC など：`struct esp32ir::XxxACDecoded { ... }`、`decodeXxxAC(...)`、`sendXxxAC(.../構造体版)`
- `bool send(const esp32ir::LogicalPacket& p);` はヘルパ実装済みプロトコルのときのみ true、それ以外は false。

---

## 13. 共通ポリシー（エラー/時間/ログ/サンプル）
- 例外は使わず、公開APIの成否は `bool` で返す。失敗時は適切なログを出し、false を返して終了する（不正利用含む）。動作は継続し、アサート/abort は行わない。
- 送信APIの失敗理由はログで把握する前提とし、戻り値は成功/失敗のみ（詳細ステータスは持たない）。
- 時間待ちは Arduino の `delay()` を基準とし、内部 tick は 1ms 固定のみを想定。その他の高精度タイマや別tick周波数は考慮しない。
- ログは ESP-IDF の `ESP_LOGE/W/I/D/V` を Arduino 環境でも利用する。タグは共通で `ESP32IRPulseCodec` を使用し、必要に応じてメッセージ先頭に `RX`/`TX` などでクラスを示す。ログ初期化やレベル設定はボード定義側で行う前提。
- サンプルコードのコメントは英語/日本語併記とし、行頭は `// en: ...` の次行に `// ja: ...`、行末なら `// en: ... / ja: ...` とする。
- サンプルは機能別にシンプルな例を多数用意し、正しい使い方習得を優先する。読み順が分かるよう `01_...` のような連番付きプロジェクト名とする。

---

## 14. 送信ギャップ
- デフォルト 40ms
- RAW/Logical/NEC すべてに適用
- 最終 Space を最低 gapUs 確保

---

## 15. ESP32 HAL（RMT利用）
- 受信：RMTのMark/Space dur列を取得し、SPEC_ITPS準拠で量子化・正規化（Mark開始、±127分割、不要分割除去）してフレーム分割後にITPSへ変換。`invertInput` はRMT設定または受信後の解釈で吸収。バッファ不足時は `OVERFLOW` として通知。
- 送信：ITPSFrame配列をMark/Space dur列へ展開しRMTへ投入。キャリア周波数・デューティ比・反転（`invertOutput`）はRMT設定で吸収。

---

## 16. 互換性・拡張性ポリシー
- ITPSFrameの `flags` は予約ビットを持ち、将来拡張しても旧版が無視できること
- 新規プロトコルはCodec登録のみで追加可能とする
- Arduino APIは破壊的変更を避け、設定項目は追加方向で拡張する

---

## 17. サンプルと期待挙動

### 17.1 コード例（完全修飾）
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

NEC 受信（デコードヘルパ使用例）
```cpp
esp32ir::Receiver rx(23);
rx.addProtocol(esp32ir::Protocol::NEC);
rx.begin();

void loop() {
  esp32ir::RxResult r;
  if (rx.poll(r)) {
    esp32ir::NECDecoded nec;
    if (esp32ir::decodeNEC(r, nec)) {
      printf("addr=0x%04X cmd=0x%02X repeat=%s\n",
             nec.address, nec.command, nec.repeat ? "true" : "false");
    }
  }
  delay(1);
}
```

### 17.2 期待挙動（ユースケース）
- NEC受信（デフォルト設定）：デコード成功時は `status=DECODED` と論理データを返す。`useRawPlusKnown()` していれば raw も添付。デコード不能で RAW モードの場合は `status=RAW_ONLY` として ITPS を返す。
- AC学習（RAW重視プリセット）：長大データでも `maxFrameUs` 超過前に Space 境界で分割し、可能な限り RAW を返す。`frameCountMax` 超過で `OVERFLOW`。
- RAW再送信：受信で得た ITPSBuffer をそのまま `send(raw)` で送信可能。反転が必要な環境でも `invertOutput` で吸収し ITPS は変更しない。
