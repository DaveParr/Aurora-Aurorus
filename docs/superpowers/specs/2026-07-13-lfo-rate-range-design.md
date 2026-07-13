# Widen LFO Rate Range

**Date:** 2026-07-13
**Project:** `aurorus`
**Issue:** [DaveParr/Aurora-Aurorus#1](https://github.com/DaveParr/Aurora-Aurorus/issues/1)

## Overview

The `KNOB_TIME`-driven LFO rate currently spans 0.05–5 Hz (`kMinRateHz` /
`kMaxRateHz` in `modulation.h`). Both ends read as too low: fully CCW is a
sweep so slow it's hard to perceive as moving, and fully CW tops out well
below the "fast/jet" character flangers and phasers are known for. This
change raises both the floor and the ceiling.

## Research: standard rate ranges per effect

Chorus, flanger, and phaser conventionally overlap heavily in practice:

| Effect | Typical range |
|---|---|
| Chorus | ~0.1–5 Hz (slow, subtle) |
| Flanger | ~0.05–10 Hz (can go faster, "jet" sweeps) |
| Phaser | ~0.1–10 Hz (sometimes to 20 Hz for fast, vibrato-like sweeps) |

Because the three ranges overlap substantially, and because this engine
already cross-fades chorus/flanger/phaser continuously via the morph knob
(not a discrete selector), giving each effect its own rate range was
considered and rejected: it would require interpolating rate range alongside
morph weight, adding complexity for a benefit only noticeable at the extremes,
and risking an audible rate discontinuity while morphing between effect
types at a fixed knob position. A single shared range stays architecturally
consistent with how morphing already works (see
`docs/superpowers/specs/2026-07-02-aurorus-modulation-design.md`) and remains
musically valid for all three effects.

## Change

In `aurorus/modulation.h`:

```cpp
constexpr float kMinRateHz = 0.1f;   // was 0.05f
constexpr float kMaxRateHz = 10.0f;  // was 5.0f
```

`MapRate01ToHz` keeps its existing exponential curve shape — only the
endpoints change:

```cpp
inline float MapRate01ToHz(float rate01)
{
    return kMinRateHz * std::pow(kMaxRateHz / kMinRateHz, rate01);
}
```

No other code changes: `ModulationEngine::UpdateRates()` and
`led_breath.h`'s breathing-rate calculation both consume
`MapRate01ToHz`'s output and need no changes themselves.

## Side effect: LED breathing rate

`led_breath.h` reuses `MapRate01ToHz` to derive the LED breathing rate from
the same `KNOB_TIME` value (see `docs/superpowers/specs/2026-07-02-led-breath-design.md`).
Widening the range means the LED breathing ceiling also rises from an
effective 5 Hz to 10 Hz equivalent at full CW.

This is accepted as intended behavior: the LED is meant to mirror the audio
LFO, and keeping one shared mapping function is simpler and more consistent
than introducing a second, decoupled range solely for the LED.

## Testing

`aurorus/tests/test_modulation.cpp` currently exercises `SetRate` at a few
mid-range values (0.5, 0.8) but never pins down the Hz bounds themselves.
Add boundary assertions:

- `MapRate01ToHz(0.0f)` ≈ `0.1f`
- `MapRate01ToHz(1.0f)` ≈ `10.0f`

These guard the two constants directly so a future accidental edit to either
value is caught.

## Documentation

- Update the `KNOB_TIME` row in
  `docs/superpowers/specs/2026-07-02-aurorus-modulation-design.md` (currently
  reads "exponential curve, ~0.05–5 Hz") to reflect the new range.

## Out of scope

- Per-effect rate ranges (considered, rejected — see Research section above).
- Any change to the exponential curve shape, `kMaxDetune`, or the
  freeze/width behavior in `UpdateRates()`.
- Extending the ceiling into true audio-rate (>20 Hz) territory — not
  requested by the issue and would change the effect's character
  fundamentally (ring-mod-like) rather than just making it "faster."
