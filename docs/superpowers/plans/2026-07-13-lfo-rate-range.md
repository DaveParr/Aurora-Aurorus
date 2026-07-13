# Widen LFO Rate Range Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Widen the shared LFO rate range from 0.05–5 Hz to 0.1–10 Hz so both ends of `KNOB_TIME` read as faster, per [issue #1](https://github.com/DaveParr/Aurora-Aurorus/issues/1).

**Architecture:** Change two constants (`kMinRateHz`, `kMaxRateHz`) in `aurorus/modulation.h`. The exponential mapping function and single-shared-range architecture are unchanged. `led_breath.h` reuses the same mapping function for LED breathing rate, so its tests need matching updates.

**Tech Stack:** C++14, doctest (header-only, vendored at `aurorus/tests/doctest.h`), GNU Make.

## Global Constraints

- New range: `kMinRateHz = 0.1f`, `kMaxRateHz = 10.0f` (was `0.05f` / `5.0f`).
- Keep the existing exponential curve shape in `MapRate01ToHz` — only the two constants change.
- Single shared range for chorus/flanger/phaser — no per-effect ranges (see spec's Research section for why).
- Full spec: `docs/superpowers/specs/2026-07-13-lfo-rate-range-design.md`.

---

### Task 1: Widen the LFO rate range and update all dependent tests

**Files:**
- Modify: `aurorus/modulation.h:13-14`
- Modify: `aurorus/tests/test_modulation.cpp` (add a boundary test)
- Modify: `aurorus/tests/test_led_breath.cpp:25-36` (two tests hardcode the old 0.05Hz/5Hz values)
- Modify: `docs/superpowers/specs/2026-07-02-aurorus-modulation-design.md:103`

**Interfaces:**
- Consumes: `MapRate01ToHz(float rate01) -> float` (existing, defined in `aurorus/modulation.h:38-41`) — signature unchanged, only its constants change.
- Produces: nothing new for later tasks — this is the only task in the plan.

- [ ] **Step 1: Write the failing boundary test for the new range**

Add to the end of `aurorus/tests/test_modulation.cpp`:

```cpp
TEST_CASE("MapRate01ToHz spans the documented 0.1-10 Hz range") {
    CHECK(MapRate01ToHz(0.0f) == doctest::Approx(0.1f).epsilon(kEps));
    CHECK(MapRate01ToHz(1.0f) == doctest::Approx(10.0f).epsilon(kEps));
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:
```bash
cd aurorus/tests && make test_modulation && ./test_modulation
```
Expected: FAIL — `MapRate01ToHz(0.0f)` currently returns `0.05f`, not `0.1f` (and `MapRate01ToHz(1.0f)` returns `5.0f`, not `10.0f`). Doctest reports 1 failed test case, e.g.:
```
[doctest] test cases:  18 |  17 passed | 1 failed | 0 skipped
```

- [ ] **Step 3: Widen the range constants**

In `aurorus/modulation.h`, change:

```cpp
constexpr float kMinRateHz   = 0.05f;
constexpr float kMaxRateHz   = 5.0f;
```

to:

```cpp
constexpr float kMinRateHz   = 0.1f;
constexpr float kMaxRateHz   = 10.0f;
```

- [ ] **Step 4: Run test_modulation again to verify it passes**

Run:
```bash
cd aurorus/tests && make test_modulation && ./test_modulation
```
Expected: PASS — all 18 test cases pass (the 17 pre-existing plus the new boundary test).

- [ ] **Step 5: Rebuild test_led_breath and observe it now fails**

`led_breath.h` calls the same `MapRate01ToHz`, and `test_led_breath.cpp` hardcodes the old 0.05Hz/5Hz-derived expected values. Confirm the break:

```bash
cd aurorus/tests && make test_led_breath && ./test_led_breath
```
Expected: FAIL — 2 of 4 test cases fail:
- `"AdvancePhase - min rate (0.05Hz) advances a small fraction per second"` now produces `0.62831853` (2·π·0.1Hz·1s), not the asserted `0.31415927`.
- `"AdvancePhase - max rate (5Hz) wraps past 2*pi correctly"` now produces `2·π·(10Hz·0.9s mod 1)` = `2·π·0` = `0`, not the asserted `3.14159265`.

- [ ] **Step 6: Update test_led_breath.cpp to match the new range**

In `aurorus/tests/test_led_breath.cpp`, replace lines 25-36:

```cpp
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

with:

```cpp
TEST_CASE("AdvancePhase - min rate (0.1Hz) advances a small fraction per second") {
    float phase = AdvancePhase(0.f, 0.f, 1.f);
    CHECK(phase == doctest::Approx(0.62831853f).epsilon(kEps));
}

TEST_CASE("AdvancePhase - max rate (10Hz) wraps past 2*pi correctly") {
    // 10Hz * 0.45s = 4.5 cycles -> wraps to exactly pi radians past a full cycle.
    float phase = AdvancePhase(0.f, 1.f, 0.45f);
    CHECK(phase == doctest::Approx(3.14159265f).epsilon(kEps));
    CHECK(phase >= 0.f);
    CHECK(phase < 6.2831853f);
}
```

- [ ] **Step 7: Run test_led_breath again to verify it passes**

Run:
```bash
cd aurorus/tests && make test_led_breath && ./test_led_breath
```
Expected: PASS — all 4 test cases pass.

- [ ] **Step 8: Update the design doc reference to the old range**

In `docs/superpowers/specs/2026-07-02-aurorus-modulation-design.md:103`, replace:

```
| `KNOB_TIME` | LFO rate | exponential curve, ~0.05–5 Hz | `SetLfoFreq(hz)` |
```

with:

```
| `KNOB_TIME` | LFO rate | exponential curve, ~0.1–10 Hz | `SetLfoFreq(hz)` |
```

- [ ] **Step 9: Run the full test suite to confirm nothing else broke**

Run:
```bash
cd aurorus/tests && make all
```
Expected: PASS — `test_modulation`, `test_blend_colour`, and `test_led_breath` all report `Status: SUCCESS!`.

- [ ] **Step 10: Commit**

```bash
git add aurorus/modulation.h aurorus/tests/test_modulation.cpp aurorus/tests/test_led_breath.cpp docs/superpowers/specs/2026-07-02-aurorus-modulation-design.md
git commit -m "$(cat <<'EOF'
Widen LFO rate range from 0.05-5Hz to 0.1-10Hz

Closes #1
EOF
)"
```
