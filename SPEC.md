# ESP32IRPulseCodec Specification

## 0. Prerequisites
- Library name: **ESP32IRPulseCodec**, namespace: `esp32ir`
- Target: ESP32 on Arduino (uses RMT, compatible with Arduino-ESP32 v3+ RMT changes; v2.x is out of scope)
- Uses ITPS (normalized intermediate format, see `SPEC_ITPS.md`) for I/O
- Polarity inversion is handled in the HAL; ITPS always stores positive Mark / negative Space
- Receive: polling only / Transmit: blocking send
- Quantization (`T_us` decision) and noise cleanup are assumed to be done before producing ITPS
- Spec focuses on how users consume the API and expected behavior, not low-level implementation details
- Do not use `using namespace`; always fully qualify with `esp32ir::` (keep namespace nesting shallow)

## 1. Goals and Non-goals

### 1.1 Goals
- Let Arduino users leverage ESP32 hardware support (RMT) for stable IR TX/RX with minimal effort
- Keep codec portability via ITPS I/O, while providing only the ESP32-specific pieces needed (pull in MIT/Fork parts only as necessary)
- Balance known-protocol decoding with RAW capture for AC-sized payloads via mode switching
- Handle inversion in the HAL so ITPS always keeps positive Mark / negative Space

### 1.2 Non-goals
- Providing HALs for platforms other than ESP32 in this repo
- Implementing every IR protocol (focus on framework and major protocols first)
- Extra features such as waveform compression/encryption
- Offering broad RTOS task management options (use only what is minimally required internally)

## 2. Terms
- **Mark / Space**: Carrier ON / OFF interval (IR LED on/off)
- **Frame**: Sequence of Mark/Space intervals (ITPSFrame). Whether the gap is included depends on split policy.
- **ITPS**: Normalized intermediate format (`SPEC_ITPS.md`)
- **ITPSFrame / ITPSBuffer**: ITPS frame / frame array
- **ProtocolMessage**: Protocol-agnostic logical message (`Protocol` + raw byte array)
- **Protocol**: `esp32ir::Protocol` (NEC/SONY etc.)
- **ProtocolPayload struct**: `esp32ir::payload::<Protocol>` (decoded result / TX parameters)
- **HAL**: ESP32-specific layer (RMT, GPIO, carrier, inversion)
- **Codec**: Protocol-specific decode/encode logic
- **AC**: Air-conditioner protocols (DaikinAC and other long payloads)
- **TBD**: To be defined (spec/implementation pending)

## 3. Architecture Overview

### 3.1 Data flow (receive)
Raw (RMT) → Split/Quantize → **ITPSBuffer (ITPSFrame array)** → Decode (enabled protocols) → ProtocolMessage (logical message) → Payload (`esp32ir::payload::<Protocol>` via decode helper)  
- `useRawOnly()`: Do not decode; return ITPS as RAW.  
- `useRawPlusKnown()`: Attempt decode and attach ITPS even on success.  
- `clearProtocols()`: Clear protocol settings. If `addProtocol()` is never called, enable all known protocols + RAW.  
- `addProtocol()`: Narrow targets (ONLY when specified). If RAW is selected, prioritize RAW.  
- ProtocolMessage is the common container; normally you convert it to `esp32ir::payload::<Protocol>` via decode helpers (e.g., `decodeNEC`).

### 3.2 Data flow (transmit)
Payload (`esp32ir::payload::<Protocol>`) → TX helper → ProtocolMessage → Encode → **ITPSBuffer (ITPSFrame array)** → HAL (RMT) send  
Polarity is absorbed in the HAL (ITPS is untouched). You may also build a ProtocolMessage yourself and send it directly.

### 3.3 Learn → resend flow (RAW)
Receive RAW: Raw (RMT) → ITPSBuffer (RAW Only/RAW Plus) → store  
Resend: stored ITPSBuffer → `send(raw)` → HAL (RMT) send  
Used for AC learning or unknown protocol replay. If inversion is needed, handle it with TX-side `invertOutput`.

### 3.4 Presets
- **ALL_KNOWN (default)**: Decode all known protocols (AC included).
- **KNOWN_WITHOUT_AC**: `useKnownWithoutAC()` to target known protocols except AC.
- **RAW_ONLY / RAW_PLUS_KNOWN**: RAW-centric presets (see modes above).

