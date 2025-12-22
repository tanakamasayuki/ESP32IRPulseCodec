# SPEC_AC（エアコンIR仕様）

## 0. 概要と位置づけ
- 本書はESP32IRPulseCodecのエアコンIR仕様を独立させたドキュメント。`SPEC.ja.md` にあったAC関連をベースに再構成し、参照なしで読めるようにした。
- 目的：ベンダー非依存でエアコンのIR制御を記述する。「既存ライブラリ互換」を狙わず、クリーンな抽象化と拡張性を優先する。
- 方針：送信は「完全なデバイス状態」を表現するフレームのみ。未知ビットは必ず保持し、部分的な上書きで壊さない。
- レイヤ：Intent → 正規化DeviceState → Protocol Frame（bytes/bits） → IR Waveform（ITPS） → 送信。ITPSの扱い・送受信フローは `SPEC.ja.md` を準拠。

## 1. 基本設計原則
- 状態ベース送信：ACリモコンは毎回「全状態」を送る前提でエンコードする。
- 関心分離：ユーザー意図（Intent）、デバイス状態、プロトコルエンコード、IR波形を段階分離する。
- Capabilities-first：機種ごとの対応範囲を宣言し、それに基づいて正規化・バリデーションする。
- 未知ビット保全：理解できないデータは `opaque.preservedBits` や `opaque.vendorData` で保持し、往復で失わない。
- MIT互換：仕様/実装ともMITで配布可能な構造を維持する。

## 2. 概念レイヤとデータモデル

### 2.1 処理レイヤ
```
User Intent
  ↓
Normalized Device State
  ↓
Protocol Frame (bytes/bits)
  ↓
IR Waveform (ITPS)
  ↓
IR Transmitter
```

### 2.2 Intent（ユーザー意図）
- 型一覧：`SetPower`、`SetMode`、`SetTargetTemperature`、`SetFanSpeed`、`SetSwing`、`ToggleFeature`、`SetFeature`。
- 必須プロパティの対応：
  - `SetPower`: `power`(bool)
  - `SetMode`: `mode`(`auto|cool|heat|dry|fan|off`)
  - `SetTargetTemperature`: `targetTemperature`(number), `temperatureUnit`(`C|F`)
  - `SetFanSpeed`: `fanSpeed`(`auto` または整数>=1)
  - `SetSwing`: `swing.vertical` / `swing.horizontal`（値 or null）
  - `ToggleFeature`: `feature`(string)
  - `SetFeature`: `feature`(string), `value`(bool/number/string/null)
- ルール：増分適用可。プロトコル知識を持たないまま事前バリデーションし、無効値はエンコード前に弾く。

### 2.3 正規化済みDeviceState
- 必須：`power`（bool）、`mode`、`targetTemperature`、`temperatureUnit (C|F)`、`fanSpeed`（auto or >=1）、`swing.vertical/horizontal`（値 or null）。
- 任意・代表例：`features`（任意キー → bool/number/string/null）、`sleepTimerMinutes`、`clockMinutesSinceMidnight`、`device`（DeviceRef：vendor/model/brand/protocolHint/notes）。
- 既知機能（quiet/turbo/eco/light など）は `features` で表現可。`OnOffToggle` 型（on/off/toggle）も許容。
- `opaque`：`preservedBits`（バイト配列0..255）と `vendorData`（任意JSON）。未知フィールドはここに入れ、再エンコード時に保持する。

### 2.4 Capabilities（機種宣言）
- `device`：vendor, model（必須）、brand/protocolHint/notes（任意）。
- 温度：`min` / `max` / `step`（>0）/ `units`（C/Fの配列）。
- モード：`modes` 配列（`auto|cool|heat|dry|fan|off` のうち対応するもの）。
- 風量：`fan.autoSupported`（bool）、`fan.levels`（整数, 0はautoのみ）、`namedLevels`（任意ラベル配列）。
- スイング：`swing.verticalValues` / `horizontalValues`（サポート値配列。`off` や `auto` も値として扱う）。
- 機能：`features.supported`（列挙）、`toggleOnly`（トグルのみの機能）、`mutuallyExclusiveSets`（排他セット）、`featureValueTypes`（boolean/number/string/onOffToggle で値型を明示）。
- 電源挙動：`powerBehavior.powerRequiredForModeChange` / `powerRequiredForTempChange`（変更に電源ONが必須か）。
- バリデーション方針：`validationPolicy` で outOfRangeTemperature（reject/coerce）、unsupportedMode（reject/coerceToAuto/coerceToOff）、unsupportedFanSpeed（reject/coerceToAuto/coerceToNearest）、unsupportedSwing（reject/coerceToOff/coerceToNearest）を指定。

