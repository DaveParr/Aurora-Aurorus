# LED Breathing (rate + depth visualization) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Aurora's LEDs breathe — a real-time brightness pulse layered on the existing morph colour — so TIME (rate) and BLUR (depth) are visible, not just audible.

**Architecture:** An independent phase accumulator lives in `main.cpp`'s UI loop (not read from `ModulationEngine`, which has no single canonical LFO phase once morph/width are in play). It advances using the same `MapRate01ToHz` curve the audio engine uses, and its brightness output is multiplied onto the existing `blendColour()` result before writing to all 9 non-button LEDs. Freezing holds the phase, mirroring the audio engine's freeze behavior.

**Tech Stack:** C++14, DaisySP, doctest (host tests), GNU Make, ARM GCC (firmware cross-compile).

## Global Constraints

- `led_breath.h` must be pure and hardware-free (no `daisy`/`aurora` includes) so it host-tests the same way `blend_colour.h` does — see `docs/superpowers/specs/2026-07-02-led-breath-design.md`.
- Depth 0 must produce brightness `1.0` at every phase (today's static-colour behavior, unchanged) — depth 1 must swing the full `[0, 1]` range.
- The LED update cadence is `kLedUpdateIntervalMs = 10` (100 Hz), defined once in `led_breath.h` and reused by `main.cpp`.
- `main.cpp` stays hardware-wiring-only — no DSP logic added there.
- `SW_FREEZE` held ⇒ breath phase does not advance. `SW_REVERSE` has no LED effect.

---

### Task 1: `led_breath.h` — pure rate/depth breathing math

**Files:**
- Create: `aurorus/led_breath.h`
- Create: `aurorus/tests/test_led_breath.cpp`
- Modify: `aurorus/tests/Makefile`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: `MapRate01ToHz(float rate01) -> float` from `aurorus/modulation.h` (already exists, `inline`, no linkage required).
- Produces (for Task 2):
  - `constexpr float kLedUpdateIntervalMs` (10.f)
  - `float AdvancePhase(float phase, float rate01, float dt_seconds)` — returns new phase wrapped into `[0, kTwoPi)`.
  - `float BreathBrightness(float phase, float depth01)` — returns a brightness multiplier in `[1 - depth01, 1]`.

- [ ] **Step 1: Write the failing test file**

Create `aurorus/tests/test_led_breath.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../led_breath.h"

static const float kEps     = 1e-3f;
static const float kHalfPi  = 1.5707963f;
static const float kThreeHalfPi = 4.7123890f;

TEST_CASE("BreathBrightness - depth 0 is steady at 1.0 regardless of phase") {
    CHECK(BreathBrightness(0.f, 0.f) == doctest::Approx(1.f).epsilon(kEps));
    CHECK(BreathBrightness(kHalfPi, 0.f) == doctest::Approx(1.f).epsilon(kEps));
    CHECK(BreathBrightness(kThreeHalfPi, 0.f) == doctest::Approx(1.f).epsilon(kEps));
}

TEST_CASE("BreathBrightness - depth 1.0 peaks at 1.0 and troughs near 0.0") {
    CHECK(BreathBrightness(kHalfPi, 1.f) == doctest::Approx(1.f).epsilon(kEps));
    CHECK(BreathBrightness(kThreeHalfPi, 1.f) == doctest::Approx(0.f).epsilon(kEps));
}

TEST_CASE("BreathBrightness - depth 0.5 swings between 1.0 and 0.5") {
    CHECK(BreathBrightness(kHalfPi, 0.5f) == doctest::Approx(1.f).epsilon(kEps));
    CHECK(BreathBrightness(kThreeHalfPi, 0.5f) == doctest::Approx(0.5f).epsilon(kEps));
}

TEST_CASE("AdvancePhase - min rate (0.05Hz) advances a small fraction per second") {
    float phase = AdvancePhase(0.f, 0.f, 1.f);
    CHECK(phase == doctest::Approx(0.31415927f).epsilon(kEps));
}

TEST_CASE("AdvancePhase - max rate (5Hz) wraps past 2*pi correctly") {
    // 5Hz * 0.9s = 4.5 cycles -> wraps to exactly pi radians past a full cycle.
    float phase = AdvancePhase(0.f, 1.f, 0.9f);
    CHECK(phase == doctest::Approx(3.14159265f).epsilon(kEps));
    CHECK(phase >= 0.f);
    CHECK(phase < 6.2831853f);
}
```

- [ ] **Step 2: Wire the new test into the host test Makefile**

Edit `aurorus/tests/Makefile`. Change:

```makefile
all: $(DOCTEST_H) test_modulation test_blend_colour
	./test_modulation
	./test_blend_colour
```

to:

```makefile
all: $(DOCTEST_H) test_modulation test_blend_colour test_led_breath
	./test_modulation
	./test_blend_colour
	./test_led_breath
```

Add a new build rule directly below the existing `test_blend_colour` rule:

```makefile
test_led_breath: test_led_breath.cpp ../led_breath.h ../modulation.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ test_led_breath.cpp
```

Change the `clean` target from:

```makefile
clean:
	rm -f test_modulation test_blend_colour $(DOCTEST_H)
```

to:

```makefile
clean:
	rm -f test_modulation test_blend_colour test_led_breath $(DOCTEST_H)
```

- [ ] **Step 3: Add the new test binary to `.gitignore`**

In `.gitignore`, under the `# Host test artifacts` section, add a line after `**/tests/test_blend_colour`:

```
**/tests/test_led_breath
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `make -C aurorus/tests test_led_breath`
Expected: FAIL — `../led_breath.h: No such file or directory` (the header doesn't exist yet).

- [ ] **Step 5: Write `led_breath.h`**

Create `aurorus/led_breath.h`:

```cpp
#pragma once

#include <cmath>

#include "modulation.h" // MapRate01ToHz

constexpr float kTwoPi = 6.2831853f;
constexpr float kLedUpdateIntervalMs = 10.f; // 100 Hz UI/LED loop cadence

// Advances the breathing phase by dt_seconds at the rate implied by rate01,
// using the same curve the audio engine uses for its own LFO rate. Wraps
// into [0, kTwoPi). Callers should skip this call while frozen.
inline float AdvancePhase(float phase, float rate01, float dt_seconds)
{
    float hz = MapRate01ToHz(rate01);
    return std::fmod(phase + kTwoPi * hz * dt_seconds, kTwoPi);
}

// Brightness multiplier in [1 - depth01, 1] for a given phase.
// depth01 == 0 -> always 1 (steady, no visible pulse).
// depth01 == 1 -> swings the full [0, 1] range (near-black at the trough).
inline float BreathBrightness(float phase, float depth01)
{
    return 1.f - depth01 * (0.5f - 0.5f * std::sin(phase));
}
```

- [ ] **Step 6: Run all host tests to verify everything passes**

Run: `make -C aurorus/tests`
Expected: PASS — all `test_modulation`, `test_blend_colour`, and `test_led_breath` cases succeed, no failures.

- [ ] **Step 7: Commit**

```bash
git add aurorus/led_breath.h aurorus/tests/test_led_breath.cpp aurorus/tests/Makefile .gitignore
git commit -m "Add led_breath.h: pure rate/depth breathing math"
```

---

### Task 2: Wire breathing into `main.cpp` and document it

**Files:**
- Modify: `aurorus/main.cpp`
- Modify: `README.md:14`, `README.md:67-68`

**Interfaces:**
- Consumes: `kLedUpdateIntervalMs`, `AdvancePhase`, `BreathBrightness` from `aurorus/led_breath.h` (Task 1). `blendColour(float morph01) -> Rgb` from `aurorus/blend_colour.h` (existing).
- Produces: nothing consumed by later tasks — this is the final integration point.

- [ ] **Step 1: Add the `led_breath.h` include**

In `aurorus/main.cpp`, change:

```cpp
#include "aurora.h"
#include "modulation.h"
#include "blend_colour.h"
```

to:

```cpp
#include "aurora.h"
#include "modulation.h"
#include "blend_colour.h"
#include "led_breath.h"
```

- [ ] **Step 2: Add the breath phase variable and rewrite the UI loop**

In `aurorus/main.cpp`, change:

```cpp
    const Leds numberedLeds[6] = { LED_1, LED_2, LED_3, LED_4, LED_5, LED_6 };
    const Leds bottomLeds[3]   = { LED_BOT_1, LED_BOT_2, LED_BOT_3 };

    while (1)
    {
        hw.ClearLeds();

        Rgb c = blendColour(hw.GetKnobValue(KNOB_WARP));

        for (int i = 0; i < 6; i++)
            hw.SetLed(numberedLeds[i], c.r, c.g, c.b);

        for (int i = 0; i < 3; i++)
            hw.SetLed(bottomLeds[i], c.r, c.g, c.b);

        if (hw.GetButton(SW_FREEZE).Pressed())
            hw.SetLed(LED_FREEZE, 1.f, 1.f, 1.f);

        if (hw.GetButton(SW_REVERSE).Pressed())
            hw.SetLed(LED_REVERSE, 1.f, 1.f, 1.f);

        hw.WriteLeds();
    }
```

to:

```cpp
    const Leds numberedLeds[6] = { LED_1, LED_2, LED_3, LED_4, LED_5, LED_6 };
    const Leds bottomLeds[3]   = { LED_BOT_1, LED_BOT_2, LED_BOT_3 };

    float breath_phase = 0.f;

    while (1)
    {
        hw.ClearLeds();

        float rate01  = hw.GetKnobValue(KNOB_TIME);
        float depth01 = hw.GetKnobValue(KNOB_BLUR);
        bool  frozen  = hw.GetButton(SW_FREEZE).Pressed();

        if (!frozen)
            breath_phase = AdvancePhase(breath_phase, rate01, kLedUpdateIntervalMs / 1000.f);

        float brightness = BreathBrightness(breath_phase, depth01);

        Rgb c = blendColour(hw.GetKnobValue(KNOB_WARP));
        c.r *= brightness;
        c.g *= brightness;
        c.b *= brightness;

        for (int i = 0; i < 6; i++)
            hw.SetLed(numberedLeds[i], c.r, c.g, c.b);

        for (int i = 0; i < 3; i++)
            hw.SetLed(bottomLeds[i], c.r, c.g, c.b);

        if (frozen)
            hw.SetLed(LED_FREEZE, 1.f, 1.f, 1.f);

        if (hw.GetButton(SW_REVERSE).Pressed())
            hw.SetLed(LED_REVERSE, 1.f, 1.f, 1.f);

        hw.WriteLeds();

        System::Delay(static_cast<uint32_t>(kLedUpdateIntervalMs));
    }
```

Note: `frozen` replaces the previous inline `hw.GetButton(SW_FREEZE).Pressed()` call inside the `if` below — it's the same read, just reused instead of called twice.

- [ ] **Step 3: Update the module doc comment**

In `aurorus/main.cpp`, change the top comment block from:

```cpp
/** aurorus
 *
 *  Morphing chorus / flanger / phaser modulation effect.
 *  KNOB_WARP morphs chorus -> flanger -> phaser.
 *  KNOB_TIME, KNOB_BLUR, KNOB_REFLECT, KNOB_MIX, KNOB_ATMOSPHERE drive
 *  rate, depth, feedback, wet/dry mix, and stereo width.
 *  SW_FREEZE holds the current modulation phase.
 *  SW_REVERSE inverts the wet signal polarity.
 */
```

to:

```cpp
/** aurorus
 *
 *  Morphing chorus / flanger / phaser modulation effect.
 *  KNOB_WARP morphs chorus -> flanger -> phaser.
 *  KNOB_TIME, KNOB_BLUR, KNOB_REFLECT, KNOB_MIX, KNOB_ATMOSPHERE drive
 *  rate, depth, feedback, wet/dry mix, and stereo width.
 *  SW_FREEZE holds the current modulation phase.
 *  SW_REVERSE inverts the wet signal polarity.
 *  The LEDs breathe at KNOB_TIME's rate, with KNOB_BLUR setting how deep
 *  the pulse swings; SW_FREEZE holds the breath too.
 */
```

- [ ] **Step 4: Update `README.md`**

Change line 14 from:

```markdown
- **LED mood indicator** — all LEDs show one blended colour (blue → green → magenta) reflecting where you are in the morph
```

to:

```markdown
- **LED mood indicator** — all LEDs show one blended colour (blue → green → magenta) reflecting where you are in the morph, breathing in brightness at the modulation rate and depth
```

Change the LEDs table (lines 65-70) from:

```markdown
| LED | Behaviour |
|-----|-----------|
| Arc (1–6) | All six show the same blended colour, tracking the Warp position: blue at full chorus, green at full flanger, magenta at full phaser, blending smoothly in between. |
| Bottom LEDs | Mirror the same blend colour as the arc LEDs. |
| Freeze LED | Lights solid white while Freeze is held. |
| Reverse LED | Lights solid white while Reverse is held. |
```

to:

```markdown
| LED | Behaviour |
|-----|-----------|
| Arc (1–6) | All six show the same blended colour, tracking the Warp position: blue at full chorus, green at full flanger, magenta at full phaser, blending smoothly in between. Brightness breathes at the Time rate; Blur sets how deep the breath swings — 0 is rock steady, fully up dips to near-black at the bottom of each breath. |
| Bottom LEDs | Mirror the same blend colour and breathing as the arc LEDs. |
| Freeze LED | Lights solid white while Freeze is held. |
| Reverse LED | Lights solid white while Reverse is held; holding Freeze also holds the breath in place. |
```

- [ ] **Step 5: Verify host tests still pass**

Run: `make -C aurorus/tests`
Expected: PASS — unchanged from Task 1 (this task doesn't touch host-tested code).

- [ ] **Step 6: Cross-compile the firmware to verify `main.cpp` builds for real hardware**

Run:
```bash
git submodule update --init --recursive
make libdaisy
make build PROJECT=aurorus
```
Expected: PASS. `make libdaisy` builds libDaisy/DaisySP from scratch on first run and may take several minutes; `make build PROJECT=aurorus` should then succeed and produce `aurorus/build/aurorus-dev.bin` with no compiler errors or warnings about `led_breath.h`/`main.cpp`.

- [ ] **Step 7: Commit**

```bash
git add aurorus/main.cpp README.md
git commit -m "Wire LED breathing (rate + depth) into main.cpp"
```

---

## Self-Review Notes

- **Spec coverage:** Independent-oscillator architecture (Task 1+2), `MapRate01ToHz` reuse (Task 1), depth 0 = steady / depth 1 = full swing (Task 1 tests), freeze holds phase (Task 2 step 2), `kLedUpdateIntervalMs = 10` fixed cadence (Task 1 constant, Task 2 `System::Delay` call), host-testable pure header following `blend_colour.h` pattern (Task 1), README documentation (Task 2 step 4) — all covered.
- **Placeholder scan:** none found.
- **Type consistency:** `AdvancePhase(float, float, float) -> float` and `BreathBrightness(float, float) -> float` are used identically in Task 1's tests and Task 2's `main.cpp` wiring. `kLedUpdateIntervalMs` is a single `constexpr float` defined once in Task 1 and consumed as-is (with an explicit `static_cast<uint32_t>` at the one call site that needs an integer) in Task 2 — no duplicate constants.
