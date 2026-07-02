# Aurorus: Knob-to-CV Wiring

**Date:** 2026-07-02
**Project:** `aurorus`

## Overview

Wire five of `aurorus`'s six knob-controlled parameters (rate, depth, feedback, mix, stereo width) to their corresponding CV inputs, so each can be modulated by an external CV source in addition to the front-panel knob. `KNOB_WARP` (morph position) stays knob-only.

---

## Hardware Context

The Aurora SDK (`lib/Aurora-SDK/include/aurora.h`) defines a CV input for every knob, with matching names:

```cpp
enum ControlKnobs { KNOB_TIME, KNOB_REFLECT, KNOB_MIX, KNOB_ATMOSPHERE, KNOB_BLUR, KNOB_WARP, KNOB_LAST };
enum ControlCVs   { CV_ATMOSPHERE, CV_TIME, CV_MIX, CV_REFLECT, CV_BLUR, CV_WARP, CV_LAST };
```

`hw.ProcessAllControls()` — already called once per audio block in `aurorus/main.cpp` — updates both knobs and CVs together (`ProcessAnalogControls()` iterates `KNOB_LAST` then `CV_LAST`), so no additional plumbing is needed beyond reading the new values.

`hw.GetCvValue(cv_idx)` returns a calibrated, offset-corrected bipolar value (roughly -1.0 to +1.0 for a ±5V input) for every CV **except** `CV_WARP`. Per the SDK's own doc comment:

> gets calibrated offset-adjusted CV Value from the hardware... (except for warp which is v/oct calibrated)... when returning warp CV from this function it will be with no calibrated offset, and is identical to reading from the AnalogControl itself.

The SDK also exposes a dedicated `hw.GetWarpVoct()`, returning a MIDI note number (-60 to 60) from a -5V to 5V input — clearly intended for 1V/octave pitch tracking, not a linear 0-1 position offset. Since `KNOB_WARP` drives `aurorus`'s morph position (a linear sweep, not a pitch), neither `GetWarpVoct()` nor the uncalibrated `GetCvValue(CV_WARP)` is a good fit, so `CV_WARP` is left unused.

### Precedent: Aurora-Morse

The sibling project `Aurora-Morse` (same author, same hardware) already wires `KNOB_TIME`/`CV_TIME` this way, in `aurora-morse/main.cpp`:

```cpp
float position = std::max(0.0f, std::min(1.0f,
    hw.GetKnobValue(KNOB_TIME) + hw.GetCvValue(CV_TIME)));
```

This is the additive-offset-with-clamp idiom `aurorus` adopts: the knob sets a center point, the CV input adds a bipolar offset around it, and the sum is clamped to the `[0, 1]` range every one of `ModulationEngine`'s setters expects.

---

## Behavior

For each of the five wired parameters: `final = clamp(knob01 + cv, 0, 1)`.

- The knob sets the center; CV swings the parameter around it.
- No attenuverting, no headroom scaling, no jack-presence detection (the SDK exposes none). This matches Aurora-Morse's existing behavior exactly, and standard behavior for a plain, non-attenuverting Eurorack CV input generally.
- Turning the knob toward the center (0.5) gives CV more room to swing before clipping at 0 or 1; turning it toward an extreme leaves less room in that direction. This is expected, standard modular-synth behavior, not a defect to design around.
- With nothing patched into a CV jack, the calibrated offset-correction means `GetCvValue` reads ~0, so the parameter behaves exactly as it does today (knob-only).

---

## Mapping

| Knob | CV | `ModulationEngine` setter | Parameter |
|------|-----|---------------------------|-----------|
| `KNOB_TIME` | `CV_TIME` | `SetRate` | Modulation rate |
| `KNOB_BLUR` | `CV_BLUR` | `SetDepth` | Modulation depth |
| `KNOB_REFLECT` | `CV_REFLECT` | `SetFeedback` | Feedback |
| `KNOB_MIX` | `CV_MIX` | `SetMix` | Wet/dry mix |
| `KNOB_ATMOSPHERE` | `CV_ATMOSPHERE` | `SetWidth` | Stereo width |
| `KNOB_WARP` | — (unused) | `SetMorph` | Morph position |

