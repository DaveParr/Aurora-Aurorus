# Aurorus: Knob-to-CV Wiring

**Date:** 2026-07-02
**Project:** `aurorus`

## Overview

Wire all six of `aurorus`'s knob-controlled parameters (morph, rate, depth, feedback, mix, stereo width) to their corresponding CV inputs, so each can be modulated by an external CV source in addition to the front-panel knob.

**Revision (2026-07-02, after hardware testing):** the original version of this spec left `KNOB_WARP`/`CV_WARP` knob-only, because the SDK calibrates `CV_WARP` differently from the other five (see Hardware Context below). After testing the first version on real hardware, the CV_WARP jack was expected to modulate morph the same way the other five CVs modulate their knobs. This revision wires it in too, accepting the calibration quirk as a minor, cosmetic imperfection rather than a blocker — see the note in Hardware Context.

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

The SDK also exposes a dedicated `hw.GetWarpVoct()`, returning a MIDI note number (-60 to 60) from a -5V to 5V input — clearly intended for 1V/octave pitch tracking, not a linear 0-1 position offset. Since `KNOB_WARP` drives `aurorus`'s morph position (a linear sweep, not a pitch), `GetWarpVoct()` is not used.

**Decision (revised):** `GetCvValue(CV_WARP)` — the uncalibrated raw value — is used anyway, with the same `clamp(knob + cv, 0, 1)` idiom as the other five. Skipping the offset calibration means an unpatched `CV_WARP` jack may read a small nonzero DC value instead of exactly 0 (the other five CVs are calibrated to read ~0 when unpatched). In practice this is a minor, cosmetic imperfection: the same as countless uncalibrated Eurorack CV inputs elsewhere, correctable by ear (nudge the Warp knob slightly to recenter) rather than a functional defect.

### Precedent: Aurora-Morse

The sibling project `Aurora-Morse` (same author, same hardware) already wires `KNOB_TIME`/`CV_TIME` this way, in `aurora-morse/main.cpp`:

```cpp
float position = std::max(0.0f, std::min(1.0f,
    hw.GetKnobValue(KNOB_TIME) + hw.GetCvValue(CV_TIME)));
```

This is the additive-offset-with-clamp idiom `aurorus` adopts: the knob sets a center point, the CV input adds a bipolar offset around it, and the sum is clamped to the `[0, 1]` range every one of `ModulationEngine`'s setters expects.

---

## Behavior

For each of the six wired parameters: `final = clamp(knob01 + cv, 0, 1)`.

- The knob sets the center; CV swings the parameter around it.
- No attenuverting, no headroom scaling, no jack-presence detection (the SDK exposes none). This matches Aurora-Morse's existing behavior exactly, and standard behavior for a plain, non-attenuverting Eurorack CV input generally.
- Turning the knob toward the center (0.5) gives CV more room to swing before clipping at 0 or 1; turning it toward an extreme leaves less room in that direction. This is expected, standard modular-synth behavior, not a defect to design around.
- For the five non-Warp CVs, with nothing patched into a CV jack, the calibrated offset-correction means `GetCvValue` reads ~0, so the parameter behaves exactly as it does today (knob-only). `CV_WARP` is uncalibrated (see Hardware Context) so an unpatched jack may carry a small nonzero offset instead of exactly 0.

**Warp's LED indicator tracks the combined (post-CV) value, not just the knob.** `aurorus`'s LEDs already show a blended colour driven by the Warp value (`blendColour(hw.GetKnobValue(KNOB_WARP))` in the existing code). Now that `KNOB_WARP` can be pushed by CV, the LED must use the same `clamp(knob + cv, 0, 1)` value the audio engine uses — otherwise the LED would show only the knob position while CV silently pushes the actual morph further, making the "mood indicator" LED misleading. Both `engine.SetMorph(...)` (in `AudioCallback`, audio-rate) and `blendColour(...)` (in the main LED loop, a separate, slower loop) independently compute `Clamp01(hw.GetKnobValue(KNOB_WARP) + hw.GetCvValue(CV_WARP))` — matching the existing architecture where both loops already independently re-read `hw.GetKnobValue(KNOB_WARP)` rather than sharing cached state across the audio/main-loop boundary.