## 3. 正規化とバリデーション
- 流れ：1) 直前状態にIntentを適用 → 2) Capabilitiesで検証 → 3) 許可される場合のみ丸め（coerce） → 4) 正規化DeviceStateを生成。
- 結果種別：`reject`（危険/未定義は破棄）、`coerce`（丸めや代替値に変更）。例：25.3°C → 0.5°Cステップなら 25.5°C に丸め。
- 未知ビット/フィールドは `opaque` を通じて維持し、既知フィールドが上書きする部分だけを変更する。

## 4. プロトコル/IRエンコード要件
- 各プロトコルで定義するもの：フレーム長、ビットオーダー（LSB/MSB）、フィールド割り当て、チェックサム。
- ルール：フレームは「完全状態」を表し、部分アップデートは禁止。学習→再送でも状態を壊さない。
- IR波形：キャリア周波数（例：38kHz）、ヘッダMark/Space、ビットMark/Space、ストップビット、リピート回数/間隔を明示。タイミング許容誤差（例：±20%）を設ける。
- ITPS連携：Protocol FrameをITPSに変換して送信し、受信したITPSからProtocol Frameを復元する。反転吸収はHAL側（`SPEC.ja.md`）で扱う。

## 5. テストとサンプル
- テストベクタ項目：`name`、`device`、`inputState`、`intents`、`expectedFrameHex`、`expectedCarrierHz`、`notes`。入力StateやIntent→期待フレーム（hex）を1セットとして管理。
- 参考テストベクタ例：
  ```json
  {
    "name": "cool-26C",
    "device": { "vendor": "ExampleVendor", "model": "X100" },
    "state": { "power": true, "mode": "cool", "targetTemperature": 26, "temperatureUnit": "C", "fanSpeed": "auto", "swing": { "vertical": "auto", "horizontal": "off" } },
    "frame": "A1 F0 3C 9D",
    "expectedCarrierHz": 38000,
    "checksum": "9D"
  }
  ```
- 機種差異テンプレート（暫定値、実機キャプチャで要確認）：
  - **Daikin系例**：温度18–30°C/1°C、モード auto/cool/heat/dry/fan/off、風量 auto+5段、スイング垂直/水平とも多段。機能 quiet/turbo/eco/light/beep/clean。light/beepはトグルのみ、quietとturboは排他。
  - **Panasonic系例**：温度16–30°C/0.5°C、風量 auto+4段、スイング垂直多段・水平は off/auto。機能 eco/nanoe/iFeel/light/sleep。lightはトグルのみ。
  - **Mitsubishi Electric系例**：温度16–31°C/1°C、風量 auto+5段、スイング垂直多段・水平は off/auto/left/mid/right。機能 quiet/turbo/eco/iFeel/beep（beepはトグルのみ、quietとturboは排他）。
- 本番では実機キャプチャや公式仕様と突き合わせ、Capabilities/TestVectorを更新する。

## 6. 拡張と互換ポリシー
- 新フィールドは常にオプションで追加し、既存データとの後方互換を確保する。未知データは `opaque` に残し、往復で失わないことを必須とする。
- 破壊的変更はメジャーバージョンアップ時のみ許容。

## 7. ライブラリ実装メモ（ESP32IRPulseCodec向け）
- AC向けコードは「Intent/Capabilities → 正規化DeviceState → Protocol Frame → ITPS送信」のパイプラインを保つ。途中で未知ビットを消さない。
- 受信側は学習RAW（ITPS）を保持しつつ、デコードに成功した場合でも `opaque` を経由して未知領域を温存する。
- Capabilitiesをもとに正規化/丸めを行い、`useKnownWithoutAC` 等のプリセットとも両立できるように実装を分離する。
