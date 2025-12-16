# ESP32向け Arduino IRライブラリ仕様（ITPS前提）

本仕様は **ITPS（Intermediate Timed Pulse Sequence）** を中核データ形式として、ESP32上で **RMTを利用した安定したIR送受信** を提供するArduinoライブラリの仕様を定義する。  
デコーダ/エンコーダは機種非依存に保ち、ESP32固有の差異（RMT、GPIO、キャリア生成、反転など）はHAL層に閉じ込める。

> 参照：`SPEC_ITPS.ja.md`（中間フォーマット仕様）

---

## 1. 目的と非目的

### 1.1 目的
- ESP32の **RMT** を用いた、ジッタに強いIR送受信
- **ITPSを入出力**として、デコーダ/エンコーダの移植性を確保
- **2モード受信**
  - KNOWN_ONLY：既知プロトコルのデコード成功率を優先
  - CAPTURE_ALL：AC等の長大データ含めRAW（ITPS）を確実に取得
- 送受信で **反転（INVERT）** をI/O直前で吸収し、ITPSは常に正しい表現を維持

### 1.2 非目的
- すべてのIRプロトコルの完全実装（まずは枠組み）
- 送受信波形の圧縮・暗号化等の付加機能
- RTOSタスク管理の自由度提供（内部で必要最低限のみ使用）

---

## 2. 用語
- **Mark**：搬送波（キャリア）ON（IR LED点灯）
- **Space**：搬送波OFF（IR LED消灯）
- **Frame**：Mark/Spaceの区間列（ITPSFrame）
- **HAL**：ESP32固有層（RMT、GPIO、キャリア、反転）
- **Codec**：プロトコル固有のデコード/エンコード（NECなど）

---

## 3. 全体アーキテクチャ

### 3.1 データフロー（受信）
Raw(RMT) → Split/Quantize → **ITPSFrame配列** →（KNOWN_ONLYなら）Decode → 論理データ  
CAPTURE_ALLでは Decode失敗でも **RAW取得成功**としてITPSFrame配列を返す。

### 3.2 データフロー（送信）
論理データ → Encode → **ITPSFrame配列** → HAL(RMT)送信  
反転はHALで吸収する（ITPSは変更しない）。

### 3.3 レイヤ構造
1) **Core（機種非依存）**
- ITPSFrame, ITPSNormalize, SplitHint合成
- ProtocolCodecインタフェース

2) **ESP32 HAL**
- RMT受信/送信ドライバ
- GPIO/キャリア設定
- 反転（invertInput/invertOutput）
- バッファ管理

3) **Arduino API**
- ユーザが扱うクラス（IRReceiver/IRTransmitter/IRDevice）

---

## 4. 公開API（Arduino向け）

### 4.1 主要クラス
- `IRDevice`：送受信をまとめたファサード（任意）
- `IRReceiver`：受信専用
- `IRTransmitter`：送信専用
- `ProtocolRegistry`：有効プロトコル集合の登録・列挙
- `ITPSBuffer`：RAW ITPSFrame配列の保持（参照カウント or ムーブ前提）

### 4.2 受信API（仕様）
#### 4.2.1 受信開始/停止
- `begin(const RxConfig&) -> bool`
- `end()`

#### 4.2.2 受信取得
- `poll(RxResult& out) -> bool`
  - 新規データがあればtrue
  - outに結果を格納
- もしくはコールバック方式：
  - `onReceive(void (*cb)(const RxResult&))`

> ポーリングとコールバックは排他ではなく、どちらかを提供すればよい（初期はポーリング推奨）。

### 4.3 送信API（仕様）
- `begin(const TxConfig&) -> bool`
- `send(const ITPSBuffer& frames, const TxPlan* plan=nullptr) -> TxStatus`
- `send(const LogicalPacket& packet) -> TxStatus`（プロトコル指定時）

---

## 5. 設定構造体

### 5.1 RxConfig
- `gpio_num_t rxPin`
- `uint16_t T_us_rx`（既定=5）
- `ReceiveMode mode`：`KNOWN_ONLY` / `CAPTURE_ALL`
- `ProtocolMask enabledProtocols`（KNOWN_ONLYで使用）
- `SplitConfig split`（未指定なら mode/プロトコル合成で決定）
- `bool invertInput`（HALで吸収）
- `bool keepRawOnDecoded`（DECODED時もITPSを添付するか）

### 5.2 TxConfig
- `gpio_num_t txPin`
- `uint32_t carrierHz`（既定=38000）
- `uint8_t dutyPercent`（既定=33）
- `bool invertOutput`（HALで吸収）
- `uint16_t rmtClockDiv`（実装依存、仕様上は任意設定扱い）

### 5.3 SplitConfig（前加工設定）
- `uint32_t frameGapUs`
- `uint32_t hardGapUs`
- `uint32_t minFrameUs`
- `uint32_t maxFrameUs`
- `uint16_t minEdges`
- `SplitPolicy splitPolicy`：`KEEP_GAP_IN_FRAME` / `DROP_GAP`
- `uint16_t frameCountMax`（CAPTURE_ALLでの強制分割上限）

