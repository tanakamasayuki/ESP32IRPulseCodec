# ESP32IRPulseCodec 仕様

## 0. 前提
- ライブラリ名：**ESP32IRPulseCodec**、namespace：`esp32ir`
- 対象：Arduino環境のESP32（RMT利用、Arduino-ESP32 v3以降のRMT変更に対応。v2系は対象外）
- ITPS（正規化済み中間フォーマット、詳細は `SPEC_ITPS.ja.md`）を入出力に使用
- 反転（INVERT）はHALで吸収し、ITPSは常に正のMark/負のSpace
- 受信：ポーリングのみ／送信：ブロッキング送信
- 量子化（`T_us` 決定）とノイズ処理は前段で済ませ、ITPSには正規化済みデータを渡す
- 仕様はユーザーの使い方（API/期待挙動）を中心に記述
- `using namespace` は使わず、常に `esp32ir::` で完全修飾する（ネストは最小限）

## 1. 目的と非目的

### 1.1 目的
- ArduinoユーザーがESP32のハードウェア支援（RMT）を活用し、安定したIR送受信を手軽に使えるようにする
- ITPS入出力でコーデック移植性を確保しつつ、ESP32特化で必要な実装だけを提供する（必要ならMIT等から必要部分のみ取り込み）
- 既知プロトコルのデコードと、AC等の長大RAW取得をモード切り替えで両立させる
- 反転はHAL層で吸収し、ITPSは常に正のMark/負のSpaceで保持

### 1.2 非目的
- ESP32以外のHALを本リポジトリで提供すること
- すべてのIRプロトコルの完全実装（まずは枠組みと主要プロトコル優先）
- 送受信波形の圧縮・暗号化等の付加機能
- RTOSタスク管理の自由度提供（内部で必要最低限のみ使用）

## 2. 用語
- **Mark / Space**：搬送波ON / OFF 区間（IR LED点灯/消灯）
- **Frame**：Mark/Spaceの区間列（ITPSFrame）。ギャップを含めるかは分割ポリシー依存。
- **ITPS**：正規化済み中間フォーマット（`SPEC_ITPS.ja.md`）
- **ITPSFrame / ITPSBuffer**：ITPSのフレーム／フレーム配列
- **ProtocolMessage**：プロトコル共通の論理メッセージ（`Protocol` + 生データバイト列）
- **Protocol**：`esp32ir::Protocol`（NEC/SONY等）
- **ProtocolPayload構造体**：`esp32ir::payload::<Protocol>`（デコード結果/送信パラメータ）
- **HAL**：ESP32固有層（RMT、GPIO、キャリア、反転）
- **Codec**：プロトコル固有のデコード/エンコード処理
- **AC**：エアコン系プロトコル（DaikinAC 等の長大ペイロード）
- **TBD**：未定義（仕様/実装がこれからの項目）

## 3. 全体アーキテクチャ

### 3.1 データフロー（受信）
Raw(RMT) → Split/Quantize → **ITPSBuffer（ITPSFrame配列）** → Decode（有効プロトコル） → ProtocolMessage（論理メッセージ） → Payload（`esp32ir::payload::<Protocol>`、デコードヘルパ使用）  
- `useRawOnly()`：デコードせず ITPS を RAW として返す。  
- `useRawPlusKnown()`：デコードを試み、成功しても ITPS を添付する。  
- `clearProtocols()`：プロトコル設定をクリア。`addProtocol()` を1回も呼ばなければ既知プロトコル全対応＋RAW。  
- `addProtocol()`：受信対象プロトコルを限定（指定があれば ONLY）。RAW系指定があれば RAW を優先。  
- ProtocolMessage はプロトコル共通コンテナで、通常はデコードヘルパ（例：`decodeNEC`）で `esp32ir::payload::<Protocol>` に変換して使う。

### 3.2 データフロー（送信）
Payload（`esp32ir::payload::<Protocol>`） → 送信ヘルパ → ProtocolMessage → Encode → **ITPSBuffer（ITPSFrame配列）** → HAL(RMT)送信  
反転はHALで吸収する（ITPSは変更しない）。Payloadを渡さず、自前で組んだProtocolMessageを直接送ってもよい。