### 3.5 Using payload vs. ProtocolMessage
- Typically use protocol-specific `esp32ir::payload::<Protocol>` with helpers (`decodeX` / `sendX`).
- Build `ProtocolMessage` directly only for low-level/custom cases (your own protocol, tests, etc.).

### 3.6 File layout (implementation guideline)
- Keep translation units small and focused:
  - `src/core/` for ITPSBuffer, ProtocolMessage, shared utilities
  - `src/hal/` for RMT/GPIO, carrier, inversion handling
  - `src/protocols/` per-protocol helpers (e.g., `nec.cpp`, `sony.cpp`, `aeha.cpp`; AC variants separated)
  - `src/receiver.cpp` / `src/transmitter.cpp` for class implementations
- Public umbrella header: `ESP32IRPulseCodec.h` includes protocol/helper declarations needed by users.

### 3.7 Layering
1) **Core (platform-agnostic)**  
   - ITPSFrame/Normalize, ProtocolCodec interfaces  
   - Helpers to convert between ProtocolMessage and payload::<Protocol> (decode/send)
2) **ESP32 HAL**  
   - RMT RX/TX, GPIO/carrier setup, inversion, buffer management  
   - Compatible with Arduino-ESP32 v3+ RMT API
3) **Arduino API**  
   - `esp32ir::Receiver` / `esp32ir::Transmitter`  
   - Mode selection, protocol selection, polling/blocking send, etc.

## 4. Receive Modes and Split Policy

- Modes
  - **KNOWN_ONLY** (default): Decode using enabled protocols. RAW is attached only when `useRawPlusKnown()` is selected.
  - **RAW_ONLY** (`useRawOnly()`): Do not decode; return ITPS.
  - **RAW_PLUS_KNOWN** (`useRawPlusKnown()`): Try decode and keep RAW even on success.
- Split/robustness
  - Use `frameGapUs` / `hardGapUs` to split frames. Space longer than `hardGapUs` forces a split.
  - If `maxFrameUs` would be exceeded, force split at a Space boundary to avoid dropping data.
  - `minFrameUs` / `minEdges` filter out noise before generating RxResult.
  - `splitPolicy`: `DROP_GAP` (for decoding, do not include gap) / `KEEP_GAP_IN_FRAME` (for RAW).
  - If `frameCountMax` is exceeded, notify as `OVERFLOW` and still return whatever RAW was captured.
  - ITPS normalization follows SPEC_ITPS: starts with Mark, `seq[i]` never 0, range `1..127/-1..-127`, long segments split at ±127, unnecessary splits removed, polarity not inverted.
- `T_us` is common across frames; default assumption 5us (adjust earlier if needed).
- Parameter selection
  - Each protocol has recommended params (`frameGapUs/hardGapUs/minFrameUs/maxFrameUs/minEdges/splitPolicy`). On begin, merge recommendations from enabled protocols to form defaults.
  - Merge guideline:
    - Upper bounds (`frameGapUs`, `hardGapUs`, `maxFrameUs`): take the maximum (most permissive) to avoid premature cuts.
    - Noise filters (`minFrameUs`, `minEdges`): take the maximum (strictest) to avoid noise hits.
    - `splitPolicy`: prefer `KEEP_GAP_IN_FRAME` for RAW modes, `DROP_GAP` for KNOWN modes. User settings override.
  - In `useRawOnly` / `useRawPlusKnown`, prioritize RAW-friendly presets (wider gaps, `KEEP_GAP_IN_FRAME`, `frameCountMax`, etc.) and ignore protocol recommendations if needed.

---

## 5. Naming
- Namespace: `esp32ir`
- Classes
  - `esp32ir::Receiver`
  - `esp32ir::Transmitter`
- Data types
  - `esp32ir::RxResult`
  - `esp32ir::ProtocolMessage`
  - `esp32ir::ITPSFrame`
  - `esp32ir::ITPSBuffer`
  - `esp32ir::Protocol`
- Protocol payload structs
  - `esp32ir::payload::<Protocol>` (e.g., NEC, SONY, AEHA, ...; see “Supported Protocols and Helpers”)

---

## 6. Receiver (RX)

### 6.1 Constructors
```cpp
esp32ir::Receiver();
esp32ir::Receiver(int rxPin, bool invert=false, uint16_t T_us_rx=5);
```
- Defaults (assumed): no-arg means you will set pin etc. later. Arg version lets you set pin, and optional defaults for `invert=false`, `T_us_rx=5us`.

