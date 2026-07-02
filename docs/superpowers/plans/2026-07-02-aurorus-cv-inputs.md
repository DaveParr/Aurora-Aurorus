# Aurorus Knob-to-CV Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire five of `aurorus`'s six knob-controlled parameters (rate, depth, feedback, mix, stereo width) to their matching CV inputs, using additive-offset-with-clamp — the same idiom the sibling `Aurora-Morse` project already uses for its Time CV.

**Architecture:** A new pure `Clamp01(float)` function in `aurorus/modulation.h` clamps the sum of a knob value and its CV value to `[0, 1]`. `aurorus/main.cpp`'s `AudioCallback` wraps five of its six `engine.SetX(hw.GetKnobValue(KNOB_X))` calls with `Clamp01(hw.GetKnobValue(KNOB_X) + hw.GetCvValue(CV_X))`. `KNOB_WARP` is unchanged (no CV — its CV input is SDK-calibrated for V/oct pitch tracking, not a linear offset).

**Tech Stack:** C++14, DaisySP/libDaisy/Aurora BSP (`lib/Aurora-SDK`), doctest for host tests, GNU Make, `gcc-arm-none-eabi` for the target build.

## Global Constraints

- `Clamp01` is pure and hardware-free — it belongs in `aurorus/modulation.h` alongside the module's existing pure helpers (`MapRate01ToHz`, `MapFeedback01`), not in `main.cpp`.
- Behavior is `final = clamp(knob01 + cv, 0, 1)` — plain additive offset, no attenuation, no headroom scaling, no jack-presence detection. This matches `Aurora-Morse`'s existing `aurora-morse/main.cpp:125-126` idiom exactly.
- `KNOB_WARP`/`CV_WARP` are explicitly out of scope — `main.cpp`'s `engine.SetMorph(hw.GetKnobValue(KNOB_WARP));` line is not touched.
- `main.cpp` remains untested wiring (matches the existing project convention where hardware glue is verified by a successful ARM build, not host tests).
- `hw.ProcessAllControls()` (already called once per `AudioCallback` in `main.cpp`) updates both knobs and CVs — no new plumbing is needed beyond reading the CV values.

---

## Task 1: `Clamp01` pure function + host tests

**Files:**
- Modify: `aurorus/modulation.h`
- Modify: `aurorus/tests/test_modulation.cpp`

**Interfaces:**
- Consumes: nothing new (pure function, no dependency on `ModulationEngine` or DaisySP).
- Produces: `float Clamp01(float x);` — used by Task 2's `main.cpp` changes.

- [ ] **Step 1: Write the failing tests**

Add to the end of `aurorus/tests/test_modulation.cpp` (after the existing test cases, before end of file):

```cpp
TEST_CASE("Clamp01 - clamps below zero") {
    CHECK(Clamp01(-0.3f) == doctest::Approx(0.0f).epsilon(kEps));
}

TEST_CASE("Clamp01 - clamps above one") {
    CHECK(Clamp01(1.4f) == doctest::Approx(1.0f).epsilon(kEps));
}

TEST_CASE("Clamp01 - passes through an in-range value unchanged") {
    CHECK(Clamp01(0.6f) == doctest::Approx(0.6f).epsilon(kEps));
}

TEST_CASE("Clamp01 - exact lower boundary") {
    CHECK(Clamp01(0.0f) == doctest::Approx(0.0f).epsilon(kEps));
}

TEST_CASE("Clamp01 - exact upper boundary") {
    CHECK(Clamp01(1.0f) == doctest::Approx(1.0f).epsilon(kEps));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: FAIL to compile — `Clamp01` was not declared in this scope.

- [ ] **Step 3: Implement the minimal code to make the tests pass**

In `aurorus/modulation.h`, add `Clamp01` immediately after `MapFeedback01` (which currently ends the block of pure helper functions, right before the `class ModulationEngine` declaration):

```cpp
inline float MapFeedback01(float fb01)
{
    return fb01 * kMaxFeedback;
}