### 3.3 学習→再送のフロー（RAW系）
受信Raw：Raw(RMT) → ITPSBuffer（RAW Only/RAW Plus） → 保存  
再送：保存した ITPSBuffer → `send(raw)` → HAL(RMT)送信  
AC学習や未知プロトコルの再送に利用する。反転が必要なら送信側の `invertOutput` で吸収。

### 3.4 プリセット
- **ALL_KNOWN（デフォルト）**：既知プロトコルすべて（AC含む）をデコード対象にする。
- **KNOWN_WITHOUT_AC**：`useKnownWithoutAC()` で AC を除外した既知プロトコルのみを対象にする。
- **RAW_ONLY / RAW_PLUS_KNOWN**：RAW取得重視のプリセット（前述のモード参照）。

### 3.5 payload と ProtocolMessage の使い分け
- 基本はプロトコル別の `esp32ir::payload::<Protocol>` とヘルパ（`decodeX` / `sendX`）を使う。
- `ProtocolMessage` を直接組み立てるのは、独自プロトコルやテストなど低レベル用途のみを想定する。

### 3.6 ファイル配置（実装ガイドライン）
- 翻訳単位は小さく分割する：
  - `src/core/`：ITPSBuffer, ProtocolMessage, 共通ユーティリティ
  - `src/hal/`：RMT/GPIO、キャリア、反転処理
  - `src/protocols/`：プロトコル別ヘルパ（例 `nec.cpp`, `sony.cpp`, `aeha.cpp`、AC系も別ファイル）
  - `src/receiver.cpp` / `src/transmitter.cpp`：クラス本体
- パブリックの傘ヘッダ `ESP32IRPulseCodec.h` から必要なプロトコル/ヘルパ宣言を提供する。

### 3.7 レイヤ構造
1) **Core（機種非依存）**  
   - ITPSFrame/Normalize、ProtocolCodecインタフェース  
   - ProtocolMessage / payload::<Protocol> の変換ヘルパ（decode/send系）
2) **ESP32 HAL**  
   - RMT受信/送信、GPIO/キャリア設定、反転、バッファ管理  
   - Arduino-ESP32 v3以降のRMT APIに対応
3) **Arduino API**  
   - `esp32ir::Receiver` / `esp32ir::Transmitter`  
   - モード設定、プロトコル選択、ポーリング/ブロッキング送信など

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
- パラメータの決め方
  - 各プロトコルは推奨値（例：`frameGapUs/hardGapUs/minFrameUs/maxFrameUs/minEdges/splitPolicy`）を持ち、受信開始時に有効プロトコルから自動マージしてデフォルトを生成する。
  - マージ例（推奨）：
    - 長さ上限系（`frameGapUs` / `hardGapUs` / `maxFrameUs`）：有効プロトコル中の最大値（最もゆるい上限）を採用し、上限不足で切れないようにする。
    - ノイズ除去系（`minFrameUs` / `minEdges`）：有効プロトコル中の最大値（最も厳しい下限）を採用し、ノイズ誤検出を避ける。
    - `splitPolicy`：RAWモードでは `KEEP_GAP_IN_FRAME`、KNOWN系では `DROP_GAP` を優先。ユーザー設定があればそれを上書き。
  - `useRawOnly`/`useRawPlusKnown` のときはRAW向けプリセット（例：ギャップ広め、`KEEP_GAP_IN_FRAME`、`frameCountMax` など）を優先し、プロトコル推奨値は無視してもよい。

---

## 5. 命名
- namespace：`esp32ir`
- クラス
  - `esp32ir::Receiver`
  - `esp32ir::Transmitter`
- データ型
  - `esp32ir::RxResult`
  - `esp32ir::ProtocolMessage`
  - `esp32ir::ITPSFrame`
  - `esp32ir::ITPSBuffer`
  - `esp32ir::Protocol`
- プロトコル別Payload構造体
  - `esp32ir::payload::<Protocol>`（例：NEC, SONY, AEHA, ...。詳細は「対応プロトコルとヘルパー」）

---

## 6. Receiver（受信）