### 6.2 Setters (only before begin)
```cpp
bool setPin(int rxPin);
bool setInvertInput(bool invert);
bool setQuantizeT(uint16_t T_us_rx);
```
- Defaults (assumed): `invert=false`, `T_us_rx=5us`

### 6.3 begin/end
```cpp
bool begin();
void end();
```
- begin initializes RMT RX and starts background reception. Incoming data is queued; retrieve via `poll`.
- end stops background RX and releases RMT resources.

### 6.4 Protocol selection (only before begin)
```cpp
bool addProtocol(esp32ir::Protocol protocol);
bool clearProtocols();
bool useRawOnly();
bool useRawPlusKnown();
bool useKnownWithoutAC();  // preset excluding AC protocols
```

- No protocol specified → ALL_KNOWN
- Protocols specified → ONLY
- RAW selections take priority
- ALL_KNOWN includes AC (DaikinAC, etc.). To exclude AC, use `useKnownWithoutAC()` or restrict with `addProtocol`.
- Merge protocol-recommended params at begin to decide RX defaults (see “Receive Modes and Split Policy”).

---

## 7. poll and Ownership
```cpp
bool poll(esp32ir::RxResult& out);
```

- Caller owns RxResult; it is not destroyed on the next poll. Internally managed via a bounded FIFO; on overflow, older data is dropped and `OVERFLOW` is reported. Noise drops do not emit RxResult.
- Return value is bool (“did we get one?”). Simple usage:
  ```cpp
  esp32ir::RxResult rxResult;
  if (rx.poll(rxResult)) {  // only when data exists
    switch (rxResult.status) {
      case esp32ir::RxStatus::DECODED:
        // handle rxResult.message
        break;
      case esp32ir::RxStatus::RAW_ONLY:
      case esp32ir::RxStatus::OVERFLOW:
        // handle rxResult.raw (e.g., save/resend)
        break;
    }
  }
  ```

---

## 8. RxResult
Holds the receive result from `poll`, containing both ProtocolMessage (logical) and ITPS RAW.
```cpp
struct RxResult {
  esp32ir::RxStatus status;
  esp32ir::Protocol protocol;
  esp32ir::ProtocolMessage message;
  esp32ir::ITPSBuffer raw;
};
```

- In RAW_PLUS, raw is always included even when DECODED.
- In RAW_ONLY, protocol/message are unused.
- On OVERFLOW, raw contains whatever was captured.
- Protocol decode helpers accept RxResult and return true only when `status==DECODED` and the protocol matches (e.g., `bool esp32ir::decodeNEC(const esp32ir::RxResult& in, esp32ir::payload::NEC& out);`).
- TX helpers can also take decoded structs; bare-argument versions should delegate to struct versions to avoid duplicate logic.

---

## 9. ProtocolMessage
Protocol-agnostic logical message (`Protocol` + raw bytes). Usually built by protocol helpers; manual assembly is for low-level/custom cases.
```cpp
struct ProtocolMessage {
  esp32ir::Protocol protocol;
  const uint8_t* data;
  uint16_t length;
  uint32_t flags;
};
```

- Protocol-specific payload structs live in `esp32ir::payload::<Protocol>` (e.g., `payload::NEC`). On RX, use decode helpers (`decodeNEC`, etc.) to convert ProtocolMessage → `payload::<Protocol>`. On TX, send helpers (`sendNEC`, etc.) build ProtocolMessage → ITPS from `payload::<Protocol>`.

---

## 10. ITPS

ITPS is the normalized intermediate format for Mark/Space. See `SPEC_ITPS.md` for details.

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

- Assumes normalized ITPS (per `SPEC_ITPS.md`):
  - `T_us` is common across the frame array; 0/missing/mismatch is invalid.
  - `seq` is read-only, `seq[0] > 0`, `seq[i] != 0`, `1 <= abs(seq[i]) <= 127`. Long segments are split at ±127; sub-127 splits are merged.
  - Quantization (`T_us`) and micro noise removal are done before ITPS creation.
  - flags are reserved (unused). Polarity is not stored in ITPS; handled by HAL. Time calculations (`totalTimeUs`, etc.) use 32-bit or wider.

---

## 11. Transmitter (TX)