> KNOWN_ONLY：プロトコル集合から合成してよい  
> CAPTURE_ALL：固定プリセット推奨

---

## 6. 受信結果・送信結果

### 6.1 RxResult
- `RxStatus status`
  - `DECODED`：既知プロトコルで論理データ取得
  - `RAW_ONLY`：デコード不能だがRAW（ITPS）取得成功
  - `NOISE_DROPPED`：ノイズとして破棄
  - `OVERFLOW`：バッファ超過等で不完全
- `ProtocolId protocol`（DECODED時）
- `LogicalPacket logical`（DECODED時）
- `ITPSBuffer raw`（RAW_ONLY時、または keepRawOnDecoded=true でDECODED時も付与）
- `uint32_t ioFlags`（任意）
  - 例：`INPUT_INVERTED_APPLIED`

### 6.2 TxStatus
- `OK`
- `BUSY`
- `INVALID_ARG`
- `OVERFLOW`
- `HAL_ERROR`

---

## 7. プロトコル拡張（Codec仕様）

### 7.1 ProtocolCodec インタフェース（機種非依存）
- `decode(const ITPSBuffer& in, LogicalPacket& out) -> DecodeStatus`
- `encode(const LogicalPacket& in, ITPSBuffer& out) -> EncodeStatus`
- `splitHint() -> SplitHint`（受信前加工の推奨値）

### 7.2 SplitHint 合成ルール（KNOWN_ONLY推奨）
有効プロトコル集合 P に対して：

- `frameGapUs = max(P.frameGapUs)`
- `hardGapUs  = max(P.hardGapUs)`
- `maxFrameUs = max(P.maxFrameUs)`
- `minEdges   = min(P.minEdges)`
- `minFrameUs = min(P.minFrameUs)`
- `splitPolicy` は受信モードで固定（KNOWN_ONLYはDROP_GAP、CAPTURE_ALLはKEEP_GAP推奨）

---

## 8. 正規化規約（ITPSの取り扱い）
- ITPSは常に「Mark=正、Space=負」
- 先頭は常にMark開始（先頭Spaceは除去）
- `seq` は 0禁止、絶対値1..127
- 同符号連続は許容（±127分割による長時間表現のため）
  - ただし「両方が|v|<127の隣接同符号」は結合して1要素にする

---

## 9. CAPTURE_ALLでの“破綻しない”挙動（必須仕様）
- `hardGapUs` で分割できる場合は通常分割
- `maxFrameUs` を超過しそうな場合：
  - **破棄せず**、可能ならSpace境界で **強制分割**して frames を増やす
- `frameCountMax` を超えたら `OVERFLOW` を返す（RAWが不完全である旨を明確化）

---

## 10. ESP32 HAL仕様（RMT利用）

### 10.1 受信（RMT）
- RMTが提供するMark/Spaceのdur列を取得し、Split/Quantizeへ渡す
- `invertInput` はRMTの設定または受信後の解釈で吸収する
- 受信バッファ不足時は `OVERFLOW` として通知

### 10.2 送信（RMT）
- ITPSFrame配列をMark/Spaceのdur列へ展開してRMTへ投入
- `invertOutput` はRMTのキャリア設定/出力極性で吸収する
- キャリア周波数・デューティ比を設定可能とする

---

## 11. デフォルト値（推奨）
### 11.1 RxConfig（ESP32既定）
- `T_us_rx=5`
- KNOWN_ONLYプリセット：
  - `frameGapUs=20000`, `hardGapUs=50000`, `minEdges=10`, `minFrameUs=3000`, `maxFrameUs=120000`, `splitPolicy=DROP_GAP`
- CAPTURE_ALLプリセット：
  - `frameGapUs=30000`, `hardGapUs=80000`, `minEdges=6`, `minFrameUs=2000`, `maxFrameUs=450000`, `splitPolicy=KEEP_GAP_IN_FRAME`, `frameCountMax=8`

### 11.2 TxConfig（ESP32既定）
- `carrierHz=38000`, `dutyPercent=33`
- `invertOutput=false`

---

## 12. 互換性・拡張性ポリシー
- ITPSFrameの `flags` は予約ビットを持ち、将来拡張しても旧版が無視できること
- 新規プロトコルはCodec登録のみで追加可能とする
- Arduino APIは破壊的変更を避け、設定構造体に項目追加で拡張する

---

## 13. 最小ユースケース（期待挙動）
1) **NEC受信（KNOWN_ONLY）**
- デコード成功時：`RxResult.status=DECODED`、論理データが返る
- デコード失敗時：`NOISE_DROPPED` または `RAW_ONLY`（設定により選択）

2) **AC学習（CAPTURE_ALL）**
- デコード不能でも `RAW_ONLY` でITPSFrame配列が返る
- 長大データでも `maxFrameUs` 超過時は強制分割し、可能な限りRAW取得を継続

3) **RAW再送信**
- 取得したITPSFrame配列を `send(raw)` で送信可能
- 反転が必要な環境でも、`invertOutput` で吸収しITPSは変更しない