### 6.1 コンストラクタ
```cpp
esp32ir::Receiver();
esp32ir::Receiver(int rxPin, bool invert=false, uint16_t T_us_rx=5);
```
- デフォルト値（想定）：引数なしは後で `setPin` 等で設定する前提。引数ありはピン以外はデフォルト（`invert=false`, `T_us_rx=5us`）を指定可能。

### 6.2 セッター（begin前のみ有効）
```cpp
bool setPin(int rxPin);
bool setInvertInput(bool invert);
bool setQuantizeT(uint16_t T_us_rx);
```
- デフォルト値（想定）：`invert=false`、`T_us_rx=5`us

### 6.3 begin/end
```cpp
bool begin();
void end();
```
- begin で RMT 受信などを初期化し、バックグラウンドで受信を開始する。受信したデータは内部キューに蓄積され、`poll` で取得する。
- end でバックグラウンド受信を停止し、RMT等のリソースを解放する。

### 6.4 プロトコル指定（begin前のみ）
```cpp
bool addProtocol(esp32ir::Protocol protocol);
bool clearProtocols();
bool useRawOnly();
bool useRawPlusKnown();
bool useKnownWithoutAC();  // AC系を除外した既知プロトコルプリセット
```

- プロトコル未指定 → ALL_KNOWN
- 指定あり → ONLY
- RAW系指定が最優先
- ALL_KNOWN には AC 系（DaikinAC 等）も含まれる。ACを除外したい場合は `useKnownWithoutAC()` または `addProtocol` で限定する。
- プロトコル推奨パラメータを begin 時にマージして受信デフォルトを決定（詳細は「受信モードと分割ポリシー」）

---

## 7. poll と所有権
```cpp
bool poll(esp32ir::RxResult& out);
```

- RxResult は呼び出し側が所有し、次回 poll でも破壊されない。内部はFIFOキュー（上限あり）で管理し、溢れた場合は古いデータを破棄して OVERFLOW を通知する。ノイズ判定で破棄した場合は RxResult を発行しない。
- 戻り値は「取得できたかどうか」の bool のみ。シンプルな利用例：
  ```cpp
  esp32ir::RxResult rxResult;
  if (rx.poll(rxResult)) {  // 取得があるときだけ分岐
    switch (rxResult.status) {
      case esp32ir::RxStatus::DECODED:
        // rxResult.message を処理
        break;
      case esp32ir::RxStatus::RAW_ONLY:
      case esp32ir::RxStatus::OVERFLOW:
        // rxResult.raw を処理（必要なら保存/再送）
        break;
    }
  }
  ```

---

## 8. RxResult
受信結果を格納する構造体。`poll` で取得し、ProtocolMessage（論理メッセージ）とITPS RAWの両方を保持できる。
```cpp
struct RxResult {
  esp32ir::RxStatus status;
  esp32ir::Protocol protocol;
  esp32ir::ProtocolMessage message;
  esp32ir::ITPSBuffer raw;
};
```

- RAW_PLUS 時は DECODED でも raw を必ず含む
- RAW_ONLY 時は protocol/message は未使用
- OVERFLOW 時も取得できた範囲の raw を返す
- NEC などのデコードヘルパは RxResult 受け取り版を用意し、`status==DECODED` かつ対象プロトコルのときだけ true を返す（例：`bool esp32ir::decodeNEC(const esp32ir::RxResult& in, esp32ir::payload::NEC& out);`）。
- デコード結果構造体を送信ヘルパでも受け取れるようにし、バラ引数版は構造体版に委譲して実装重複を避ける。

---

## 9. ProtocolMessage
プロトコル共通の論理メッセージ（`Protocol` + 生データ）。通常はプロトコル別ヘルパが生成し、直接組み立てるのはカスタム/テストなどの低レベル用途。
```cpp
struct ProtocolMessage {
  esp32ir::Protocol protocol;
  const uint8_t* data;
  uint16_t length;
  uint32_t flags;
};
```

- プロトコル固有のPayload構造体は `esp32ir::payload::<Protocol>` に用意する（例：`payload::NEC`）。受信時はデコードヘルパ（`decodeNEC` 等）で `ProtocolMessage` から `payload::<Protocol>` へ変換し、送信時は送信ヘルパ（`sendNEC` 等）が `payload::<Protocol>` から `ProtocolMessage`（→ITPS）を組み立てる。

---