### 11.1 Constructors
```cpp
esp32ir::Transmitter();
esp32ir::Transmitter(int txPin, bool invert=false, uint32_t hz=38000);
```
- Defaults (assumed): no-arg means you will set pin etc. later. Arg version lets you set pin plus optional defaults (`invert=false`, `hz=38000` Hz, duty ≈50%, `gapUs=40000us`).

### 11.2 Setters (only before begin)
```cpp
bool setPin(int txPin);
bool setInvertOutput(bool invert);
bool setCarrierHz(uint32_t hz);
bool setDutyPercent(uint8_t dutyPercent);
bool setGapUs(uint32_t gapUs);
```
- Defaults (assumed): `invert=false`, `hz=38000` Hz, `dutyPercent` around 50%, `gapUs=40000us` (TX gap default). Protocol helpers may carry their own recommended gaps; if `setGapUs` has not overridden, those recommendations take priority.

### 11.3 begin/end
```cpp
bool begin();
void end();
```
- begin initializes RMT for TX; end releases resources (send is blocking, so begin/end mainly manage resources).

### 11.4 send (blocking)
```cpp
bool send(const esp32ir::ITPSBuffer& itps);
bool send(const esp32ir::ProtocolMessage& message);
```
- See “Supported Protocols and Helpers” for protocol-specific send helpers (`esp32ir::sendNEC`, etc.). `gapUs` uses user override if set, else helper recommendation, else default 40ms.

---

## 12. Supported Protocols and Helpers
- Policy: Provide decode/send helpers per protocol; normally both struct and bare-argument versions (AC: struct only). If `addProtocol` is not called, enable all known protocols + RAW.
- Status legend (○=helper implemented, △=stub/planned, RAW is ITPS direct)
  - AC: arbitrary payload generation is TBD, but RAW-mode learning and RAW resend (struct helper or `send(raw)`) are expected to work.

| Protocol                 | Payload struct                     | Decode helper                   | Send helper                                | Status |
|--------------------------|------------------------------------|---------------------------------|--------------------------------------------|--------|
| RAW (ITPS)               | none (ITPSBuffer direct)           | -                               | `tx.send(ITPSBuffer)`                      | ○      |
| NEC                      | `esp32ir::payload::NEC`            | `esp32ir::decodeNEC`            | `tx.sendNEC(struct/args)`                   | ○      |
| SONY (SIRC 12/15/20)     | `esp32ir::payload::SONY`           | `esp32ir::decodeSONY`           | `tx.sendSONY(struct/args)`                  | ○      |
| AEHA (Japanese Home Elec)| `esp32ir::payload::AEHA`           | `esp32ir::decodeAEHA`           | `tx.sendAEHA(struct/args)`                  | △      |
| Panasonic/Kaseikaden     | `esp32ir::payload::Panasonic`      | `esp32ir::decodePanasonic`      | `tx.sendPanasonic(struct/args)`             | △      |
| JVC                      | `esp32ir::payload::JVC`            | `esp32ir::decodeJVC`            | `tx.sendJVC(struct/args)`                   | △      |
| Samsung                  | `esp32ir::payload::Samsung`        | `esp32ir::decodeSamsung`        | `tx.sendSamsung(struct/args)`               | △      |
| LG                       | `esp32ir::payload::LG`             | `esp32ir::decodeLG`             | `tx.sendLG(struct/args)`                    | △      |
| Denon/Sharp              | `esp32ir::payload::Denon`          | `esp32ir::decodeDenon`          | `tx.sendDenon(struct/args)`                 | △      |
| RC5                      | `esp32ir::payload::RC5`            | `esp32ir::decodeRC5`            | `tx.sendRC5(struct/args)`                   | △      |
| RC6                      | `esp32ir::payload::RC6`            | `esp32ir::decodeRC6`            | `tx.sendRC6(struct/args)`                   | △      |
| Apple (NEC ext.)         | `esp32ir::payload::Apple`          | `esp32ir::decodeApple`          | `tx.sendApple(struct/args)`                 | △      |
| Pioneer                  | `esp32ir::payload::Pioneer`        | `esp32ir::decodePioneer`        | `tx.sendPioneer(struct/args)`               | △      |
| Toshiba                  | `esp32ir::payload::Toshiba`        | `esp32ir::decodeToshiba`        | `tx.sendToshiba(struct/args)`               | △      |
| Mitsubishi               | `esp32ir::payload::Mitsubishi`     | `esp32ir::decodeMitsubishi`     | `tx.sendMitsubishi(struct/args)`            | △      |
| Hitachi                  | `esp32ir::payload::Hitachi`        | `esp32ir::decodeHitachi`        | `tx.sendHitachi(struct/args)`               | △      |
| Daikin AC                | `esp32ir::payload::DaikinAC`       | `esp32ir::decodeDaikinAC`       | `tx.sendDaikinAC(struct)`                   | △      |
| Panasonic AC             | `esp32ir::payload::PanasonicAC`    | `esp32ir::decodePanasonicAC`    | `tx.sendPanasonicAC(struct)`                | △      |
| Mitsubishi AC            | `esp32ir::payload::MitsubishiAC`   | `esp32ir::decodeMitsubishiAC`   | `tx.sendMitsubishiAC(struct)`               | △      |
| Toshiba AC               | `esp32ir::payload::ToshibaAC`      | `esp32ir::decodeToshibaAC`      | `tx.sendToshibaAC(struct)`                  | △      |
| Fujitsu AC               | `esp32ir::payload::FujitsuAC`      | `esp32ir::decodeFujitsuAC`      | `tx.sendFujitsuAC(struct)`                  | △      |

