# Aurorus Modulation Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the `hello-aurora` scaffold with `aurorus`, a firmware that morphs continuously between chorus, flanger, and phaser using DaisySP's `Chorus`/`Flanger`/`Phaser` classes crossfaded by `KNOB_WARP`.

**Architecture:** A hardware-free `ModulationEngine` (`aurorus/modulation.h`/`.cpp`) owns one `daisysp::Chorus`, two `daisysp::Flanger` (L/R), and two `daisysp::Phaser` (L/R) instances, crossfading their outputs by morph position. A hardware-free `blend_colour.h` maps morph position to an LED colour. `aurorus/main.cpp` is wiring only — reads knobs/buttons, drives the engine, writes LEDs.

**Tech Stack:** C++14, DaisySP (`lib/Aurora-SDK/libs/DaisySP`), libDaisy/Aurora BSP (`lib/Aurora-SDK`), doctest for host tests, GNU Make, `gcc-arm-none-eabi` for the target build.

## Global Constraints

- Host tests compile with plain `g++ -std=c++14 -Wall`, no ARM toolchain (matches `hello-aurora/tests` convention).
- `modulation.h`/`.cpp` and `blend_colour.h` must not include any Aurora/Daisy hardware header — only DaisySP and the standard library. `main.cpp` is the only file that includes `aurora.h`.
- Feedback sent to any DaisySP effect is capped at `0.9` (`MapFeedback01`) regardless of knob position, for stability headroom.
- Morph and mix both use equal-power (square-root) crossfade curves, not linear.
- LFO rate range is `0.05`–`5.0` Hz, mapped exponentially from the `0..1` knob value.
- Stereo width detune is `±1%` of rate per channel (`kMaxDetune = 0.01`), giving up to 2% L/R spread at maximum width.
- Every new source file follows the existing repo convention: no namespaces, plain structs/free functions for pure logic (matches `hello-aurora/colour.h`/`audio.h` style).
- The Aurora-SDK submodule (including nested `DaisySP`/`libDaisy` submodules) must be checked out for any task that touches DaisySP headers or does an ARM build: `git submodule update --init --recursive lib/Aurora-SDK`.
- The phaser leg uses 4 manually-managed `daisysp::PhaserEngine` poles per channel (`kPhaserPoles = 4`), not DaisySP's `Phaser` wrapper class — the wrapper always allocates a fixed 8-pole array regardless of runtime `SetPoles()`, which overflows the STM32H750's 128KB DTCMRAM budget when combined with the rest of the engine. See Task 6's memory-footprint fix for the full rationale.

---

## Task 1: Morph crossfade weights + host test harness

**Files:**
- Create: `aurorus/modulation.h`
- Create: `aurorus/tests/Makefile`
- Create: `aurorus/tests/test_modulation.cpp`

**Interfaces:**
- Produces: `struct MorphWeights { float chorus, flanger, phaser; };` and `MorphWeights ComputeMorphWeights(float morph01);` — pure function, no DaisySP dependency yet. Later tasks extend this same header with `ModulationEngine`.

- [ ] **Step 1: Create the project and tests directories**

```bash
mkdir -p /home/dave/Development/Aurora-Aurorus/aurorus/tests
```

- [ ] **Step 2: Write the failing test**

Create `aurorus/tests/test_modulation.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../modulation.h"

static const float kEps = 1e-4f;

TEST_CASE("morph weights at chorus anchor (0.0)") {
    MorphWeights w = ComputeMorphWeights(0.0f);
    CHECK(w.chorus  == doctest::Approx(1.0f).epsilon(kEps));
    CHECK(w.flanger == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(w.phaser  == doctest::Approx(0.0f).epsilon(kEps));
}

TEST_CASE("morph weights at flanger anchor (0.5)") {
    MorphWeights w = ComputeMorphWeights(0.5f);
    CHECK(w.chorus  == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(w.flanger == doctest::Approx(1.0f).epsilon(kEps));
    CHECK(w.phaser  == doctest::Approx(0.0f).epsilon(kEps));
}

TEST_CASE("morph weights at phaser anchor (1.0)") {
    MorphWeights w = ComputeMorphWeights(1.0f);
    CHECK(w.chorus  == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(w.flanger == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(w.phaser  == doctest::Approx(1.0f).epsilon(kEps));
}

TEST_CASE("morph weights preserve equal power across the full sweep") {
    // Equal-power (square-root) crossfade: the sum of the SQUARES of the
    // two active weights is 1, not their linear sum (which peaks above 1
    // at the midpoint of each zone - that's what avoids the volume dip).
    for (float m = 0.0f; m <= 1.0f; m += 0.05f) {
        MorphWeights w = ComputeMorphWeights(m);
        float sumOfSquares = w.chorus * w.chorus + w.flanger * w.flanger + w.phaser * w.phaser;
        CHECK(sumOfSquares == doctest::Approx(1.0f).epsilon(kEps));
        CHECK(w.chorus  >= 0.0f);
        CHECK(w.flanger >= 0.0f);
        CHECK(w.phaser  >= 0.0f);
    }
}

TEST_CASE("morph weights: only adjacent effects are non-zero in each zone") {
    MorphWeights low = ComputeMorphWeights(0.25f);   // chorus/flanger zone
    CHECK(low.phaser == doctest::Approx(0.0f).epsilon(kEps));

    MorphWeights high = ComputeMorphWeights(0.75f);  // flanger/phaser zone
    CHECK(high.chorus == doctest::Approx(0.0f).epsilon(kEps));
}
```

- [ ] **Step 3: Create the test Makefile**

Create `aurorus/tests/Makefile`:

```makefile
CXX      = g++
CXXFLAGS = -std=c++14 -Wall -I..

DOCTEST_URL = https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h
DOCTEST_H   = doctest.h

all: $(DOCTEST_H) test_modulation
	./test_modulation

$(DOCTEST_H):
	curl -sSL $(DOCTEST_URL) -o $@

test_modulation: test_modulation.cpp ../modulation.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ test_modulation.cpp

clean:
	rm -f test_modulation $(DOCTEST_H)
```