## 10. ITPS

ITPS は Mark/Space の正規化済み中間フォーマット。詳細は `SPEC_ITPS.ja.md` を参照。

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
  - flags は拡張用（現状は未使用）。反転の有無は ITPS に持たせず HAL で吸収し、時間計算（`totalTimeUs` 等）は 32bit 以上で扱う。

---

## 11. Transmitter（送信）

### 11.1 コンストラクタ
```cpp
esp32ir::Transmitter();
esp32ir::Transmitter(int txPin, bool invert=false, uint32_t hz=38000);
```
- デフォルト値（想定）：引数なしは後で `setPin` 等で設定する前提。引数ありはピン以外はデフォルト（`invert=false`, `hz=38000`Hz, duty=約50%, gapUs=40000us）を指定可能。

### 11.2 セッター（begin前のみ）
```cpp
bool setPin(int txPin);
bool setInvertOutput(bool invert);
bool setCarrierHz(uint32_t hz);
bool setDutyPercent(uint8_t dutyPercent);
bool setGapUs(uint32_t gapUs);
```
- デフォルト値（想定）：`invert=false`、`hz=38000`Hz、`dutyPercent` は一般的な50%近辺、`gapUs=40000`us（送信ギャップ既定）。プロトコル別ヘルパは推奨ギャップを持つ場合があり、`setGapUs` で上書きされていなければそれを優先する。

### 11.3 begin/end
```cpp
bool begin();
void end();
```
- begin で送信に必要なRMT等を初期化し、end で解放する（ブロッキング送信なので begin/end はリソース管理が主目的）。


### 11.4 send（ブロッキング）
```cpp
bool send(const esp32ir::ITPSBuffer& itps);
bool send(const esp32ir::ProtocolMessage& message);
```
- プロトコル別の送信ヘルパ（`esp32ir::sendNEC` 等）は「対応プロトコルとヘルパー」を参照。`gapUs` はユーザー設定があればそれを、なければヘルパが持つ推奨値（なければ既定40ms）を適用する。

---

## 12. 対応プロトコルとヘルパー
- 方針：プロトコルごとにデコード/送信ヘルパを用意し、基本は構造体版＋バラ引数版を揃える（AC系は構造体版のみ）。`addProtocol` を呼ばなければ既知プロトコル全対応＋RAW。
- 対応状況（○=ヘルパ実装、△=枠のみ/予定、RAWはITPS直扱い）
  - AC 系は任意データの生成はTBDだが、RAWモードでの学習と受信RAWの再送（構造体版ヘルパ or `send(raw)`）は可能とする想定。

