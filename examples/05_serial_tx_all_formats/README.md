# 05_serial_tx_all_formats

Serial-driven IR TX console that uses the flat helper methods (`sendNEC`, `sendSONY`, etc.) to send every supported non-AC protocol. The TX pin is set to 25 in the sketch; change `esp32ir::Transmitter tx(25);` to match your wiring.

## Requirements
- Arduino-ESP32 v3 or later (recommended environment for this library)
- Serial 115200bps
- IR LED with proper driver circuitry (38 kHz carrier expected)

## Usage
1. Flash the sketch to your ESP32 and open a serial monitor at 115200bps.
2. Type one command per line. Examples:
   - `NEC 0x00FF 0xA2`
   - `NEC 0x00FF 0xA2 repeat=1 count=3 interval_ms=120`
   - `SONY 0x01 0x1A 20`
   - `RC6 0x1234 0 toggle=1`
3. On success you get `OK ...`; on failure you get `ERR ...`.

## Command spec
- Input: line-delimited (`\n`), tokens separated by whitespace. Case-insensitive. Numbers can be decimal or `0x` hex. Booleans accept `1/0/true/false/on/off`.
- Common options: append `count=<n>` (repeat count, default 1) and `interval_ms=<m>` (repeat interval, default 80ms) to any send command.
- Response: `OK <echo>` on success, `ERR <reason>` on failure.

### Control commands
- `HELP` / `?` : show available commands
- `LIST` : list supported protocol names
- `CFG` : show current settings (pin/invert/carrier/duty/gap)
- `GAP <ms>` : override `setGapUs` in milliseconds (disables per-protocol recommended gap)
- `CARRIER <hz>` : set `setCarrierHz`
- `DUTY <percent>` : set `setDutyPercent`
- `INVERT <0|1>` : set `setInvertOutput`

### Protocol send commands
- `NEC <address16> <command8> [repeat]` (use `repeat=1` to send the NEC repeat frame)
- `SONY <address> <command> <bits>` (bits = 12/15/20)
- `AEHA <address16> <data> <nbits>` (nbits=1..32)
- `PANA | PANASONIC <address16> <data> <nbits>` (nbits=1..32)
- `JVC <address16> <command16> [bits]` (`bits`=24 or 32; default 32)
- `SAMSUNG <address16> <command16>`
- `LG <address16> <command16>`
- `DENON <address16> <command16> [repeat]`
- `RC5 <command> [toggle]` (address is fixed to 0 in current implementation)
- `RC6 <command16> <mode> [toggle]` (mode=0..7)
- `APPLE <address16> <command8>`
- `PIONEER <address16> <command16> [extra8]`
- `TOSHIBA <address16> <command16> [extra8]`
- `MITSUBISHI <address16> <command16> [extra8]`
- `HITACHI <address16> <command16> [extra8]`

AC helpers are struct-only, so AC protocols are not covered by this console. RAW send is also out of scope.
