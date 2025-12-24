# 05_serial_tx_all_formats

シリアル経由で1行コマンドを入力し、`sendNEC` などのバラ引数送信ヘルパーを使って（ACを除く）全対応フォーマットを送信するサンプルです。TXピンはスケッチ内で 25 に設定しています。ご利用の基板に合わせて `esp32ir::Transmitter tx(25);` を変更してください。

## 動作環境
- Arduino-ESP32 v3 以降（本ライブラリ推奨環境）
- シリアル 115200bps
- IR LED（38kHz キャリア想定）、適切な抵抗・トランジスタ等のドライバ回路

## 使い方
1. スケッチを ESP32 に書き込み、シリアルモニタ（115200bps）を開きます。
2. 1行でコマンドを入力すると送信します。例:
   - `NEC 0x00FF 0xA2`
   - `NEC 0x00FF 0xA2 repeat=1 count=3 interval_ms=120`
   - `SONY 0x01 0x1A 20`
   - `RC6 0x1234 0 toggle=1`
3. 送信に成功すると `OK ...`、失敗すると `ERR ...` を返します。

## コマンド仕様
- 入力: 行末 (`\n`) 区切り、トークンは空白区切り。大文字/小文字は無視。数値は10進/`0x`16進を許容。真偽値は `1/0/true/false/on/off`。
- 共通オプション: 任意の送信コマンド末尾に `count=<n>`（繰り返し回数、既定1）、`interval_ms=<m>`（繰り返し間隔、既定80ms）を付与可能。
- 応答: 成功 `OK <echo>`、失敗 `ERR <reason>`。

### 制御コマンド
- `HELP` / `?` : コマンド一覧を表示
- `LIST` : 対応プロトコル名を表示
- `CFG` : 現在設定を表示（pin/invert/carrier/duty/gap）
- `GAP <ms>` : `setGapUs` を ms で上書き（推奨値優先を無効化）
- `CARRIER <hz>` : `setCarrierHz` を設定
- `DUTY <percent>` : `setDutyPercent` を設定
- `INVERT <0|1>` : `setInvertOutput` を設定

### プロトコル別送信コマンド
- `NEC <address16> <command8> [repeat]` （`repeat=1` でリピートコード送信）
- `SONY <address> <command> <bits>` （bits は 12/15/20）
- `AEHA <address16> <data> <nbits>` （nbits=1..32）
- `PANA | PANASONIC <address16> <data> <nbits>` （nbits=1..32）
- `JVC <address16> <command8> [bits]`（`bits`=24または32、省略時32）。32bitモードではコマンドの反転バイトを自動付与します。
- `SAMSUNG <address16> <command16>`
- `LG <address16> <command16>`
- `DENON <address16> <command16> [repeat]`
- `RC5 <command> [toggle]` （アドレスは実装上0固定）
- `RC6 <command16> <mode> [toggle]` （mode=0..7）
- `APPLE <address16> <command8>`
- `PIONEER <address16> <command16> [extra8]`
- `TOSHIBA <address16> <command16> [extra8]`
- `MITSUBISHI <address16> <command16> [extra8]`
- `HITACHI <address16> <command16> [extra8]`

※ AC系は構造体版のみのためこのサンプルでは送信対象外。RAW送信も対象外です。