| プロトコル                 | フレーム構造体                    | デコードヘルパ                   | 送信ヘルパ                                  | 状態 |
|---------------------------|----------------------------------|---------------------------------|--------------------------------------------|------|
| RAW (ITPS)                | なし（ITPSBuffer直接）            | -                               | `send(ITPSBuffer)`                         | ○    |
| NEC                       | `esp32ir::payload::NEC`          | `esp32ir::decodeNEC`            | `esp32ir::sendNEC(struct/args)`            | ○    |
| SONY (SIRC 12/15/20)      | `esp32ir::payload::SONY`         | `esp32ir::decodeSONY`           | `esp32ir::sendSONY(struct/args)`           | ○    |
| AEHA (家電協)             | `esp32ir::payload::AEHA`         | `esp32ir::decodeAEHA`           | `esp32ir::sendAEHA(struct/args)`           | △   |
| Panasonic/Kaseikaden      | `esp32ir::payload::Panasonic`    | `esp32ir::decodePanasonic`      | `esp32ir::sendPanasonic(struct/args)`      | △   |
| JVC                       | `esp32ir::payload::JVC`          | `esp32ir::decodeJVC`            | `esp32ir::sendJVC(struct/args)`            | △   |
| Samsung                   | `esp32ir::payload::Samsung`      | `esp32ir::decodeSamsung`        | `esp32ir::sendSamsung(struct/args)`        | △   |
| LG                        | `esp32ir::payload::LG`           | `esp32ir::decodeLG`             | `esp32ir::sendLG(struct/args)`             | △   |
| Denon/Sharp               | `esp32ir::payload::Denon`        | `esp32ir::decodeDenon`          | `esp32ir::sendDenon(struct/args)`          | △   |
| RC5                       | `esp32ir::payload::RC5`          | `esp32ir::decodeRC5`            | `esp32ir::sendRC5(struct/args)`            | △   |
| RC6                       | `esp32ir::payload::RC6`          | `esp32ir::decodeRC6`            | `esp32ir::sendRC6(struct/args)`            | △   |
| Apple (NEC拡張系)         | `esp32ir::payload::Apple`        | `esp32ir::decodeApple`          | `esp32ir::sendApple(struct/args)`          | △   |
| Pioneer                   | `esp32ir::payload::Pioneer`      | `esp32ir::decodePioneer`        | `esp32ir::sendPioneer(struct/args)`        | △   |
| Toshiba                   | `esp32ir::payload::Toshiba`      | `esp32ir::decodeToshiba`        | `esp32ir::sendToshiba(struct/args)`        | △   |
| Mitsubishi                | `esp32ir::payload::Mitsubishi`   | `esp32ir::decodeMitsubishi`     | `esp32ir::sendMitsubishi(struct/args)`     | △   |
| Hitachi                   | `esp32ir::payload::Hitachi`      | `esp32ir::decodeHitachi`        | `esp32ir::sendHitachi(struct/args)`        | △   |
| Daikin AC                 | `esp32ir::payload::DaikinAC`     | `esp32ir::decodeDaikinAC`       | `esp32ir::sendDaikinAC(struct)`            | △   |
| Panasonic AC              | `esp32ir::payload::PanasonicAC`  | `esp32ir::decodePanasonicAC`    | `esp32ir::sendPanasonicAC(struct)`         | △   |
| Mitsubishi AC             | `esp32ir::payload::MitsubishiAC` | `esp32ir::decodeMitsubishiAC`   | `esp32ir::sendMitsubishiAC(struct)`        | △   |
| Toshiba AC                | `esp32ir::payload::ToshibaAC`    | `esp32ir::decodeToshibaAC`      | `esp32ir::sendToshibaAC(struct)`           | △   |
| Fujitsu AC                | `esp32ir::payload::FujitsuAC`    | `esp32ir::decodeFujitsuAC`      | `esp32ir::sendFujitsuAC(struct)`           | △   |