inline float Clamp01(float x)
{
    if (x < 0.f) return 0.f;
    if (x > 1.f) return 1.f;
    return x;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: all test cases pass, including the 5 new `Clamp01` cases (17 total test cases in `test_modulation`, up from 12).

- [ ] **Step 5: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git add aurorus/modulation.h aurorus/tests/test_modulation.cpp
git commit -m "Add Clamp01 helper for knob+CV summing in aurorus"
```

---

## Task 2: Wire main.cpp to CV inputs + update README

**Files:**
- Modify: `aurorus/main.cpp`
- Modify: `README.md`

**Interfaces:**
- Consumes: `Clamp01(float) -> float` from Task 1.

This task has no host unit tests — it is hardware wiring, verified by a successful ARM build, matching the project's existing convention for `main.cpp` changes.

- [ ] **Step 1: Update `aurorus/main.cpp`**

Replace the `AudioCallback` function body. The current version:

```cpp
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();

    engine.SetMorph(hw.GetKnobValue(KNOB_WARP));
    engine.SetRate(hw.GetKnobValue(KNOB_TIME));
    engine.SetDepth(hw.GetKnobValue(KNOB_BLUR));
    engine.SetFeedback(hw.GetKnobValue(KNOB_REFLECT));
    engine.SetMix(hw.GetKnobValue(KNOB_MIX));
    engine.SetWidth(hw.GetKnobValue(KNOB_ATMOSPHERE));
    engine.SetFreeze(hw.GetButton(SW_FREEZE).Pressed());
    engine.SetReversePolarity(hw.GetButton(SW_REVERSE).Pressed());

    for (size_t i = 0; i < size; i++)
    {
        StereoFrame frame = engine.Process({in[0][i], in[1][i]});
        out[0][i] = frame.left;
        out[1][i] = frame.right;
    }
}
```

becomes:

```cpp
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();

    engine.SetMorph(hw.GetKnobValue(KNOB_WARP));
    engine.SetRate(Clamp01(hw.GetKnobValue(KNOB_TIME) + hw.GetCvValue(CV_TIME)));
    engine.SetDepth(Clamp01(hw.GetKnobValue(KNOB_BLUR) + hw.GetCvValue(CV_BLUR)));
    engine.SetFeedback(Clamp01(hw.GetKnobValue(KNOB_REFLECT) + hw.GetCvValue(CV_REFLECT)));
    engine.SetMix(Clamp01(hw.GetKnobValue(KNOB_MIX) + hw.GetCvValue(CV_MIX)));
    engine.SetWidth(Clamp01(hw.GetKnobValue(KNOB_ATMOSPHERE) + hw.GetCvValue(CV_ATMOSPHERE)));
    engine.SetFreeze(hw.GetButton(SW_FREEZE).Pressed());
    engine.SetReversePolarity(hw.GetButton(SW_REVERSE).Pressed());

    for (size_t i = 0; i < size; i++)
    {
        StereoFrame frame = engine.Process({in[0][i], in[1][i]});
        out[0][i] = frame.left;
        out[1][i] = frame.right;
    }
}
```

`engine.SetMorph(hw.GetKnobValue(KNOB_WARP));` is unchanged — no CV. Nothing else in `main.cpp` changes.

- [ ] **Step 2: Update the doc comment at the top of `main.cpp`**

The current top-of-file comment:

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

becomes:

```cpp
/** aurorus
 *
 *  Morphing chorus / flanger / phaser modulation effect.
 *  KNOB_WARP morphs chorus -> flanger -> phaser.
 *  KNOB_TIME, KNOB_BLUR, KNOB_REFLECT, KNOB_MIX, KNOB_ATMOSPHERE drive
 *  rate, depth, feedback, wet/dry mix, and stereo width. Each of these
 *  five has a matching CV input, summed with its knob and clamped to
 *  0-1 (CV_WARP is left unused - it's SDK-calibrated for V/oct pitch
 *  tracking, not a linear offset).
 *  SW_FREEZE holds the current modulation phase.
 *  SW_REVERSE inverts the wet signal polarity.
 */
```

- [ ] **Step 3: Confirm the DaisySP submodule and pre-built libs are present**

```bash
cd /home/dave/Development/Aurora-Aurorus
ls lib/Aurora-SDK/libs/libDaisy/build/libdaisy.a lib/Aurora-SDK/libs/DaisySP/build/libdaisysp.a
```

If either is missing:

```bash
make libdaisy
make -C lib/Aurora-SDK/libs/DaisySP GCC_PATH=$(grep '^GCC_PATH' config.mk | head -1 | cut -d= -f2 | tr -d ' ?')
```

- [ ] **Step 4: Build the firmware to verify it compiles and links**

```bash
cd /home/dave/Development/Aurora-Aurorus
rm -rf aurorus/build
make build PROJECT=aurorus
```

Expected: build succeeds, producing `aurorus/build/aurorus-dev.bin`. DTCMRAM usage should be unchanged from before this task (91.75%, 120,264 bytes) — this change adds no new persistent state to `ModulationEngine`, only local reads in `AudioCallback`.

- [ ] **Step 5: Update `README.md`**

In the `### CV Inputs` section, replace:

```markdown
### CV Inputs

| Input | Function |
|-------|----------|
| Warp, Time, Blur, Reflect, Mix, Atmosphere | Unused |

The Freeze and Reverse gate inputs are also unused — both controls are front-panel/button only.
```

with:

```markdown
### CV Inputs

| Input | Function |
|-------|----------|
| Time | Added to the Time knob (rate) |
| Blur | Added to the Blur knob (depth) |
| Reflect | Added to the Reflect knob (feedback) |
| Mix | Added to the Mix knob (wet/dry) |
| Atmosphere | Added to the Atmosphere knob (stereo width) |
| Warp | Unused — CV_WARP is calibrated for 1V/oct pitch tracking on this hardware, not a plain offset, so it doesn't fit a morph-position parameter |

Each CV input sums with its knob and clamps to the 0-1 parameter range: the knob sets the center, CV swings the parameter around it. Turning a knob toward center leaves more room for CV to swing before it clips at either end; turning it toward an extreme leaves less. The Freeze and Reverse gate inputs remain unused — both controls are front-panel/button only.
```

- [ ] **Step 6: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git add aurorus/main.cpp README.md
git commit -m "Wire aurorus knobs to their CV inputs"
```

---

## Self-Review Notes

- **Spec coverage:** mapping table (Task 2 Step 1), behavior/clamp semantics (Task 1 + Task 2 Step 1), Warp exclusion (Task 2 Step 1, unchanged line), doc comment update (Task 2 Step 2), testing (Task 1), README update (Task 2 Step 5) — every spec section has a corresponding step.
- **Placeholder scan:** none found — every step has complete, exact code.
- **Type consistency:** `Clamp01(float) -> float` is declared identically in Task 1 (production code) and consumed identically in Task 2 (`main.cpp` call sites) — no signature drift.