---

## Mapping

| Knob | CV | `ModulationEngine` setter | Parameter |
|------|-----|---------------------------|-----------|
| `KNOB_TIME` | `CV_TIME` | `SetRate` | Modulation rate |
| `KNOB_BLUR` | `CV_BLUR` | `SetDepth` | Modulation depth |
| `KNOB_REFLECT` | `CV_REFLECT` | `SetFeedback` | Feedback |
| `KNOB_MIX` | `CV_MIX` | `SetMix` | Wet/dry mix |
| `KNOB_ATMOSPHERE` | `CV_ATMOSPHERE` | `SetWidth` | Stereo width |
| `KNOB_WARP` | `CV_WARP` | `SetMorph` (and `blendColour` for the LED) | Morph position |

---

## Architecture

```
aurorus/modulation.h    # + Clamp01(float) -> float, pure, host-tested
aurorus/main.cpp        # AudioCallback + LED loop: all 6 SetX(...)/blendColour(...) calls gain "+ hw.GetCvValue(CV_X)", wrapped in Clamp01
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

In `AudioCallback`, all six `engine.SetX(hw.GetKnobValue(KNOB_X))` calls become:

```cpp
engine.SetMorph(Clamp01(hw.GetKnobValue(KNOB_WARP) + hw.GetCvValue(CV_WARP)));
engine.SetRate(Clamp01(hw.GetKnobValue(KNOB_TIME) + hw.GetCvValue(CV_TIME)));
engine.SetDepth(Clamp01(hw.GetKnobValue(KNOB_BLUR) + hw.GetCvValue(CV_BLUR)));
engine.SetFeedback(Clamp01(hw.GetKnobValue(KNOB_REFLECT) + hw.GetCvValue(CV_REFLECT)));
engine.SetMix(Clamp01(hw.GetKnobValue(KNOB_MIX) + hw.GetCvValue(CV_MIX)));
engine.SetWidth(Clamp01(hw.GetKnobValue(KNOB_ATMOSPHERE) + hw.GetCvValue(CV_ATMOSPHERE)));
```

In the main LED loop, `blendColour(hw.GetKnobValue(KNOB_WARP))` becomes:

```cpp
Rgb c = blendColour(Clamp01(hw.GetKnobValue(KNOB_WARP) + hw.GetCvValue(CV_WARP)));
```

`main.cpp` remains untested wiring, matching the project's existing convention — the only new testable logic is `Clamp01` itself (already added and tested).

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

`README.md`'s "CV Inputs" table currently (after the first version of this feature) reads:

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

It changes to:

```markdown
| Input | Function |
|-------|----------|
| Warp | Added to the Warp knob (morph position); the LED blend colour also reflects the combined value |
| Time | Added to the Time knob (rate) |
| Blur | Added to the Blur knob (depth) |
| Reflect | Added to the Reflect knob (feedback) |
| Mix | Added to the Mix knob (wet/dry) |
| Atmosphere | Added to the Atmosphere knob (stereo width) |
```

The "Freeze and Reverse gate inputs are also unused" line is unchanged — this feature doesn't touch gates.

---

## Out of Scope

- Attenuverting or scaling CV amount (no free knob exists to drive one, and it wasn't requested)
- Jack-presence detection / CV-override mode (no SDK support exists for this)
- `GetWarpVoct()` usage (the SDK's V/oct-calibrated accessor for `CV_WARP` — the uncalibrated `GetCvValue(CV_WARP)` is used instead, per the revised decision above)
- Correcting `CV_WARP`'s uncalibrated offset in firmware (accepted as a minor, cosmetic imperfection rather than solved with custom calibration logic)
- Gate inputs (`GATE_FREEZE`, `GATE_REVERSE`) — unchanged, still unused