- 構造体とヘルパー詳細
  - RAW  
    - ITPSBuffer をそのまま扱う。`useRawOnly`/`useRawPlusKnown` で有効化。  
    - `send(const esp32ir::ITPSBuffer& raw);` で再送可能。
  - NEC  
    - `struct esp32ir::payload::NEC { uint16_t address; uint8_t command; bool repeat; };`  
    - `bool esp32ir::decodeNEC(const esp32ir::RxResult& in, esp32ir::payload::NEC& out);`  
    - `bool esp32ir::sendNEC(const esp32ir::payload::NEC& p);` / `bool esp32ir::sendNEC(uint16_t address, uint8_t command, bool repeat=false);`
  - SONY（SIRC 12/15/20bit）  
    - `struct esp32ir::payload::SONY { uint16_t address; uint16_t command; uint8_t bits; };`  
    - `bool esp32ir::decodeSONY(const esp32ir::RxResult& in, esp32ir::payload::SONY& out);`（`bits`は12/15/20のみ）  
    - `bool esp32ir::sendSONY(const esp32ir::payload::SONY& p);` / `bool esp32ir::sendSONY(uint16_t address, uint16_t command, uint8_t bits=12);`（bitsは12/15/20のみ）
  - AEHA(家電協)  
    - `struct esp32ir::payload::AEHA { uint16_t address; uint32_t data; uint8_t nbits; };`  
    - `bool esp32ir::decodeAEHA(const esp32ir::RxResult&, esp32ir::payload::AEHA&);`  
    - `bool esp32ir::sendAEHA(const esp32ir::payload::AEHA&);` / `bool esp32ir::sendAEHA(uint16_t address, uint32_t data, uint8_t nbits);`
  - Panasonic/Kaseikaden  
    - `struct esp32ir::payload::Panasonic { uint16_t address; uint32_t data; uint8_t nbits; };`  
    - `bool esp32ir::decodePanasonic(const esp32ir::RxResult&, esp32ir::payload::Panasonic&);`  
    - `bool esp32ir::sendPanasonic(const esp32ir::payload::Panasonic&);` / `bool esp32ir::sendPanasonic(uint16_t address, uint32_t data, uint8_t nbits);`
  - JVC  
    - `struct esp32ir::payload::JVC { uint16_t address; uint16_t command; };`  
    - `bool esp32ir::decodeJVC(const esp32ir::RxResult&, esp32ir::payload::JVC&);`  
    - `bool esp32ir::sendJVC(const esp32ir::payload::JVC&);` / `bool esp32ir::sendJVC(uint16_t address, uint16_t command);`
  - Samsung  
    - `struct esp32ir::payload::Samsung { uint16_t address; uint16_t command; };`  
    - `bool esp32ir::decodeSamsung(const esp32ir::RxResult&, esp32ir::payload::Samsung&);`  
    - `bool esp32ir::sendSamsung(const esp32ir::payload::Samsung&);` / `bool esp32ir::sendSamsung(uint16_t address, uint16_t command);`
  - LG  
    - `struct esp32ir::payload::LG { uint16_t address; uint16_t command; };`  
    - `bool esp32ir::decodeLG(const esp32ir::RxResult&, esp32ir::payload::LG&);`  
    - `bool esp32ir::sendLG(const esp32ir::payload::LG&);` / `bool esp32ir::sendLG(uint16_t address, uint16_t command);`
  - Denon/Sharp  
    - `struct esp32ir::payload::Denon { uint16_t address; uint16_t command; bool repeat; };`  
    - `bool esp32ir::decodeDenon(const esp32ir::RxResult&, esp32ir::payload::Denon&);`  
    - `bool esp32ir::sendDenon(const esp32ir::payload::Denon&);` / `bool esp32ir::sendDenon(uint16_t address, uint16_t command, bool repeat=false);`
  - RC5  
    - `struct esp32ir::payload::RC5 { uint16_t command; bool toggle; };`  
    - `bool esp32ir::decodeRC5(const esp32ir::RxResult&, esp32ir::payload::RC5&);`  
    - `bool esp32ir::sendRC5(const esp32ir::payload::RC5&);` / `bool esp32ir::sendRC5(uint16_t command, bool toggle);`
  - RC6  
    - `struct esp32ir::payload::RC6 { uint32_t command; uint8_t mode; bool toggle; };`  
    - `bool esp32ir::decodeRC6(const esp32ir::RxResult&, esp32ir::payload::RC6&);`  
    - `bool esp32ir::sendRC6(const esp32ir::payload::RC6&);` / `bool esp32ir::sendRC6(uint32_t command, uint8_t mode, bool toggle);`
  - Apple(NEC拡張系)  
    - `struct esp32ir::payload::Apple { uint16_t address; uint8_t command; };`  
    - `bool esp32ir::decodeApple(const esp32ir::RxResult&, esp32ir::payload::Apple&);`  
    - `bool esp32ir::sendApple(const esp32ir::payload::Apple&);` / `bool esp32ir::sendApple(uint16_t address, uint8_t command);`
  - Pioneer  
    - `struct esp32ir::payload::Pioneer { uint16_t address; uint16_t command; uint8_t extra; };`  
    - `bool esp32ir::decodePioneer(const esp32ir::RxResult&, esp32ir::payload::Pioneer&);`  
    - `bool esp32ir::sendPioneer(const esp32ir::payload::Pioneer&);` / `bool esp32ir::sendPioneer(uint16_t address, uint16_t command, uint8_t extra=0);`
  - Toshiba  
    - `struct esp32ir::payload::Toshiba { uint16_t address; uint16_t command; uint8_t extra; };`  
    - `bool esp32ir::decodeToshiba(const esp32ir::RxResult&, esp32ir::payload::Toshiba&);`  
    - `bool esp32ir::sendToshiba(const esp32ir::payload::Toshiba&);` / `bool esp32ir::sendToshiba(uint16_t address, uint16_t command, uint8_t extra=0);`
  - Mitsubishi  
    - `struct esp32ir::payload::Mitsubishi { uint16_t address; uint16_t command; uint8_t extra; };`  
    - `bool esp32ir::decodeMitsubishi(const esp32ir::RxResult&, esp32ir::payload::Mitsubishi&);`  
    - `bool esp32ir::sendMitsubishi(const esp32ir::payload::Mitsubishi&);` / `bool esp32ir::sendMitsubishi(uint16_t address, uint16_t command, uint8_t extra=0);`
  - Hitachi  
    - `struct esp32ir::payload::Hitachi { uint16_t address; uint16_t command; uint8_t extra; };`  
    - `bool esp32ir::decodeHitachi(const esp32ir::RxResult&, esp32ir::payload::Hitachi&);`  
    - `bool esp32ir::sendHitachi(const esp32ir::payload::Hitachi&);` / `bool esp32ir::sendHitachi(uint16_t address, uint16_t command, uint8_t extra=0);`
  - Daikin AC  
    - 構造体名のみ予約：`esp32ir::payload::DaikinAC`（中身TBD）。関数名のみ予約：`bool esp32ir::decodeDaikinAC(const esp32ir::RxResult&, esp32ir::payload::DaikinAC&);` / `bool esp32ir::sendDaikinAC(const esp32ir::payload::DaikinAC&);`
  - Panasonic AC  
    - 構造体名のみ予約：`esp32ir::payload::PanasonicAC`（中身TBD）。関数名のみ予約：`bool esp32ir::decodePanasonicAC(const esp32ir::RxResult&, esp32ir::payload::PanasonicAC&);` / `bool esp32ir::sendPanasonicAC(const esp32ir::payload::PanasonicAC&);`
  - Mitsubishi AC  
    - 構造体名のみ予約：`esp32ir::payload::MitsubishiAC`（中身TBD）。関数名のみ予約：`bool esp32ir::decodeMitsubishiAC(const esp32ir::RxResult&, esp32ir::payload::MitsubishiAC&);` / `bool esp32ir::sendMitsubishiAC(const esp32ir::payload::MitsubishiAC&);`
  - Toshiba AC  
    - 構造体名のみ予約：`esp32ir::payload::ToshibaAC`（中身TBD）。関数名のみ予約：`bool esp32ir::decodeToshibaAC(const esp32ir::RxResult&, esp32ir::payload::ToshibaAC&);` / `bool esp32ir::sendToshibaAC(const esp32ir::payload::ToshibaAC&);`
  - Fujitsu AC  
    - 構造体名のみ予約：`esp32ir::payload::FujitsuAC`（中身TBD）。関数名のみ予約：`bool esp32ir::decodeFujitsuAC(const esp32ir::RxResult&, esp32ir::payload::FujitsuAC&);` / `bool esp32ir::sendFujitsuAC(const esp32ir::payload::FujitsuAC&);`

