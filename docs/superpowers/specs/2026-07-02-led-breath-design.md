# LED breathing: representing modulation rate and depth

## Problem

All 9 non-button LEDs (`LED_1`–`6`, `LED_BOT_1`–`3`) currently show one static
"mood colour" driven only by `KNOB_WARP` (morph position between
chorus/flanger/phaser, via `blendColour()`). `KNOB_TIME` (modulation rate) and
`KNOB_BLUR` (modulation depth) have no visual representation at all — you have
to listen to hear what they're doing.

## Goal

Add a real-time brightness pulse on top of the existing morph colour:

- **TIME** (rate) sets how fast the LEDs breathe — using the same 0.05–5 Hz
  exponential curve the audio engine uses (`MapRate01ToHz`), so the pulse
  speed matches what you'd expect from turning that knob.
- **BLUR** (depth, i.e. "strength") sets how deep the pulse swings — at
  depth 0 the LEDs hold rock-solid at the morph colour (today's behavior,
  unchanged); at depth 1 the brightness swings the full range, dipping to
  near-black at the bottom of each breath.

Non-goals: this does not touch `REFLECT` (feedback), `MIX`, or `ATMOSPHERE`
(width) — those remain inaudible-in-LEDs, as today. `LED_FREEZE` and
`LED_REVERSE` keep their existing solid-white-while-held behavior, unchanged.

## Architecture

The breathing phase is generated **independently in the UI loop**, not read
from `ModulationEngine`. Two reasons:

1. `ModulationEngine` has no single canonical LFO phase to read — chorus,
   flanger (L/R), and phaser (L/R) are crossfaded by morph and independently
   detuned by width, so "the" phase is ambiguous once those controls are in
   play.
2. `main.cpp` is documented as hardware-wiring-only, with no DSP logic — the
   existing `ModulationEngine` boundary stays audio-thread-only, and the UI
   loop stays free of any coupling to it.

The independent phase is driven by the *same* rate-mapping function the audio
engine uses (`MapRate01ToHz` from `modulation.h`), so the visual pulse tracks
the same 0.05–5 Hz curve you hear, without reading any audio-thread state.

### New file: `aurorus/led_breath.h`

Pure, hardware-free functions, following the existing `blend_colour.h`
pattern (no dependencies beyond `<cmath>` and `MapRate01ToHz`):

```cpp
#pragma once
#include <cmath>
#include "modulation.h"  // MapRate01ToHz

constexpr float kTwoPi = 6.2831853f;
constexpr float kLedUpdateIntervalMs = 10.f;  // 100 Hz UI/LED loop cadence

// Advances the breathing phase by dt_seconds at the rate implied by rate01.
// Wraps the result into [0, kTwoPi). Callers skip this call while frozen.
inline float AdvancePhase(float phase, float rate01, float dt_seconds);

// Brightness multiplier in [1 - depth01, 1] for a given phase.
// depth01 == 0  -> always 1 (steady, no visible pulse).
// depth01 == 1  -> swings the full [0, 1] range (near-black at the trough).
inline float BreathBrightness(float phase, float depth01);
```

`BreathBrightness` shape: `1.f - depth01 * (0.5f - 0.5f * std::sin(phase))`.
At `phase = pi/2` (peak) this is `1` regardless of depth. At `phase = 3*pi/2`
(trough) this is `1 - depth01`.

### `main.cpp` changes

- Add `float breath_phase = 0.f;` before the UI loop.
- Each iteration, after the existing `hw.ClearLeds()`:
  - `float rate01 = hw.GetKnobValue(KNOB_TIME);`
  - `float depth01 = hw.GetKnobValue(KNOB_BLUR);`
  - If `!hw.GetButton(SW_FREEZE).Pressed()`: advance `breath_phase` via
    `AdvancePhase(breath_phase, rate01, kLedUpdateIntervalMs / 1000.f)`.
    (Frozen: phase holds, matching the audio engine holding its own LFO
    phase during freeze.)
  - Compute `brightness = BreathBrightness(breath_phase, depth01)`.
  - Multiply the existing `blendColour(...)` result's r/g/b by `brightness`
    before writing to the arc and bottom LEDs. (`LED_BOT_*` already silently
    discard the red channel in hardware — no special-casing needed.)
  - Replace the current flat-out loop with a fixed cadence:
    `System::Delay(kLedUpdateIntervalMs)` at the end of each iteration, using
    the constant from `led_breath.h` (100 Hz) — smooth relative to the 5 Hz
    max rate, and stops the loop from spinning/writing the LED bus needlessly
    fast.
- `AudioCallback` and `ModulationEngine` are untouched.

## Testing

New `aurorus/tests/test_led_breath.cpp` (doctest, host-built, same pattern as
`test_blend_colour.cpp`):

- `BreathBrightness` at `depth01 = 0` is `1.0` at every phase.
- `BreathBrightness` at `depth01 = 1` is `1.0` at the peak and `~0.0` at the
  trough.
- `BreathBrightness` at `depth01 = 0.5` swings between `1.0` and `0.5`.
- `AdvancePhase` wraps correctly past `2*pi`.
- `AdvancePhase` at `rate01 = 0` (min rate, 0.05 Hz) advances by a tiny
  fraction of a radian per 10ms tick; at `rate01 = 1` (max rate, 5 Hz)
  advances by a larger, correctly-scaled amount.

`aurorus/tests/Makefile` gets a new target for `test_led_breath`, mirroring
the existing `test_blend_colour`/`test_modulation` targets. `.gitignore`
already anchors `**/tests/test_*` patterns generically enough to need one
new line for the new binary (`**/tests/test_led_breath`).

## Open assumption for review

The architecture choice (independent LED-loop oscillator vs. reading a live
phase from `ModulationEngine`) was decided by the author's judgment call
after the design's brainstorming session went unanswered on this specific
question — the independent-oscillator approach was recommended throughout
and is consistent with every other answer given (real-time animation,
breathing-pulse style, depth-as-full-swing). Flag if a tighter audio sync is
actually wanted; see "Architecture" above for the tradeoff.
