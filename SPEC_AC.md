# SPEC_AC (Air Conditioner IR Specification)

## 0. Overview & Scope
- This document is the standalone AC IR spec for ESP32IRPulseCodec. It consolidates AC content that previously lived inside the main spec and can be read without cross-reference.
- Goal: describe vendor-agnostic AC IR control. Rather than compatibility with legacy libraries, prioritize clean abstraction and extensibility.
- Policy: transmit only “full device state” frames. Unknown bits must be preserved; no partial updates that destroy state.
- Layers: Intent → Normalized DeviceState → Protocol Frame (bytes/bits) → IR Waveform (ITPS) → TX. ITPS handling follows `SPEC.md` (polarity absorbed by HAL).

## 1. Core Design Principles
- State-based TX: AC remotes send full state every time; encoders follow that model.
- Separation of concerns: user intent, device state, protocol encoding, and IR waveform are distinct stages.
- Capabilities-first: each model declares supported ranges/features, then normalization/validation uses that.
- Preserve unknowns: keep opaque bits in `opaque.preservedBits` / `opaque.vendorData`; never drop them on round-trips.
- MIT-friendly: structure remains suitable for MIT-licensed implementations.

## 2. Conceptual Layers & Data Model

### 2.1 Processing layers
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

### 2.2 Intent
- Types: `SetPower`, `SetMode`, `SetTargetTemperature`, `SetFanSpeed`, `SetSwing`, `ToggleFeature`, `SetFeature`.
- Required properties:
  - `SetPower`: `power` (bool)
  - `SetMode`: `mode` (`auto|cool|heat|dry|fan|off`)
  - `SetTargetTemperature`: `targetTemperature` (number), `temperatureUnit` (`C|F`)
  - `SetFanSpeed`: `fanSpeed` (`auto` or integer >=1)
  - `SetSwing`: `swing.vertical` / `swing.horizontal` (value or null)
  - `ToggleFeature`: `feature` (string)
  - `SetFeature`: `feature` (string), `value` (bool/number/string/null)
- Rules: intents apply incrementally; validate before encoding without needing protocol knowledge; reject invalid values early.

### 2.3 Normalized DeviceState
- Required: `power` (bool), `mode`, `targetTemperature`, `temperatureUnit (C|F)`, `fanSpeed` (auto or >=1), `swing.vertical/horizontal` (value or null).
- Optional examples: `features` (arbitrary keys → bool/number/string/null), `sleepTimerMinutes`, `clockMinutesSinceMidnight`, `device` (DeviceRef: vendor/model/brand/protocolHint/notes).
- Known features (quiet/turbo/eco/light, etc.) live in `features`; `OnOffToggle` (on/off/toggle) is allowed.
- `opaque`: `preservedBits` (byte array 0..255) and `vendorData` (any JSON). Unknown fields go here and must survive re-encode.

### 2.4 Capabilities
- `device`: vendor, model (required); brand/protocolHint/notes (optional).
- Temperature: `min` / `max` / `step` (>0) / `units` (array of C/F).
- Modes: `modes` array (subset of `auto|cool|heat|dry|fan|off`).
- Fan: `fan.autoSupported` (bool), `fan.levels` (int, 0 means auto-only), `namedLevels` (optional labels).
- Swing: `swing.verticalValues` / `horizontalValues` (supported values; `off`/`auto` are values too).
- Features: `features.supported`, `toggleOnly`, `mutuallyExclusiveSets`, `featureValueTypes` (boolean/number/string/onOffToggle).
- Power behavior: `powerBehavior.powerRequiredForModeChange` / `powerRequiredForTempChange`.
- Validation policy: `validationPolicy` for outOfRangeTemperature (reject/coerce), unsupportedMode (reject/coerceToAuto/coerceToOff), unsupportedFanSpeed (reject/coerceToAuto/coerceToNearest), unsupportedSwing (reject/coerceToOff/coerceToNearest).

## 3. Normalization & Validation
- Flow: (1) apply intents to previous state → (2) validate via Capabilities → (3) coerce only when allowed → (4) emit normalized DeviceState.
- Outcomes: `reject` (drop unsafe/undefined), `coerce` (round/replace). Example: 25.3°C → 25.5°C if step is 0.5°C.
- Preserve unknown bits/fields through `opaque`; only overwrite known portions.

## 4. Protocol / IR Encoding Requirements
- Each protocol defines: frame length, bit order (LSB/MSB), field mapping, checksum.
- Rule: frame represents complete state; partial updates are forbidden. Learning → resend must not corrupt state.
- IR waveform: carrier (e.g., 38kHz), header mark/space, bit timings, stop bits, repeat behavior. Define timing tolerance (e.g., ±20%).
- ITPS integration: convert Protocol Frame ↔ ITPS for TX/RX; polarity inversion handled in HAL per `SPEC.md`.

## 5. Tests and Samples
- Test vector fields: `name`, `device`, `inputState`, `intents`, `expectedFrameHex`, `expectedCarrierHz`, `notes`. Manage each input/intent → expected frame as a set.
- Sample test vector:
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
- Brand templates (plausible, verify with captures):
  - **Daikin-like**: temp 18–30°C step 1°C; modes auto/cool/heat/dry/fan/off; fan auto+5; swing vertical+horizontal rich; features quiet/turbo/eco/light/beep/clean; light/beep are toggle-only; quiet/turbo are exclusive.
  - **Panasonic-like**: temp 16–30°C step 0.5°C; fan auto+4; swing vertical rich, horizontal off/auto; features eco/nanoe/iFeel/light/sleep; light is toggle-only.
  - **Mitsubishi Electric-like**: temp 16–31°C step 1°C; fan auto+5; swing vertical rich, horizontal off/auto/left/mid/right; features quiet/turbo/eco/iFeel/beep; beep toggle-only; quiet/turbo exclusive.
- In production, confirm Capabilities/TestVectors with real captures or official docs.

## 6. Extensibility & Compatibility
- Always add new fields as optional; keep backward compatibility. Keep unknown data in `opaque` and preserve round-trips.
- Breaking changes only on major version bumps.

## 7. Implementation Notes (ESP32IRPulseCodec)
- Keep the pipeline: Intent/Capabilities → normalized DeviceState → Protocol Frame → ITPS TX. Never drop unknown bits mid-way.
- RX should keep learned RAW (ITPS); even on successful decode, preserve unknown areas via `opaque`.
- Normalize/round using Capabilities; keep compatibility with presets like `useKnownWithoutAC`.
- Public API proposal: `bool esp32ir::decodeAC(const esp32ir::RxResult&, const esp32ir::ac::Capabilities&, esp32ir::ac::DeviceState&)` and `bool esp32ir::Transmitter::sendAC(const esp32ir::ac::DeviceState&, const esp32ir::ac::Capabilities&)` as entry points. Brand encoders/decoders (e.g., `encodeDaikinAC`/`decodeDaikinAC`, `encodePanasonicAC`/`decodePanasonicAC`, …) stay internal helpers that only convert between the common types and ProtocolMessage/ITPS.