- Structs and helper details
  - RAW  
    - Work with ITPSBuffer directly. Enable via `useRawOnly` / `useRawPlusKnown`.  
    - Resend with `send(const esp32ir::ITPSBuffer& raw);`.
  - NEC  
    - `struct esp32ir::payload::NEC { uint16_t address; uint8_t command; bool repeat; };`  
    - `bool esp32ir::decodeNEC(const esp32ir::RxResult& in, esp32ir::payload::NEC& out);`  
    - `bool esp32ir::Transmitter::sendNEC(const esp32ir::payload::NEC& p);` / `bool esp32ir::Transmitter::sendNEC(uint16_t address, uint8_t command, bool repeat=false);` (gap uses helper recommendation; falls back to default 40ms)
  - SONY (SIRC 12/15/20bit)  
    - `struct esp32ir::payload::SONY { uint16_t address; uint16_t command; uint8_t bits; };`  
    - `bool esp32ir::decodeSONY(const esp32ir::RxResult& in, esp32ir::payload::SONY& out);` (`bits` is only 12/15/20)  
    - `bool esp32ir::Transmitter::sendSONY(const esp32ir::payload::SONY& p);` / `bool esp32ir::Transmitter::sendSONY(uint16_t address, uint16_t command, uint8_t bits=12);` (bits only 12/15/20; gap uses helper recommendation, else 40ms)
  - AEHA  
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
  - Apple (NEC ext.)  
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
    - Struct reserved: `esp32ir::payload::DaikinAC` (fields TBD). Functions reserved: `bool esp32ir::decodeDaikinAC(const esp32ir::RxResult&, esp32ir::payload::DaikinAC&);` / `bool esp32ir::sendDaikinAC(const esp32ir::payload::DaikinAC&);`
  - Panasonic AC  
    - Struct reserved: `esp32ir::payload::PanasonicAC` (fields TBD). Functions reserved: `bool esp32ir::decodePanasonicAC(const esp32ir::RxResult&, esp32ir::payload::PanasonicAC&);` / `bool esp32ir::sendPanasonicAC(const esp32ir::payload::PanasonicAC&);`
  - Mitsubishi AC  
    - Struct reserved: `esp32ir::payload::MitsubishiAC` (fields TBD). Functions reserved: `bool esp32ir::decodeMitsubishiAC(const esp32ir::RxResult&, esp32ir::payload::MitsubishiAC&);` / `bool esp32ir::sendMitsubishiAC(const esp32ir::payload::MitsubishiAC&);`
  - Toshiba AC  
    - Struct reserved: `esp32ir::payload::ToshibaAC` (fields TBD). Functions reserved: `bool esp32ir::decodeToshibaAC(const esp32ir::RxResult&, esp32ir::payload::ToshibaAC&);` / `bool esp32ir::sendToshibaAC(const esp32ir::payload::ToshibaAC&);`
  - Fujitsu AC  
    - Struct reserved: `esp32ir::payload::FujitsuAC` (fields TBD). Functions reserved: `bool esp32ir::decodeFujitsuAC(const esp32ir::RxResult&, esp32ir::payload::FujitsuAC&);` / `bool esp32ir::sendFujitsuAC(const esp32ir::payload::FujitsuAC&);`