---

## Architecture

```
aurorus/modulation.h    # + Clamp01(float) -> float, pure, host-tested
aurorus/main.cpp        # AudioCallback: 5 of 6 SetX(...) calls gain "+ hw.GetCvValue(CV_X)", wrapped in Clamp01
aurorus/tests/test_modulation.cpp  # + Clamp01 test cases
README.md               # CV Inputs table updated
```

### `Clamp01` (`aurorus/modulation.h`)

```cpp
inline float Clamp01(float x)
{
    if (x < 0.f) return 0.f;
    if (x > 1.f) return 1.f;
    return x;
}
```

A small, pure, hardware-free function alongside the module's existing pure helpers (`MapRate01ToHz`, `MapFeedback01`). Host-tested with doctest: below 0, above 1, in-range, and the exact boundary values 0.0 and 1.0.

### `main.cpp` changes

In `AudioCallback`, five of the six `engine.SetX(hw.GetKnobValue(KNOB_X))` calls become:

```cpp
engine.SetRate(Clamp01(hw.GetKnobValue(KNOB_TIME) + hw.GetCvValue(CV_TIME)));
engine.SetDepth(Clamp01(hw.GetKnobValue(KNOB_BLUR) + hw.GetCvValue(CV_BLUR)));
engine.SetFeedback(Clamp01(hw.GetKnobValue(KNOB_REFLECT) + hw.GetCvValue(CV_REFLECT)));
engine.SetMix(Clamp01(hw.GetKnobValue(KNOB_MIX) + hw.GetCvValue(CV_MIX)));
engine.SetWidth(Clamp01(hw.GetKnobValue(KNOB_ATMOSPHERE) + hw.GetCvValue(CV_ATMOSPHERE)));
```

`engine.SetMorph(hw.GetKnobValue(KNOB_WARP));` is unchanged.

`main.cpp` remains untested wiring, matching the project's existing convention — the only new testable logic is `Clamp01` itself.

---

## Testing

Added to `aurorus/tests/test_modulation.cpp` (same file, same doctest/host-`g++` harness already in place):

| Input | Expected |
|-------|----------|
| `-0.3` | `0.0` (clamped below) |
| `1.4` | `1.0` (clamped above) |
| `0.6` | `0.6` (unchanged, in range) |
| `0.0` | `0.0` (exact lower boundary) |
| `1.0` | `1.0` (exact upper boundary) |

No changes to `ModulationEngine`'s public interface or any existing test — `Clamp01` is additive, and `main.cpp`'s wiring change has no host-testable surface of its own (verified only by the existing ARM build check).

---

## Documentation

`README.md`'s "CV Inputs" table changes from:

```markdown
| Input | Function |
|-------|----------|
| Warp, Time, Blur, Reflect, Mix, Atmosphere | Unused |
```

to:

```markdown
| Input | Function |
|-------|----------|
| Time | Added to the Time knob (rate) |
| Blur | Added to the Blur knob (depth) |
| Reflect | Added to the Reflect knob (feedback) |
| Mix | Added to the Mix knob (wet/dry) |
| Atmosphere | Added to the Atmosphere knob (stereo width) |
| Warp | Unused — CV_WARP is calibrated for 1V/oct pitch tracking on this hardware, not a plain offset, so it doesn't fit a morph-position parameter |
```

The "Freeze and Reverse gate inputs are also unused" line is unchanged — this feature doesn't touch gates.

---

## Out of Scope

- Attenuverting or scaling CV amount (no free knob exists to drive one, and it wasn't requested)
- Jack-presence detection / CV-override mode (no SDK support exists for this)
- `CV_WARP` / `GetWarpVoct()` usage
- Gate inputs (`GATE_FREEZE`, `GATE_REVERSE`) — unchanged, still unused