## 13. 共通ポリシー（エラー/時間/ログ/サンプル）
- **エラーハンドリング**：例外は使わず公開APIは `bool` で成否のみ返す。失敗時は適切なログを出し false を返して処理を終える（不正利用も同様）。動作は継続し、アサート/abort は行わない。送信APIも詳細ステータスは持たない。
- **時間/スケジューリング**：Arduino の `delay()` を基準とし、内部 tick は 1ms 固定のみを想定。高精度タイマや他周波数の tick は考慮しない。
- **ロギング**：ESP-IDF の `ESP_LOGE/W/I/D/V` を Arduino 環境でも利用可能とし、タグは共通で `ESP32IRPulseCodec`。クラス識別が必要ならメッセージ先頭に `RX`/`TX` などを付与。ログレベル目安は E=致命/操作失敗、W=リトライやタイムアウト、I=初期化や設定値、D/V=デバッグ詳細。ログ初期化やレベル設定はArduino IDEの Core Debug Level（ボード設定）で行う想定。
- **サンプル**：コメントは英語/日本語併記（行頭分離 `// en: ...` → `// ja: ...`、行末なら `// en: ... / ja: ...`）。機能別にシンプルな例を数多く用意し、読み順が分かるよう `01_...` など連番付きプロジェクト名とする。

---