## 13. Common Policies (Error/Timing/Log/Samples)
- **Error handling**: No exceptions. Public APIs return `bool` success only. On failure, log appropriately and return false (including misuse). Keep running; no assert/abort. TX APIs also do not expose detailed status.
- **Time/scheduling**: Base waits on Arduino `delay()`; internal tick is fixed at 1 ms only. No high-precision timers or alternate tick rates considered.
- **Logging**: Use ESP-IDF `ESP_LOGE/W/I/D/V` even in Arduino. Tag is always `ESP32IRPulseCodec`. If class hint is needed, prefix message with `RX`/`TX`, etc. Level guide: E=fatal/operation failed, W=retry/timeout, I=init/settings, D/V=debug detail. Log init/level selection is expected via Arduino IDE “Core Debug Level” (board setting).
- **Samples**: Comments bilingual (en/ja) — if at line head, split lines (`// en: ...` then `// ja: ...`); if inline, `// en: ... / ja: ...`. Provide many simple, feature-focused examples; use numbered project names like `01_...` to indicate reading order.

---

## 14. TX Gap
- Base value 40ms (`gapUs`), but protocol send helpers prefer their recommended gap; if `setGapUs` overrides, use that.
- Ensure at least `gapUs` Space after the last frame before considering send complete.
- When sending RAW/ProtocolMessage directly, apply the currently set `gapUs`.

---

## 15. ESP32 HAL (RMT)
- **Receive**: Get RMT Mark/Space duration list, quantize/normalize per SPEC_ITPS (start with Mark, ±127 split, remove needless splits), then frame-split and convert to ITPS. `invertInput` is absorbed via RMT settings or sign interpretation. On buffer shortage, notify as `OVERFLOW`.
- **Transmit**: Expand ITPSBuffer (ITPSFrame array) to Mark/Space durations and feed RMT. Carrier freq/duty/inversion (`invertOutput`) handled in RMT settings; ITPS itself is unchanged.

---

## 16. Compatibility/Extensibility Policy
- ITPSFrame `flags` are reserved bits; allow future extensions while old versions can ignore them.
- New protocols should be addable via codec registration (decode/send helpers + recommended params).
- Arduino API avoids breaking changes; extend by adding settings, keep existing signatures.

---

## 17. Samples and Expected Behavior

### 17.1 Code examples (fully qualified)

NEC transmit (minimal sketch)
```cpp
#include <ESP32IRPulseCodec.h>

esp32ir::Transmitter tx(25);

void setup() {
  tx.begin();
  // Example: send address 0x00FF, command 0xA2 once
  tx.sendNEC(0x00FF, 0xA2);
}

void loop() {
  // TX only; add delay if you need spacing
  delay(100);
}
```

NEC receive (decode helper)
```cpp
#include <ESP32IRPulseCodec.h>

esp32ir::Receiver rx(23);

void setup() {
  rx.addProtocol(esp32ir::Protocol::NEC);  // target NEC only
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

### 17.2 Expected behavior (use cases)
- **Default (no protocol specified / KNOWN_ONLY)**: Decodes all known protocols and returns `status=DECODED` only. No RAW unless explicitly `useRawPlusKnown()` / `useRawOnly()`.
- **NEC decode**: With defaults, returns `status=DECODED` ProtocolMessage and `decodeNEC` returns true. `useRawPlusKnown()` attaches RAW as well.
- **RAW-centric learning**: `useRawOnly()` always returns `status=RAW_ONLY` ITPS (no decode attempt). `useRawPlusKnown()` keeps RAW even on successful decode.
- **Long AC capture**: For long AC frames, split before exceeding `maxFrameUs` at Space boundaries to preserve as much RAW as possible. If `frameCountMax` is exceeded, return `status=OVERFLOW` with captured RAW.
- **Protocol restriction**: `addProtocol(NEC)` limits decode to NEC (RAW prioritized if selected). `useKnownWithoutAC()` targets known protocols except AC. With no specification, decode all known protocols in KNOWN_ONLY mode (RAW requires explicit selection).
- **Queue behavior**: If the queue is empty, `poll` returns false. On overflow, older entries are dropped and the next result indicates `status=OVERFLOW`.
- **RAW resend**: ITPSBuffer from RX can be sent directly via `send(raw)`. If inversion is needed, use `invertOutput`; ITPS stays unchanged.