- [ ] **Step 4: Run the tests to verify they fail**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: FAIL — `../modulation.h: No such file or directory`.

- [ ] **Step 5: Implement the minimal code to make the tests pass**

Create `aurorus/modulation.h`:

```cpp
#pragma once

#include <cmath>

struct MorphWeights { float chorus, flanger, phaser; };

inline MorphWeights ComputeMorphWeights(float morph01)
{
    MorphWeights w;
    if (morph01 <= 0.5f)
    {
        float t   = morph01 / 0.5f;
        w.chorus  = std::sqrt(1.f - t);
        w.flanger = std::sqrt(t);
        w.phaser  = 0.f;
    }
    else
    {
        float t   = (morph01 - 0.5f) / 0.5f;
        w.chorus  = 0.f;
        w.flanger = std::sqrt(1.f - t);
        w.phaser  = std::sqrt(t);
    }
    return w;
}
```

- [ ] **Step 6: Run the tests to verify they pass**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: all 5 test cases pass, binary exits 0.

- [ ] **Step 7: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git add aurorus/modulation.h aurorus/tests/Makefile aurorus/tests/test_modulation.cpp
git commit -m "Add morph crossfade weight function for aurorus"
```

---

## Task 2: Blend colour LED indicator

**Files:**
- Create: `aurorus/blend_colour.h`
- Create: `aurorus/tests/test_blend_colour.cpp`
- Modify: `aurorus/tests/Makefile`

**Interfaces:**
- Produces: `struct Rgb { float r, g, b; };` and `Rgb blendColour(float morph01);`

- [ ] **Step 1: Write the failing test**

Create `aurorus/tests/test_blend_colour.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../blend_colour.h"

static const float kEps = 1e-4f;

TEST_CASE("blendColour - pure chorus (blue) at morph 0.0") {
    Rgb c = blendColour(0.f);
    CHECK(c.r == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(1.f).epsilon(kEps));
}