## 14. 送信ギャップ
- 基本値は 40ms（`gapUs`）だが、プロトコル別送信ヘルパは各プロトコルの推奨ギャップを優先する（ユーザーが `setGapUs` で上書きした場合はその値を使う）。
- フレーム末尾の Space を最低 `gapUs` 確保してから送信完了とみなす。
- RAW/ProtocolMessage を直接 `send` する場合は、現在設定されている `gapUs` を適用する。

---

## 15. ESP32 HAL（RMT利用）
- **受信**：RMTのMark/Space dur列を取得し、SPEC_ITPS準拠で量子化・正規化（Mark開始、±127分割、不要分割除去）してフレーム分割後にITPSへ変換。`invertInput` はRMT設定または受信後の符号解釈で吸収。バッファ不足時は `OVERFLOW` として通知。
- **送信**：ITPSBuffer（ITPSFrame配列）をMark/Space dur列へ展開しRMTへ投入。キャリア周波数・デューティ比・反転（`invertOutput`）はRMT設定で吸収し、ITPS自体は変更しない。

---

## 16. 互換性・拡張性ポリシー
- ITPSFrame の `flags` は予約ビットとして拡張を許容し、旧版が無視できる互換性を維持する。
- 新規プロトコルは Codec の追加登録（decode/sendヘルパ＋推奨パラメータ）で拡張できる枠組みとする。
- Arduino API は破壊的変更を避け、設定項目を追加する方向で拡張する（既存シグネチャは維持）。

---

## 17. サンプルと期待挙動

### 17.1 コード例（完全修飾）

NEC 送信（最小スケッチ）
```cpp
#include <ESP32IRPulseCodec.h>

esp32ir::Transmitter tx(25);

void setup() {
  tx.begin();
  // 例: アドレス0x00FF, コマンド0xA2 を1回送信
  tx.sendNEC(0x00FF, 0xA2);
}

void loop() {
  // 送信のみ。必要ならdelay等で間隔調整。
  delay(1);
}
```

NEC 受信（デコードヘルパ使用例）
```cpp
#include <ESP32IRPulseCodec.h>

esp32ir::Receiver rx(23);

void setup() {
  rx.addProtocol(esp32ir::Protocol::NEC);  // NECのみを対象にする例
  rx.begin();
}

void loop() {
  esp32ir::RxResult rxResult;
  if (rx.poll(rxResult)) {
    esp32ir::payload::NEC nec;
    if (esp32ir::decodeNEC(rxResult, nec)) {
      printf("addr=0x%04X cmd=0x%02X repeat=%s\n",
             nec.address, nec.command, nec.repeat ? "true" : "false");
    }
  }
  delay(1);
}
```

### 17.2 期待挙動（ユースケース）
- **デフォルト（プロトコル無指定/KNOWN_ONLY）**：既知プロトコルを自動でデコードし、成功時は `status=DECODED` のみを返す。RAW は返さない（RAWが必要なら `useRawPlusKnown()` / `useRawOnly()` を明示）。
- **NECデコード**：デフォルト設定で `status=DECODED` の ProtocolMessage を返し、`decodeNEC` が true を返す。`useRawPlusKnown()` 指定時は raw も添付。
- **RAW優先の学習**：`useRawOnly()` では常に `status=RAW_ONLY` で ITPS を返す（デコードは試行しない）。`useRawPlusKnown()` ではデコード成功でも RAW を保持。
- **長大ACキャプチャ**：AC系でフレームが長い場合、`maxFrameUs` 超過前に Space 境界で分割して可能な限り RAW を返す。`frameCountMax` 超過時は `status=OVERFLOW` とし、取得できた範囲の RAW を渡す。
- **プロトコル限定受信**：`addProtocol(NEC)` のみ指定すれば NEC 以外はデコードしない（RAW指定があれば RAW のみ優先）。`useKnownWithoutAC()` なら AC を除いた既知プロトコルのみを対象にする。指定なしは既知プロトコルすべてを KNOWN_ONLY でデコードする（RAWは明示が必要）。
- **キュー動作**：受信キューが空なら `poll` は false。溢れた場合は古いデータを破棄し、次回取得で `status=OVERFLOW` を通知。
- **RAWの再送**：受信で得た ITPSBuffer は `send(raw)` でそのまま再送できる。反転が必要な環境は `invertOutput` で吸収し、ITPSはそのまま扱う。
