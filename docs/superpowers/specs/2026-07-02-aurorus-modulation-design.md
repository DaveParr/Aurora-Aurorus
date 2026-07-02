# Aurorus: Morphing Chorus / Flanger / Phaser

**Date:** 2026-07-02
**Project:** `aurorus` (replaces `hello-aurora`)

## Overview

Replace the scaffolded `hello-aurora` example with `aurorus`: a single modulation
effect that continuously morphs between chorus, flanger, and phaser character,
built on DaisySP's `Chorus`, `Flanger`, and `Phaser` classes crossfaded together
rather than a from-scratch DSP core.

This is the first "real" firmware in this fork of the Aurora template — `hello-aurora`
is removed as part of this work.

---

## Hardware Context

- **6 knobs:** `KNOB_TIME`, `KNOB_REFLECT`, `KNOB_MIX`, `KNOB_ATMOSPHERE`, `KNOB_BLUR`, `KNOB_WARP`
- **2 usable buttons:** `SW_FREEZE`, `SW_REVERSE` (each with a directly attached LED)
- **11 RGB LEDs:** `LED_1`–`LED_6` (abstract), `LED_FREEZE`, `LED_REVERSE` (attached to
  their buttons), `LED_BOT_1/2/3` (green+blue only, no red channel)
- See `context.md` for physical layout — unchanged by this work

### DaisySP effect APIs (confirmed from `lib/Aurora-SDK/libs/DaisySP/Source/Effects/`)

All three are pure portable C++ (only depend on `dsp.h`, `math.h` — no hardware
headers), so they compile and run identically on host and target.

- **`Chorus`** — stereo-native (two internal engines + `SetPan`). Per-channel or
  combined setters: `SetLfoDepth`, `SetLfoFreq`, `SetDelay`/`SetDelayMs`, `SetFeedback`.
- **`Flanger`** — **mono only**, single `Process(in)` in/out. `SetFeedback`,
  `SetLfoDepth`, `SetLfoFreq`, `SetDelay`/`SetDelayMs`. No phase/direction API.
- **`Phaser`** — **mono only**, single `Process(in)` in/out. `SetPoles` (1–8),
  `SetLfoDepth`, `SetLfoFreq`, `SetFreq` (allpass center), `SetFeedback`. No
  phase/direction API.
- All internal LFOs are free-running triangle waves with no public phase
  read/write — this rules out a literal "freeze at phase X" or "reverse direction"
  implementation; see Buttons section for the practical equivalents used instead.

---

## Architecture

```
aurorus/
├── main.cpp             # hardware wiring only — knobs/buttons/LEDs, no DSP logic
├── modulation.h          # ModulationEngine class declaration
├── modulation.cpp        # ModulationEngine implementation (owns DaisySP instances)
├── blend_colour.h         # pure morph-position -> RGB colour math (LED indicator)
├── Makefile              # same pattern as hello-aurora/Makefile, TARGET = aurorus
└── tests/
    ├── Makefile           # host g++ build; compiles real DaisySP effect sources
    ├── test_modulation.cpp
    └── test_blend_colour.cpp
```

`modulation.h`/`modulation.cpp` have **zero dependency on Aurora or Daisy headers** —
they depend only on DaisySP (itself hardware-free) and operate on plain floats.
`main.cpp` is the only file that touches the Aurora BSP.

This is a stronger split than `hello-aurora` achieved: there, only trivial helper
math (`hsvToRgb`, `scaleVolume`) was host-testable. Here, because DaisySP's effect
classes have no hardware dependency, the **actual DSP engine** runs in host tests —
real `Chorus`/`Flanger`/`Phaser` instances processing real sample blocks, not a
mock or a re-implementation.

---

## DSP Engine (`modulation.h` / `modulation.cpp`)

### Instances

`ModulationEngine` owns:
- 1× `daisysp::Chorus` — stereo built-in
- 2× `daisysp::Flanger` — one per channel (mono-only in DaisySP)
- 2× `daisysp::Phaser` — one per channel (mono-only in DaisySP)

All five are processed every audio block regardless of morph position; only the
crossfaded output changes. This avoids clicks/resets from switching engines in
and out, at a small constant CPU cost (trivial on STM32H750 at this scale).

### Morph crossfade

`KNOB_WARP` (0–1) drives an equal-power (square-root) crossfade across two
adjacent zones, so only two effects are ever audible at once and their combined
gain stays constant through the blend (no volume dip at the midpoints):

- `[0.0, 0.5]`: Chorus → Flanger
- `[0.5, 1.0]`: Flanger → Phaser

At `morph = 0`: pure Chorus. At `morph = 0.5`: pure Flanger. At `morph = 1.0`: pure Phaser.

### Parameter mapping

Applied identically to whichever engines are currently weighted (all five are
always updated with current parameters, but only the crossfaded ones are audible):

| Knob | Parameter | Mapping | DaisySP call |
|------|-----------|---------|---------------|
| `KNOB_WARP` | Morph position | 0–1, drives crossfade zones above | n/a (engine selection) |
| `KNOB_TIME` | LFO rate | exponential curve, ~0.05–5 Hz | `SetLfoFreq(hz)` |
| `KNOB_BLUR` | LFO depth | direct 0–1 passthrough | `SetLfoDepth(depth)` |
| `KNOB_REFLECT` | Feedback | 0–1 scaled to 0–0.9 (stability headroom) | `SetFeedback(fb)` |
| `KNOB_MIX` | Wet/dry | equal-power crossfade at final output stage | n/a (post-mix) |
| `KNOB_ATMOSPHERE` | Stereo width | 0–2% L/R LFO-rate detune, applied uniformly across all three effect types | `SetLfoFreq(freqL, freqR)` on Chorus; separate calls on L/R Flanger/Phaser instances |

