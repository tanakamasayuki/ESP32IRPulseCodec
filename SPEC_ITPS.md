# ITPS Specification

## 1. Purpose
ITPS (Intermediate Timed Pulse Sequence) is a **platform/board-agnostic** intermediate format to represent IR waveforms (Mark/Space time sequences).  
Hardware-specific differences (timers, capture/tx peripherals, etc.) are absorbed in the implementation-dependent layer (HAL) right before RX/TX. Codecs take ITPS as input/output to keep portability.

## 2. Data Model
IR data is expressed as an **array of frames** (frame boundaries are kept by the array; ITPS itself has no delimiter tokens).  
Serialization (byte array / JSON / text, etc.) is out of scope; names/layout are up to the implementation.

### 2.1 ITPSFrame Structure
ITPSFrame holds:

- `uint16_t T_us`  
  Time per count (microseconds).
- `uint16_t len`  
  Number of elements in `seq[]`.
- `int8_t seq[len]`  
  Sequence of Mark/Space segments. Sign = polarity, abs = duration.
- `uint8_t flags`  
  Extension metadata. Bit/value meaning is implementation-defined; use 0 if unused.

#### Handling of T_us
- `T_us` is mandatory and scales `seq` counts to actual time (us).
- For ITPS, `T_us` is assumed **common within the frame array**. Missing or inconsistent values are not allowed.
- `T_us` itself has no logical meaning; decode/encode should convert to real time for judgment.
- Selection guideline (reference): RX quantization is typically 1–10us. For long AC waveforms, ~5us; for short remotes, near 1us. If a protocol defines a resolution (e.g., NEC TX), use that. Smaller `T_us` reduces error but increases data length; larger reduces length but increases error. Do not over-optimize for size.
- Time range: use at least 32-bit unsigned for `abs(seq[i]) * T_us` and frame totals. Even with hundreds of ms to seconds per frame, `T_us≈5us` fits well in 32-bit.

### 2.2 Frame Boundaries and Gaps
- One frame represents a “contiguous Mark/Space sequence”; boundaries are maintained by the array structure.
- Optionally include a trailing Space for guard/delimiter (no restriction on ending polarity). Boundary/gap handling is left to the producer.
- Boundary hints: split at long Spaces (gap-like), separate protocol repeat/retransmit sections, etc. ITPS itself is agnostic to how you split.

### 2.3 Ownership and Placement
- `seq` is treated as read-only; ITPS consumers must not modify it.
- `seq` storage may be RAM/ROM/flash. Lifetime management is the producer’s responsibility.

## 3. Meaning of seq[]
Each `seq[i]` represents one interval.

- `seq[i] > 0`: Mark (carrier ON / IR LED on)
- `seq[i] < 0`: Space (carrier OFF / IR LED off)
- Interval time (us) is `abs(seq[i]) * T_us`

## 4. Normalization Guarantees
ITPSFrame is treated as a **normalized canonical form** so decoders/encoders can work easily and 8-bit constraints handle long data safely.

A normalized ITPSFrame satisfies:

1. **Value range**
   - `seq[i] != 0`
   - `1 <= abs(seq[i]) <= 127`
   - Within `int8_t` range; do not use -128 (max abs is 127)

2. **Start polarity**
   - `seq[0] > 0` (always starts with Mark)
   - If RX raw starts with Space, drop the leading meaningless Space during ITPS conversion to unify on Mark start.

3. **Consecutive same-sign handling (8-bit constraint)**
   - Same-sign segments (Mark-Mark or Space-Space) may appear, to allow **±127 splitting** for long durations.
   - However, **remove meaningless splits**:
     - If adjacent same-sign elements are both below 127, merge into one.
     - If the merged abs exceeds 127, split into ±127 chunks.
   - A value of 127 may be followed by a polarity change; 127 does not guarantee continuity. When reconstructing time, check neighbor signs to see if they should be concatenated.

4. **Input assumptions**
   - Time quantization (`T_us` decision) and micro-noise removal are assumed to be done before ITPS conversion.

5. **Invalid conditions**
   - Invalid if `len == 0`, `seq` is null, or `T_us == 0`.
   - Invalid if `seq[0] <= 0`, `abs(seq[i]) > 127`, or `seq[i] == 0`.
   - `len` should fit at least in `uint16_t`; implementations may use wider types for longer frames (exact limits are implementation-defined).

## 5. flags Field
- `flags` is an 8-bit extension meta field. It is independent of the normalization rules (Mark-first, polarity, trailing Space). Use 0 if unused.
- Bits or values may be used as needed; this spec does not define bit positions/names (implementation-defined).

## 6. Handling of Inversion
Inversion is **not an ITPS attribute**.  
Handle it in the layer just before RX/TX (HAL, etc.).

- ITPS always guarantees “Mark = positive, Space = negative”.
- If you need to record inversion, keep it as external metadata (caller context), not inside ITPS.