TEST_CASE("blendColour - pure flanger (green) at morph 0.5") {
    Rgb c = blendColour(0.5f);
    CHECK(c.r == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(1.f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(0.f).epsilon(kEps));
}

TEST_CASE("blendColour - pure phaser (magenta) at morph 1.0") {
    Rgb c = blendColour(1.f);
    CHECK(c.r == doctest::Approx(1.f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(1.f).epsilon(kEps));
}

TEST_CASE("blendColour - halfway between chorus and flanger at morph 0.25") {
    Rgb c = blendColour(0.25f);
    CHECK(c.r == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(0.5f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(0.5f).epsilon(kEps));
}

TEST_CASE("blendColour - halfway between flanger and phaser at morph 0.75") {
    Rgb c = blendColour(0.75f);
    CHECK(c.r == doctest::Approx(0.5f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(0.5f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(0.5f).epsilon(kEps));
}
```

- [ ] **Step 2: Update the test Makefile**

Update `aurorus/tests/Makefile`:

```makefile
CXX      = g++
CXXFLAGS = -std=c++14 -Wall -I..

DOCTEST_URL = https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h
DOCTEST_H   = doctest.h

all: $(DOCTEST_H) test_modulation test_blend_colour
	./test_modulation
	./test_blend_colour

$(DOCTEST_H):
	curl -sSL $(DOCTEST_URL) -o $@

test_modulation: test_modulation.cpp ../modulation.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ test_modulation.cpp

test_blend_colour: test_blend_colour.cpp ../blend_colour.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f test_modulation test_blend_colour $(DOCTEST_H)
```

- [ ] **Step 3: Run the tests to verify the new test fails**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: FAIL — `../blend_colour.h: No such file or directory`.

- [ ] **Step 4: Implement the minimal code to make the tests pass**

Create `aurorus/blend_colour.h`:

```cpp
#pragma once

struct Rgb { float r, g, b; };

inline Rgb lerp(const Rgb &a, const Rgb &b, float t)
{
    return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

inline Rgb blendColour(float morph01)
{
    constexpr Rgb kChorus  = {0.f, 0.f, 1.f}; // blue
    constexpr Rgb kFlanger = {0.f, 1.f, 0.f}; // green
    constexpr Rgb kPhaser  = {1.f, 0.f, 1.f}; // magenta

    if (morph01 <= 0.5f)
        return lerp(kChorus, kFlanger, morph01 / 0.5f);

    return lerp(kFlanger, kPhaser, (morph01 - 0.5f) / 0.5f);
}
```

- [ ] **Step 5: Run the tests to verify they pass**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: both `test_modulation` and `test_blend_colour` pass.

- [ ] **Step 6: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git add aurorus/blend_colour.h aurorus/tests/Makefile aurorus/tests/test_blend_colour.cpp
git commit -m "Add morph-position LED blend colour for aurorus"
```

---

## Task 3: ModulationEngine core — chorus/flanger/phaser + morph + mix

This task wires the real DaisySP effects into a `ModulationEngine` class, crossfaded by the `MorphWeights` from Task 1, with an equal-power wet/dry mix. Freeze, reverse polarity, and stereo width are NOT included yet (added in later tasks).

**Files:**
- Modify: `aurorus/modulation.h`
- Create: `aurorus/modulation.cpp`
- Modify: `aurorus/tests/test_modulation.cpp`
- Modify: `aurorus/tests/Makefile`

**Interfaces:**
- Consumes: `MorphWeights`, `ComputeMorphWeights(float)` from Task 1.
- Produces:
  - `struct StereoFrame { float left, right; };`
  - `constexpr float kMinRateHz = 0.05f;`, `constexpr float kMaxRateHz = 5.0f;`, `constexpr float kMaxFeedback = 0.9f;`
  - `float MapRate01ToHz(float rate01);`
  - `float MapFeedback01(float fb01);`
  - `class ModulationEngine` with `void Init(float sample_rate);`, `void SetMorph(float morph01);`, `void SetRate(float rate01);`, `void SetDepth(float depth01);`, `void SetFeedback(float fb01);`, `void SetMix(float mix01);`, `StereoFrame Process(StereoFrame in);` — later tasks add `SetWidth`, `SetFreeze`, `SetReversePolarity`.

- [ ] **Step 1: Confirm the DaisySP submodule is checked out**

```bash
cd /home/dave/Development/Aurora-Aurorus
git submodule update --init --recursive lib/Aurora-SDK
ls lib/Aurora-SDK/libs/DaisySP/Source/Effects/chorus.h
```

Expected: the file exists (clones the SDK and its nested `DaisySP`/`libDaisy` submodules if not already present).

- [ ] **Step 2: Write the failing tests**

Add to the end of `aurorus/tests/test_modulation.cpp` (keep the existing `MorphWeights` tests above):

```cpp
namespace
{
float TestSignal(int i)
{
    constexpr float kTwoPi = 6.28318530718f;
    return 0.5f * std::sin(kTwoPi * i / 20.f);
}
}

TEST_CASE("dry passthrough when mix is fully dry") {
    ModulationEngine engine;
    engine.Init(48000.f);
    engine.SetMorph(0.0f);
    engine.SetMix(0.0f);

    StereoFrame out = engine.Process({0.42f, -0.17f});
    CHECK(out.left  == doctest::Approx(0.42f).epsilon(kEps));
    CHECK(out.right == doctest::Approx(-0.17f).epsilon(kEps));
}

TEST_CASE("wet signal is engaged when mix is fully wet") {
    ModulationEngine engine;
    engine.Init(48000.f);
    engine.SetMorph(0.0f); // pure chorus
    engine.SetRate(0.5f);
    engine.SetDepth(1.0f);
    engine.SetFeedback(0.2f);
    engine.SetMix(1.0f);

    StereoFrame out = engine.Process({0.5f, 0.5f});
    CHECK(out.left != doctest::Approx(0.5f).epsilon(kEps));
}

TEST_CASE("morph selects distinctly different effects at each anchor") {
    ModulationEngine chorusOnly, flangerOnly, phaserOnly;
    for (ModulationEngine *e : {&chorusOnly, &flangerOnly, &phaserOnly})
    {
        e->Init(48000.f);
        e->SetMix(1.0f);
        e->SetRate(0.5f);
        e->SetDepth(1.0f);
        e->SetFeedback(0.3f);
    }
    chorusOnly.SetMorph(0.0f);
    flangerOnly.SetMorph(0.5f);
    phaserOnly.SetMorph(1.0f);

    StereoFrame c{0.f, 0.f}, f{0.f, 0.f}, p{0.f, 0.f};
    for (int i = 0; i < 200; i++)
    {
        float x = TestSignal(i);
        c = chorusOnly.Process({x, x});
        f = flangerOnly.Process({x, x});
        p = phaserOnly.Process({x, x});
    }

    CHECK(c.left != doctest::Approx(f.left).epsilon(kEps));
    CHECK(f.left != doctest::Approx(p.left).epsilon(kEps));
}
```

Also add `#include <initializer_list>` near the top of the file (needed for the `for (ModulationEngine *e : {&a, &b, &c})` loop form), and update the `#include "../modulation.h"` line to stay as-is — it now also declares `ModulationEngine`.

The top of `aurorus/tests/test_modulation.cpp` should now read:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <initializer_list>
#include "../modulation.h"

static const float kEps = 1e-4f;
```

- [ ] **Step 3: Update the test Makefile to compile the real DaisySP sources**

Update `aurorus/tests/Makefile`:

```makefile
CXX         = g++
DAISYSP_DIR = ../../lib/Aurora-SDK/libs/DaisySP
CXXFLAGS    = -std=c++14 -Wall -I.. -I$(DAISYSP_DIR)/Source -I$(DAISYSP_DIR)/Source/Utility

DOCTEST_URL = https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h
DOCTEST_H   = doctest.h

DAISYSP_SOURCES = $(DAISYSP_DIR)/Source/Effects/chorus.cpp \
                   $(DAISYSP_DIR)/Source/Effects/flanger.cpp \
                   $(DAISYSP_DIR)/Source/Effects/phaser.cpp

all: $(DOCTEST_H) test_modulation test_blend_colour
	./test_modulation
	./test_blend_colour

$(DOCTEST_H):
	curl -sSL $(DOCTEST_URL) -o $@

test_modulation: test_modulation.cpp ../modulation.cpp ../modulation.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ test_modulation.cpp ../modulation.cpp $(DAISYSP_SOURCES)

test_blend_colour: test_blend_colour.cpp ../blend_colour.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f test_modulation test_blend_colour $(DOCTEST_H)
```

- [ ] **Step 4: Run the tests to verify they fail**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: FAIL to compile — `ModulationEngine` not declared, `../modulation.cpp` missing.

- [ ] **Step 5: Implement the minimal code to make the tests pass**

Replace `aurorus/modulation.h` with:

```cpp
#pragma once

#include <cmath>

#include "Effects/chorus.h"
#include "Effects/flanger.h"
#include "Effects/phaser.h"

struct StereoFrame { float left, right; };

struct MorphWeights { float chorus, flanger, phaser; };

constexpr float kMinRateHz   = 0.05f;
constexpr float kMaxRateHz   = 5.0f;
constexpr float kMaxFeedback = 0.9f;

inline MorphWeights ComputeMorphWeights(float morph01)
{
    MorphWeights w;
    if (morph01 <= 0.5f)
    {
        float t   = morph01 / 0.5f;
        w.chorus  = std::sqrt(1.f - t);
        w.flanger = std::sqrt(t);
        w.phaser  = 0.f;
    }
    else
    {
        float t   = (morph01 - 0.5f) / 0.5f;
        w.chorus  = 0.f;
        w.flanger = std::sqrt(1.f - t);
        w.phaser  = std::sqrt(t);
    }
    return w;
}

inline float MapRate01ToHz(float rate01)
{
    return kMinRateHz * std::pow(kMaxRateHz / kMinRateHz, rate01);
}

inline float MapFeedback01(float fb01)
{
    return fb01 * kMaxFeedback;
}

class ModulationEngine
{
  public:
    void Init(float sample_rate);

    void SetMorph(float morph01);
    void SetRate(float rate01);
    void SetDepth(float depth01);
    void SetFeedback(float fb01);
    void SetMix(float mix01);

    StereoFrame Process(StereoFrame in);

  private:
    daisysp::Chorus  chorus_;
    daisysp::Flanger flangerL_;
    daisysp::Flanger flangerR_;
    daisysp::Phaser  phaserL_;
    daisysp::Phaser  phaserR_;

    MorphWeights weights_ = {1.f, 0.f, 0.f};
    float        rate01_  = 0.f;
    float        mix01_   = 0.f;

    void UpdateRates();
};
```

Create `aurorus/modulation.cpp`:

```cpp
#include "modulation.h"

void ModulationEngine::Init(float sample_rate)
{
    chorus_.Init(sample_rate);
    flangerL_.Init(sample_rate);
    flangerR_.Init(sample_rate);
    phaserL_.Init(sample_rate);
    phaserR_.Init(sample_rate);
}

void ModulationEngine::SetMorph(float morph01)
{
    weights_ = ComputeMorphWeights(morph01);
}

void ModulationEngine::SetRate(float rate01)
{
    rate01_ = rate01;
    UpdateRates();
}

void ModulationEngine::SetDepth(float depth01)
{
    chorus_.SetLfoDepth(depth01);
    flangerL_.SetLfoDepth(depth01);
    flangerR_.SetLfoDepth(depth01);
    phaserL_.SetLfoDepth(depth01);
    phaserR_.SetLfoDepth(depth01);
}

void ModulationEngine::SetFeedback(float fb01)
{
    float fb = MapFeedback01(fb01);
    chorus_.SetFeedback(fb);
    flangerL_.SetFeedback(fb);
    flangerR_.SetFeedback(fb);
    phaserL_.SetFeedback(fb);
    phaserR_.SetFeedback(fb);
}

void ModulationEngine::SetMix(float mix01)
{
    mix01_ = mix01;
}

void ModulationEngine::UpdateRates()
{
    float rateHz = MapRate01ToHz(rate01_);
    chorus_.SetLfoFreq(rateHz);
    flangerL_.SetLfoFreq(rateHz);
    flangerR_.SetLfoFreq(rateHz);
    phaserL_.SetLfoFreq(rateHz);
    phaserR_.SetLfoFreq(rateHz);
}

StereoFrame ModulationEngine::Process(StereoFrame in)
{
    float mono = (in.left + in.right) * 0.5f;

    chorus_.Process(mono);
    float chorusL = chorus_.GetLeft();
    float chorusR = chorus_.GetRight();

    float flangerL = flangerL_.Process(in.left);
    float flangerR = flangerR_.Process(in.right);

    float phaserL = phaserL_.Process(in.left);
    float phaserR = phaserR_.Process(in.right);

    float wetL = weights_.chorus * chorusL + weights_.flanger * flangerL + weights_.phaser * phaserL;
    float wetR = weights_.chorus * chorusR + weights_.flanger * flangerR + weights_.phaser * phaserR;

    float dryGain = std::cos(mix01_ * 1.57079632679f);
    float wetGain = std::sin(mix01_ * 1.57079632679f);

    return {
        in.left  * dryGain + wetL * wetGain,
        in.right * dryGain + wetR * wetGain,
    };
}
```

- [ ] **Step 6: Run the tests to verify they pass**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: all test cases pass (this has been spiked and verified against the real DaisySP sources — chorus/flanger/phaser outputs at morph anchors 0.0/0.5/1.0 measurably diverge after 200 samples of a 20-sample-period test sine).

- [ ] **Step 7: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git add aurorus/modulation.h aurorus/modulation.cpp aurorus/tests/test_modulation.cpp aurorus/tests/Makefile
git commit -m "Add ModulationEngine core: chorus/flanger/phaser morph crossfade"
```

---

## Task 4: Stereo width (KNOB_ATMOSPHERE detune)

**Files:**
- Modify: `aurorus/modulation.h`
- Modify: `aurorus/modulation.cpp`
- Modify: `aurorus/tests/test_modulation.cpp`

**Interfaces:**
- Consumes: `ModulationEngine` from Task 3.
- Produces: `void SetWidth(float width01);` added to `ModulationEngine`. `constexpr float kMaxDetune = 0.01f;` added to `modulation.h`.

- [ ] **Step 1: Write the failing test**

Add to `aurorus/tests/test_modulation.cpp`:

```cpp
TEST_CASE("width creates a stereo difference between channels") {
    ModulationEngine narrow, wide;
    for (ModulationEngine *e : {&narrow, &wide})
    {
        e->Init(48000.f);
        e->SetMorph(0.5f); // pure flanger - channel-separable
        e->SetMix(1.0f);
        e->SetRate(0.8f);
        e->SetDepth(1.0f);
        e->SetFeedback(0.2f);
    }
    narrow.SetWidth(0.0f);
    wide.SetWidth(1.0f);

    StereoFrame narrowOut{0.f, 0.f}, wideOut{0.f, 0.f};
    for (int i = 0; i < 50000; i++)
    {
        float x = TestSignal(i);
        narrowOut = narrow.Process({x, x});
        wideOut   = wide.Process({x, x});
    }

    CHECK(narrowOut.left == doctest::Approx(narrowOut.right).epsilon(kEps));
    CHECK(wideOut.left   != doctest::Approx(wideOut.right).epsilon(kEps));
}
```

- [ ] **Step 2: Run the tests to verify the new test fails**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: FAIL to compile — `SetWidth` not a member of `ModulationEngine`.

- [ ] **Step 3: Implement the minimal code to make the test pass**

In `aurorus/modulation.h`, add the detune constant after `kMaxFeedback`:

```cpp
constexpr float kMaxFeedback = 0.9f;
constexpr float kMaxDetune   = 0.01f; // +-1% => up to 2% L/R spread at width=1
```

Add `void SetWidth(float width01);` to the `public:` section of `ModulationEngine`, and `float width01_ = 0.f;` to the `private:` section (next to `mix01_`).

In `aurorus/modulation.cpp`, add:

```cpp
void ModulationEngine::SetWidth(float width01)
{
    width01_ = width01;
    UpdateRates();
}
```

Update `UpdateRates()` to apply the detune per channel:

```cpp
void ModulationEngine::UpdateRates()
{
    float rateHz = MapRate01ToHz(rate01_);
    float freqL  = rateHz * (1.f - width01_ * kMaxDetune);
    float freqR  = rateHz * (1.f + width01_ * kMaxDetune);

    chorus_.SetLfoFreq(freqL, freqR);
    flangerL_.SetLfoFreq(freqL);
    flangerR_.SetLfoFreq(freqR);
    phaserL_.SetLfoFreq(freqL);
    phaserR_.SetLfoFreq(freqR);
}
```

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: all test cases pass (spiked and verified: at width=0, L and R outputs are bit-identical every frame; at width=1, after 50,000 samples the channels measurably diverge — observed diff ≈ 0.0092 in the spike, comfortably above the 1e-4 epsilon).

- [ ] **Step 5: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git add aurorus/modulation.h aurorus/modulation.cpp aurorus/tests/test_modulation.cpp
git commit -m "Add stereo width detune to ModulationEngine"
```

---

## Task 5: Freeze and reverse polarity

**Files:**
- Modify: `aurorus/modulation.h`
- Modify: `aurorus/modulation.cpp`
- Modify: `aurorus/tests/test_modulation.cpp`

**Interfaces:**
- Consumes: `ModulationEngine` from Task 4.
- Produces: `void SetFreeze(bool freeze);` and `void SetReversePolarity(bool reversed);` added to `ModulationEngine`.

- [ ] **Step 1: Write the failing tests**

Add to `aurorus/tests/test_modulation.cpp`:

```cpp
TEST_CASE("reverse polarity inverts the wet signal") {
    ModulationEngine normal, reversed;
    for (ModulationEngine *e : {&normal, &reversed})
    {
        e->Init(48000.f);
        e->SetMorph(1.0f); // pure phaser - single active effect
        e->SetMix(1.0f);
        e->SetRate(0.5f);
        e->SetDepth(1.0f);
        e->SetFeedback(0.3f);
    }
    reversed.SetReversePolarity(true);

    StereoFrame outN{0.f, 0.f}, outR{0.f, 0.f};
    for (int i = 0; i < 50; i++)
    {
        float x = TestSignal(i);
        outN = normal.Process({x, x});
        outR = reversed.Process({x, x});
    }

    CHECK(outR.left == doctest::Approx(-outN.left).epsilon(kEps));
}

TEST_CASE("reverse polarity leaves the dry signal untouched") {
    // A fully-wet mix can't distinguish "invert wet only" from "invert
    // everything" (dry contributes nothing either way). Use a partial mix
    // so both are present, then verify algebraically: with reverse
    // flipping only the wet term, outNormal = dry + wet and
    // outReversed = dry - wet, so (outNormal + outReversed) / 2 == dry
    // exactly, regardless of what wet is.
    ModulationEngine normal, reversed;
    for (ModulationEngine *e : {&normal, &reversed})
    {
        e->Init(48000.f);
        e->SetMorph(1.0f); // pure phaser - single active effect
        e->SetMix(0.3f);   // partial mix - both dry and wet contribute
        e->SetRate(0.5f);
        e->SetDepth(1.0f);
        e->SetFeedback(0.3f);
    }
    reversed.SetReversePolarity(true);

    StereoFrame in{0.f, 0.f}, outN{0.f, 0.f}, outR{0.f, 0.f};
    for (int i = 0; i < 50; i++)
    {
        float x = TestSignal(i);
        in   = {x, x};
        outN = normal.Process(in);
        outR = reversed.Process(in);
    }

    float dryGain    = std::cos(0.3f * 1.57079632679f);
    float expectedDry = dryGain * in.left;

    CHECK((outN.left + outR.left) / 2.f == doctest::Approx(expectedDry).epsilon(kEps));
    CHECK(outR.left != doctest::Approx(outN.left).epsilon(kEps)); // wet still audibly differs
}

TEST_CASE("freeze halts the modulation sweep at its current phase") {
    // Freezing immediately after Init (phase still at its default 0)
    // can't distinguish "hold the current phase" from a buggy "reset
    // phase to 0" implementation - both look identical from t=0. Run
    // both engines un-frozen through a pre-warmup first so the LFO
    // moves to a non-default phase, THEN engage freeze, so a reset-to-0
    // bug would diverge from the correct hold-in-place behavior.
    ModulationEngine frozen, moving;
    for (ModulationEngine *e : {&frozen, &moving})
    {
        e->Init(48000.f);
        e->SetMorph(0.5f); // pure flanger - simplest single active effect
        e->SetMix(1.0f);
        e->SetRate(0.8f);
        e->SetDepth(1.0f);
        e->SetFeedback(0.2f);
    }

    const int kPreWarmup = 10000;
    for (int i = 0; i < kPreWarmup; i++)
    {
        float x = TestSignal(i);
        frozen.Process({x, x});
        moving.Process({x, x});
    }

    frozen.SetFreeze(true);

    // Continue warming up so the feedback transient settles into a
    // steady periodic response to the repeating test signal.
    const int kWarmup = kPreWarmup + 20000;
    for (int i = kPreWarmup; i < kWarmup; i++)
    {
        float x = TestSignal(i);
        frozen.Process({x, x});
        moving.Process({x, x});
    }

    float frozenA = frozen.Process({TestSignal(kWarmup), TestSignal(kWarmup)}).left;
    for (int i = kWarmup + 1; i < kWarmup + 20; i++)
        frozen.Process({TestSignal(i), TestSignal(i)});
    float frozenB = frozen.Process({TestSignal(kWarmup + 20), TestSignal(kWarmup + 20)}).left;

    float movingA = moving.Process({TestSignal(kWarmup), TestSignal(kWarmup)}).left;
    for (int i = kWarmup + 1; i < kWarmup + 20; i++)
        moving.Process({TestSignal(i), TestSignal(i)});
    float movingB = moving.Process({TestSignal(kWarmup + 20), TestSignal(kWarmup + 20)}).left;

    // Same phase of the repeating (period-20) input, one period apart.
    // A frozen LFO reads the delay line at a fixed offset, so the output
    // repeats; a moving LFO keeps sweeping the offset, so it does not.
    CHECK(frozenA == doctest::Approx(frozenB).epsilon(kEps));
    CHECK(movingA != doctest::Approx(movingB).epsilon(kEps));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: FAIL to compile — `SetReversePolarity`/`SetFreeze` not members of `ModulationEngine`.

- [ ] **Step 3: Implement the minimal code to make the tests pass**

Add `void SetFreeze(bool freeze);` and `void SetReversePolarity(bool reversed);` to the `public:` section of `ModulationEngine` in `aurorus/modulation.h`, and `bool freeze_ = false;`, `bool reverse_ = false;` to the `private:` section.

In `aurorus/modulation.cpp`, add:

```cpp
void ModulationEngine::SetFreeze(bool freeze)
{
    freeze_ = freeze;
    UpdateRates();
}

void ModulationEngine::SetReversePolarity(bool reversed)
{
    reverse_ = reversed;
}
```

Update `UpdateRates()` to honor freeze:

```cpp
void ModulationEngine::UpdateRates()
{
    float rateHz = freeze_ ? 0.f : MapRate01ToHz(rate01_);
    float freqL  = rateHz * (1.f - width01_ * kMaxDetune);
    float freqR  = rateHz * (1.f + width01_ * kMaxDetune);

    chorus_.SetLfoFreq(freqL, freqR);
    flangerL_.SetLfoFreq(freqL);
    flangerR_.SetLfoFreq(freqR);
    phaserL_.SetLfoFreq(freqL);
    phaserR_.SetLfoFreq(freqR);
}
```

Update `Process()` to apply the polarity inversion to the wet signal, before the mix:

```cpp
StereoFrame ModulationEngine::Process(StereoFrame in)
{
    float mono = (in.left + in.right) * 0.5f;

    chorus_.Process(mono);
    float chorusL = chorus_.GetLeft();
    float chorusR = chorus_.GetRight();

    float flangerL = flangerL_.Process(in.left);
    float flangerR = flangerR_.Process(in.right);

    float phaserL = phaserL_.Process(in.left);
    float phaserR = phaserR_.Process(in.right);

    float wetL = weights_.chorus * chorusL + weights_.flanger * flangerL + weights_.phaser * phaserL;
    float wetR = weights_.chorus * chorusR + weights_.flanger * flangerR + weights_.phaser * phaserR;

    if (reverse_)
    {
        wetL = -wetL;
        wetR = -wetR;
    }

    float dryGain = std::cos(mix01_ * 1.57079632679f);
    float wetGain = std::sin(mix01_ * 1.57079632679f);

    return {
        in.left  * dryGain + wetL * wetGain,
        in.right * dryGain + wetR * wetGain,
    };
}
```

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
```

Expected: all test cases pass (spiked and verified: fully-wet reverse produces an exact sign mirror; partial-mix reverse gives `(outNormal + outReversed) / 2 == dry` to within ~1e-8, proving dry is untouched; freeze engaged mid-sweep (after a 10,000-sample pre-warmup so the LFO has already moved off its Init-time phase) gives a same-phase diff of ≈ 0.0000236 after a further 20,000-sample warmup vs ≈ 0.0079 for the non-frozen engine — comfortably separated by the 1e-4 epsilon).

- [ ] **Step 5: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git add aurorus/modulation.h aurorus/modulation.cpp aurorus/tests/test_modulation.cpp
git commit -m "Add freeze and reverse polarity to ModulationEngine"
```

---

## Task 6: Hardware wiring (main.cpp + ARM Makefile)

This task has no host unit tests — it is hardware wiring, verified by a successful ARM build (matching the `hello-aurora` convention where `main.cpp` itself is untested, only the pure logic it calls into).

**Files:**
- Create: `aurorus/main.cpp`
- Create: `aurorus/Makefile`
- Modify: `aurorus/modulation.h`, `aurorus/modulation.cpp` (memory-footprint fix, see below)

**Interfaces:**
- Consumes: `ModulationEngine` (`Init`, `SetMorph`, `SetRate`, `SetDepth`, `SetFeedback`, `SetMix`, `SetWidth`, `SetFreeze`, `SetReversePolarity`, `Process`) from Tasks 3-5; `StereoFrame` from Task 3; `blendColour(float)`, `Rgb` from Task 2.

### Memory-footprint fix (discovered at ARM build time)

The first attempt at this task's ARM build failed at link time: `.bss` overflowed the STM32H750's 128KB `DTCMRAM` region by ~66KB. Measured with `sizeof()` on the host: `sizeof(daisysp::Chorus) = 19,320B`, `sizeof(daisysp::Flanger) = 3,888B`, `sizeof(daisysp::Phaser) = 77,320B`. The two `Phaser` instances (`phaserL_`, `phaserR_`) alone cost ~154,640 bytes — because DaisySP's `Phaser` wrapper class always allocates a **fixed 8-pole array internally** (`PhaserEngine engines_[8]`), regardless of the runtime pole count passed to `SetPoles()`.

Two fixes were considered: placing the engine in the Daisy Seed's external SDRAM (via `DSY_SDRAM_BSS`), or reducing the phaser's pole count. SDRAM was rejected — the Aurora hardware notes (`aurora.h`) describe REV3 as "normal Daisy Seed," and standard Daisy Seed revisions often don't have SDRAM populated on the PCB; that fact can't be verified from this repo, and getting it wrong risks a hard fault on real hardware. Reducing the pole count was chosen instead: it's a well-established, musically legitimate phaser configuration (4 poles is classic analog-phaser territory, e.g. MXR Phase 90), stays entirely within tested DaisySP code, and carries no hardware risk.

**Fix:** replace the two `daisysp::Phaser phaserL_, phaserR_;` members with manually-managed 4-element arrays of the lower-level `daisysp::PhaserEngine` building block (the same class `Phaser` uses internally, but without its fixed 8-element array):

In `aurorus/modulation.h`, replace:
```cpp
    daisysp::Phaser  phaserL_;
    daisysp::Phaser  phaserR_;
```
with:
```cpp
    static constexpr int kPhaserPoles = 4;
    daisysp::PhaserEngine phaserL_[kPhaserPoles];
    daisysp::PhaserEngine phaserR_[kPhaserPoles];
```

In `aurorus/modulation.cpp`, every place that called a method directly on `phaserL_`/`phaserR_` becomes a loop over the array, calling the same method on each element (this exactly mirrors what `Phaser`'s own methods already do internally for its 8 elements — no behavior change, just a smaller pole count):

```cpp
void ModulationEngine::Init(float sample_rate)
{
    chorus_.Init(sample_rate);
    flangerL_.Init(sample_rate);
    flangerR_.Init(sample_rate);
    for (int i = 0; i < kPhaserPoles; i++)
    {
        phaserL_[i].Init(sample_rate);
        phaserR_[i].Init(sample_rate);
    }
}
```
```cpp
void ModulationEngine::SetDepth(float depth01)
{
    chorus_.SetLfoDepth(depth01);
    flangerL_.SetLfoDepth(depth01);
    flangerR_.SetLfoDepth(depth01);
    for (int i = 0; i < kPhaserPoles; i++)
    {
        phaserL_[i].SetLfoDepth(depth01);
        phaserR_[i].SetLfoDepth(depth01);
    }
}
```
```cpp
void ModulationEngine::SetFeedback(float fb01)
{
    float fb = MapFeedback01(fb01);
    chorus_.SetFeedback(fb);
    flangerL_.SetFeedback(fb);
    flangerR_.SetFeedback(fb);
    for (int i = 0; i < kPhaserPoles; i++)
    {
        phaserL_[i].SetFeedback(fb);
        phaserR_[i].SetFeedback(fb);
    }
}
```
```cpp
void ModulationEngine::UpdateRates()
{
    float rateHz = freeze_ ? 0.f : MapRate01ToHz(rate01_);
    float freqL  = rateHz * (1.f - width01_ * kMaxDetune);
    float freqR  = rateHz * (1.f + width01_ * kMaxDetune);

    chorus_.SetLfoFreq(freqL, freqR);
    flangerL_.SetLfoFreq(freqL);
    flangerR_.SetLfoFreq(freqR);
    for (int i = 0; i < kPhaserPoles; i++)
    {
        phaserL_[i].SetLfoFreq(freqL);
        phaserR_[i].SetLfoFreq(freqR);
    }
}
```
```cpp
StereoFrame ModulationEngine::Process(StereoFrame in)
{
    float mono = (in.left + in.right) * 0.5f;

    chorus_.Process(mono);
    float chorusL = chorus_.GetLeft();
    float chorusR = chorus_.GetRight();

    float flangerL = flangerL_.Process(in.left);
    float flangerR = flangerR_.Process(in.right);

    float phaserL = 0.f, phaserR = 0.f;
    for (int i = 0; i < kPhaserPoles; i++)
    {
        phaserL += phaserL_[i].Process(in.left);
        phaserR += phaserR_[i].Process(in.right);
    }

    float wetL = weights_.chorus * chorusL + weights_.flanger * flangerL + weights_.phaser * phaserL;
    float wetR = weights_.chorus * chorusR + weights_.flanger * flangerR + weights_.phaser * phaserR;

    if (reverse_)
    {
        wetL = -wetL;
        wetR = -wetR;
    }

    float dryGain = std::cos(mix01_ * 1.57079632679f);
    float wetGain = std::sin(mix01_ * 1.57079632679f);

    return {
        in.left  * dryGain + wetL * wetGain,
        in.right * dryGain + wetR * wetGain,
    };
}
```

Verified: `sizeof(ModulationEngine)` drops from 181,736 bytes (8-pole) to 104,440 bytes (4-pole) — comfortable headroom under the 128KB (131,072-byte) DTCMRAM budget once linked. All 12 existing test cases (Tasks 1-5) pass unchanged against this version with zero test-code changes — none of the existing assertions depend on the phaser's absolute output magnitude, only on relative/qualitative properties (distinctness, freeze holding steady, sign inversion, L/R divergence with width), which hold regardless of pole count.

- [ ] **Step 0: Apply the phaser memory-footprint fix above to `aurorus/modulation.h`/`.cpp`, then rerun the full host test suite (`cd aurorus/tests && make`) and confirm all 12 test_modulation cases and 5 test_blend_colour cases still pass before proceeding to Step 1.**

- [ ] **Step 1: Create `aurorus/main.cpp`**

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
#include "aurora.h"
#include "modulation.h"
#include "blend_colour.h"

using namespace daisy;
using namespace aurora;

Hardware         hw;
ModulationEngine engine;

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

int main(void)
{
    hw.Init();
    engine.Init(hw.seed.AudioSampleRate());
    hw.StartAudio(AudioCallback);

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
}
```

- [ ] **Step 2: Create `aurorus/Makefile`**

```makefile
include ../config.mk

C_DEFS += -DFIRMWARE_VERSION=\"$(FIRMWARE_VERSION)\"

# Project Name
TARGET = aurorus

# Build Project for Daisy Bootloader
APP_TYPE = BOOT_SRAM

# Sources
CPP_SOURCES = main.cpp modulation.cpp

# Aurora BSP header
C_INCLUDES += -I$(AURORA_SDK_DIR)/include/

# Library Locations
LIBDAISY_DIR ?= $(AURORA_SDK_DIR)/libs/libDaisy
DAISYSP_DIR  ?= $(AURORA_SDK_DIR)/libs/DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
```

- [ ] **Step 3: Build libDaisy and DaisySP (one-time, only if not already built)**

```bash
cd /home/dave/Development/Aurora-Aurorus
grep GCC_PATH config.mk
ls lib/Aurora-SDK/libs/libDaisy/build/libdaisy.a 2>/dev/null || make libdaisy
ls lib/Aurora-SDK/libs/DaisySP/build/libdaisysp.a 2>/dev/null || \
  make -C lib/Aurora-SDK/libs/DaisySP GCC_PATH=$(grep '^GCC_PATH' config.mk | head -1 | cut -d= -f2 | tr -d ' ?')
```

If `GCC_PATH` in `config.mk` doesn't resolve on this machine, pass the correct path explicitly, e.g. `make libdaisy GCC_PATH=/path/to/arm-gcc/bin`.

- [ ] **Step 4: Build the firmware to verify it compiles and links**

```bash
cd /home/dave/Development/Aurora-Aurorus
make build PROJECT=aurorus
```

Expected: build succeeds, producing `aurorus/build/aurorus-dev.bin` (or `aurorus-<version>.bin` on a tagged commit). If it fails with missing symbols from `daisysp`, confirm `DaisySP` was built per Step 3.

- [ ] **Step 5: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git add aurorus/main.cpp aurorus/Makefile
git commit -m "Add aurorus hardware wiring (main.cpp + ARM Makefile)"
```

---

## Task 7: Remove hello-aurora

**Files:**
- Delete: `hello-aurora/main.cpp`, `hello-aurora/colour.h`, `hello-aurora/audio.h`, `hello-aurora/Makefile`, `hello-aurora/tests/Makefile`, `hello-aurora/tests/test_colour.cpp`, `hello-aurora/tests/test_audio.cpp`

- [ ] **Step 1: Delete the directory**

```bash
cd /home/dave/Development/Aurora-Aurorus
git rm -r hello-aurora
```

- [ ] **Step 2: Verify the repo still builds and tests pass without it**

```bash
cd /home/dave/Development/Aurora-Aurorus/aurorus/tests && make
cd /home/dave/Development/Aurora-Aurorus && make build PROJECT=aurorus
```

Expected: both succeed — `aurorus` has no dependency on `hello-aurora`.

- [ ] **Step 3: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git commit -m "Remove hello-aurora scaffold"
```

---

## Task 8: Update CI workflow and README

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `README.md`

**Interfaces:**
- None — documentation and CI configuration only.

- [ ] **Step 1: Update the CI test job**

In `.github/workflows/ci.yml`, change:

```yaml
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build and run host tests
        run: make -C hello-aurora/tests
```

to:

```yaml
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Build and run host tests
        run: make -C aurorus/tests
```

Note: `submodules: recursive` is now required in the `test` job (it wasn't before) because `aurorus/tests/Makefile` compiles DaisySP source files directly, unlike `hello-aurora/tests` which had no SDK dependency.

The `build` job's project-discovery step (`find . -maxdepth 1 -mindepth 1 -type d ...`) needs no change — it already discovers whatever directories contain a `Makefile`.

- [ ] **Step 2: Update the README**

In `README.md`, replace the table row:

```markdown
| `hello-aurora/` | Example project: knob-driven LED colours + audio passthrough |
```

with:

```markdown
| `aurorus/` | Morphing chorus/flanger/phaser modulation effect |
```

Replace the `### hello-aurora` section (and its bullet list) with:

```markdown
### aurorus

A firmware that continuously morphs between chorus, flanger, and phaser:

- `KNOB_WARP` morphs the effect character: chorus -> flanger -> phaser
- `KNOB_TIME` sets the LFO rate, `KNOB_BLUR` sets LFO depth, `KNOB_REFLECT` sets feedback
- `KNOB_MIX` sets the wet/dry balance, `KNOB_ATMOSPHERE` sets stereo width
- `SW_FREEZE` holds the current modulation phase in place while held
- `SW_REVERSE` inverts the wet signal's polarity while held (classic "through-zero" flanger switch)
- All LEDs show a single blended colour reflecting the current morph position (blue = chorus, green = flanger, magenta = phaser)

The `aurorus/tests/` directory has host-side unit tests for `modulation.h`/`.cpp` and `blend_colour.h`, built and run with plain `g++` and [doctest](https://github.com/doctest/doctest) — including the real DaisySP `Chorus`/`Flanger`/`Phaser` sources compiled directly for the host, not a mock.
```

Update the two `make build`/`make flash` example commands further down from `PROJECT=hello-aurora` to `PROJECT=aurorus`, and the `cd hello-aurora/tests` line to `cd aurorus/tests`.

- [ ] **Step 3: Commit**

```bash
cd /home/dave/Development/Aurora-Aurorus
git add .github/workflows/ci.yml README.md
git commit -m "Update CI and README for aurorus"
```

---

## Self-Review Notes

- **Spec coverage:** Morph crossfade (Task 1/3), stereo width (Task 4), freeze/reverse (Task 5), knob/button wiring (Task 6), LED blend indicator (Task 2/6), hello-aurora removal (Task 7), CI/README (Task 8) — all spec sections have a corresponding task.
- **Verified against real DaisySP:** the freeze-warmup sample count (20,000) and width-detune sample count (50,000) were empirically confirmed by compiling and running the actual engine logic against the checked-out DaisySP sources before this plan was finalized — not estimated.
- **Type consistency:** `StereoFrame`, `MorphWeights`, `Rgb`, and all `ModulationEngine` method signatures are identical across every task that references them.