Stereo width implementation detail: since Flanger/Phaser expose no phase or pan
API, width is created by giving the right-channel instance a slightly different
LFO rate than the left (`freqR = freq * (1 + width)`), causing the two channels'
modulation to drift gradually in and out of phase — a standard practical
stereo-widening technique for mono modulation effects. The same detune ratio is
applied to Chorus's native per-channel `SetLfoFreq(freqL, freqR)` so all three
effects widen consistently as `KNOB_ATMOSPHERE` increases. At `width = 0`, L and R
run identically (mono-compatible).

### Buttons

- **`SW_FREEZE`** (held): every instance's `SetLfoFreq(0)` is called instead of
  the `KNOB_TIME`-derived rate. Since LFO phase advances by `freq * dt` per
  sample, zero Hz halts the phase exactly where it was — no jump, no click.
  Releasing the button resumes the normal rate from that same phase.
- **`SW_REVERSE`** (held): inverts the wet signal (× -1) after crossfade, before
  the dry/wet mix. This is the classic hardware flanger "through-zero"/polarity
  switch — most audible with `KNOB_REFLECT` feedback engaged, shifting the
  comb/notch character.

### Public interface (sketch)

```cpp
class ModulationEngine
{
  public:
    void Init(float sample_rate);
    void SetMorph(float morph01);
    void SetRate(float rate01);       // maps internally to Hz
    void SetDepth(float depth01);
    void SetFeedback(float fb01);
    void SetMix(float mix01);
    void SetWidth(float width01);
    void SetFreeze(bool freeze);
    void SetReversePolarity(bool reversed);

    StereoFrame Process(StereoFrame in);
};
```

---

## LEDs (`blend_colour.h`)

Pure function, same morph zones as the audio crossfade, blending between three
fixed anchor colours:

| Morph | Colour |
|-------|--------|
| 0.0 | Blue (chorus) |
| 0.5 | Green (flanger) |
| 1.0 | Magenta (phaser) |

```cpp
struct Rgb { float r, g, b; };
Rgb blendColour(float morph01);
```

The resulting colour is written to all of `LED_1`–`LED_6` and `LED_BOT_1`–`3`
uniformly — one "mood colour" for the whole module reflecting the current
character at a glance, replacing `hello-aurora`'s per-knob rainbow mapping.

`LED_FREEZE`/`LED_REVERSE` keep the existing pattern from `hello-aurora`: light
white while `SW_FREEZE`/`SW_REVERSE` is held, giving tactile confirmation of the
freeze/polarity-invert state.

---

## Testing

### Framework

[doctest](https://github.com/doctest/doctest), same as `hello-aurora` — single
header, downloaded at test build time if absent.

### `tests/Makefile`

Same host `g++` pattern as `hello-aurora/tests/Makefile`, extended to compile
DaisySP's effect sources directly against the host (no ARM toolchain, no
pre-built `.a`):

```makefile
DAISYSP_DIR = ../../lib/Aurora-SDK/libs/DaisySP
CXXFLAGS   += -I$(DAISYSP_DIR)/Source -std=c++14
SOURCES     = $(DAISYSP_DIR)/Source/Effects/chorus.cpp \
              $(DAISYSP_DIR)/Source/Effects/flanger.cpp \
              $(DAISYSP_DIR)/Source/Effects/phaser.cpp \
              ../modulation.cpp
```

DaisySP's own `tests/` (which pulls in a `googletest` submodule) is unrelated and
not built — only the three effect `.cpp` files are compiled directly into our
test binaries.

### `test_modulation.cpp` — engine behavior with real DaisySP instances

- `morph = 0.0` → output matches a bare `Chorus` instance (Flanger/Phaser weight ≈ 0)
- `morph = 0.5` → output matches a bare `Flanger` instance (both zone weights meet here)
- `morph = 1.0` → output matches a bare `Phaser` instance
- Crossfade weights sum to constant total power across the full morph sweep
- `SetReversePolarity(true)` inverts the sign of the wet output relative to `false`
- `SetFreeze(true)` held across N processed blocks produces non-progressing
  modulation (output does not continue to sweep) compared to `SetFreeze(false)`

### `test_blend_colour.cpp` — pure colour math

Same style as `hello-aurora`'s `test_colour.cpp`: checks the three anchor colours
at morph 0 / 0.5 / 1.0, plus a mid-blend value in each zone.

---

## Removing `hello-aurora`

- Delete `hello-aurora/` entirely (`main.cpp`, `colour.h`, `audio.h`, `Makefile`, `tests/`)
- `.github/workflows/ci.yml`: `test` job hardcodes `make -C hello-aurora/tests` →
  change to `make -C aurorus/tests`. The `build` job already auto-discovers
  projects by `Makefile` presence — no change needed there.
- `README.md`: replace the `hello-aurora` table row and "Build and flash
  hello-aurora" instructions with `aurorus` equivalents
- `context.md`: unchanged — documents physical hardware, not firmware behavior
- `config.mk` / root `Makefile`: unchanged — already project-name-agnostic via `PROJECT=`

---

## Out of Scope

- CV inputs
- `SW_SHIFT`
- Calibration or persistent storage
- Tap tempo / MIDI sync
- Per-channel independent morph or parameter values (all mappings are global, not L/R-independent, aside from the width detune)
