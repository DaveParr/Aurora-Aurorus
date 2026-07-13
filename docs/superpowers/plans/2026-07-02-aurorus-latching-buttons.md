# Aurorus Latching Freeze/Reverse Buttons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Change `SW_FREEZE` and `SW_REVERSE` from momentary (hold-to-activate) to latching (press-to-toggle) buttons in the `aurorus` firmware, with LEDs reflecting toggled state instead of button contact.

**Architecture:** `ModulationEngine` (hardware-independent, host-testable) gains `ToggleFreeze()`/`ToggleReversePolarity()` methods and `IsFrozen()`/`IsReversed()` getters, replacing its `SetFreeze(bool)`/`SetReversePolarity(bool)` setters. `main.cpp` (hardware-coupled, ARM-only) calls the toggle methods on `daisy::Switch::RisingEdge()` instead of passing `Pressed()` through every audio block, and the LED loop reads engine state instead of button state.

**Tech Stack:** C++14, DaisySP (host + ARM), libDaisy (`daisy::Switch`), doctest (host tests), GNU Make, `arm-none-eabi-gcc` (target build).

## Global Constraints

- Freeze and Reverse toggle independently of each other — toggling one must not change the other's state.
- No persistence across power cycles: both `freeze_` and `reverse_` default to `false` at construction (existing default-member-initializers, unchanged).
- `daisy::Switch` debounce/`RisingEdge()` behavior is not modified — only consumed.
- `ModulationEngine` (`modulation.h`/`modulation.cpp`) keeps zero dependency on Aurora/Daisy headers — toggle logic operates on plain bools, same as today.
- Match existing code style: 4-space indentation, brace-on-own-line, aligned trailing types as seen in `modulation.cpp`/`main.cpp`.
- Host test build: `cd aurorus/tests && make all` (compiles + runs both binaries). ARM firmware build: `make build PROJECT=aurorus` from repo root (uses `GCC_PATH` from `config.mk`, already confirmed installed at `$(HOME)/.local/share/gcc-arm-none-eabi/gcc-arm-none-eabi-10-2020-q4-major/bin`).

---

### Task 1: `ModulationEngine` toggle API

**Files:**
- Modify: `aurorus/modulation.h:59-60` (declarations)
- Modify: `aurorus/modulation.cpp:62-71` (definitions)
- Test: `aurorus/tests/test_modulation.cpp:148,178,225` (update existing call sites), append new cases at end of file

**Interfaces:**
- Produces (consumed by Task 2's `main.cpp`): `void ModulationEngine::ToggleFreeze()`, `void ModulationEngine::ToggleReversePolarity()`, `bool ModulationEngine::IsFrozen() const`, `bool ModulationEngine::IsReversed() const`
- Removes: `void ModulationEngine::SetFreeze(bool)`, `void ModulationEngine::SetReversePolarity(bool)` — no longer available after this task; Task 2 depends on this task being complete first, since `main.cpp` currently calls the removed setters.

This task's existing test file (`test_modulation.cpp`) has three call sites using the setters being removed (`SetReversePolarity(true)` at lines 148 and 178, `SetFreeze(true)` at line 225) — these must be updated to the toggle API in the same step as the new tests, or the test binary won't compile.

- [ ] **Step 1: Update existing test call sites and add new toggle test cases**

In `aurorus/tests/test_modulation.cpp`, change line 148 (inside `TEST_CASE("reverse polarity inverts the wet signal")`):

```cpp
    reversed.SetReversePolarity(true);
```
to:
```cpp
    reversed.ToggleReversePolarity();
```

Change line 178 (inside `TEST_CASE("reverse polarity leaves the dry signal untouched")`), same replacement:
```cpp
    reversed.SetReversePolarity(true);
```
to:
```cpp
    reversed.ToggleReversePolarity();
```

Change line 225 (inside `TEST_CASE("freeze halts the modulation sweep")`):
```cpp
    frozen.SetFreeze(true);
```
to:
```cpp
    frozen.ToggleFreeze();
```

Then append these new test cases at the end of the file (after the last `TEST_CASE`, which currently ends at line 252 with the closing brace of `"freeze halts the modulation sweep"`):

```cpp

TEST_CASE("freeze and reverse default to off") {
    ModulationEngine engine;
    engine.Init(48000.f);
    CHECK(engine.IsFrozen()   == false);
    CHECK(engine.IsReversed() == false);
}

TEST_CASE("ToggleFreeze flips state on each call") {
    ModulationEngine engine;
    engine.Init(48000.f);

    engine.ToggleFreeze();
    CHECK(engine.IsFrozen() == true);

    engine.ToggleFreeze();
    CHECK(engine.IsFrozen() == false);
}

TEST_CASE("ToggleReversePolarity flips state on each call") {
    ModulationEngine engine;
    engine.Init(48000.f);

    engine.ToggleReversePolarity();
    CHECK(engine.IsReversed() == true);

    engine.ToggleReversePolarity();
    CHECK(engine.IsReversed() == false);
}

TEST_CASE("toggling freeze does not affect reverse, and vice versa") {
    ModulationEngine engine;
    engine.Init(48000.f);

    engine.ToggleFreeze();
    CHECK(engine.IsFrozen()   == true);
    CHECK(engine.IsReversed() == false);

    engine.ToggleReversePolarity();
    CHECK(engine.IsFrozen()   == true);
    CHECK(engine.IsReversed() == true);

    engine.ToggleFreeze();
    CHECK(engine.IsFrozen()   == false);
    CHECK(engine.IsReversed() == true);
}
```

- [ ] **Step 2: Run tests to verify they fail to compile**

Run: `cd aurorus/tests && make all`
Expected: FAIL — compiler errors like `'class ModulationEngine' has no member named 'ToggleFreeze'` (and similarly for `ToggleReversePolarity`/`IsFrozen`/`IsReversed`), plus `'class ModulationEngine' has no member named 'SetReversePolarity'`/`'SetFreeze'` no longer apply since those calls were replaced — the only errors should be the four missing new members.

- [ ] **Step 3: Implement the toggle API**

In `aurorus/modulation.h`, replace lines 59-60:
```cpp
    void SetFreeze(bool freeze);
    void SetReversePolarity(bool reversed);
```
with:
```cpp
    void ToggleFreeze();
    void ToggleReversePolarity();
    bool IsFrozen() const { return freeze_; }
    bool IsReversed() const { return reverse_; }
```

In `aurorus/modulation.cpp`, replace lines 62-71:
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
with:
```cpp
void ModulationEngine::ToggleFreeze()
{
    freeze_ = !freeze_;
    UpdateRates();
}

void ModulationEngine::ToggleReversePolarity()
{
    reverse_ = !reverse_;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd aurorus/tests && make all`
Expected: PASS — both binaries run and print `[doctest] Status: SUCCESS!`. `test_modulation` should report 16 test cases (the existing 12, minus the 3 modified in place, still counts as 12, plus the 4 new ones = 16), all passed.

- [ ] **Step 5: Commit**

```bash
git add aurorus/modulation.h aurorus/modulation.cpp aurorus/tests/test_modulation.cpp
git commit -m "Replace Freeze/Reverse setters with toggle API in ModulationEngine"
```

---

### Task 2: Wire latching behavior into `main.cpp`

**Files:**
- Modify: `aurorus/main.cpp:7-8` (header comment), `aurorus/main.cpp:30-31` (audio callback), `aurorus/main.cpp:62-66` (LED loop)

**Interfaces:**
- Consumes (from Task 1): `engine.ToggleFreeze()`, `engine.ToggleReversePolarity()`, `engine.IsFrozen()`, `engine.IsReversed()`
- Consumes (existing, unchanged): `hw.GetButton(SW_FREEZE).RisingEdge()`, `hw.GetButton(SW_REVERSE).RisingEdge()` (from `daisy::Switch`, `lib/Aurora-SDK/libs/libDaisy/src/hid/switch.h:70`)

This task has no host-testable surface (it's pure hardware wiring in `main.cpp`, which is never compiled into the host test binaries). It's verified by a successful ARM cross-compile, which is the same bar `main.cpp` changes are held to today.

- [ ] **Step 1: Update the file header comment**

In `aurorus/main.cpp`, replace lines 7-8:
```cpp
 *  SW_FREEZE holds the current modulation phase.
 *  SW_REVERSE inverts the wet signal polarity.
```
with:
```cpp
 *  SW_FREEZE toggles freezing the current modulation phase.
 *  SW_REVERSE toggles inverting the wet signal polarity.
```

- [ ] **Step 2: Replace the audio callback's button handling**

In `aurorus/main.cpp`, replace lines 30-31:
```cpp
    engine.SetFreeze(hw.GetButton(SW_FREEZE).Pressed());
    engine.SetReversePolarity(hw.GetButton(SW_REVERSE).Pressed());
```
with:
```cpp
    if (hw.GetButton(SW_FREEZE).RisingEdge())
        engine.ToggleFreeze();
    if (hw.GetButton(SW_REVERSE).RisingEdge())
        engine.ToggleReversePolarity();
```

- [ ] **Step 3: Replace the LED loop's button reads with engine state reads**

In `aurorus/main.cpp`, replace lines 62-66:
```cpp
        if (hw.GetButton(SW_FREEZE).Pressed())
            hw.SetLed(LED_FREEZE, 1.f, 1.f, 1.f);

        if (hw.GetButton(SW_REVERSE).Pressed())
            hw.SetLed(LED_REVERSE, 1.f, 1.f, 1.f);
```
with:
```cpp
        if (engine.IsFrozen())
            hw.SetLed(LED_FREEZE, 1.f, 1.f, 1.f);

        if (engine.IsReversed())
            hw.SetLed(LED_REVERSE, 1.f, 1.f, 1.f);
```

- [ ] **Step 4: Cross-compile the firmware for the target**

Run (from repo root): `make build PROJECT=aurorus`
Expected: Build succeeds with no errors, ending in a line like `Wrote build/aurorus-dev.bin` or similar linker/objcopy success output, and `aurorus/build/aurorus-dev.bin` exists on disk. Verify with:

```bash
ls -la aurorus/build/aurorus-dev.bin
```
Expected: file exists with nonzero size.

- [ ] **Step 5: Commit**

```bash
git add aurorus/main.cpp
git commit -m "Switch Freeze/Reverse buttons to press-to-toggle in main.cpp"
```

---

### Task 3: Update README documentation

**Files:**
- Modify: `README.md:12-13` (feature bullets), `README.md:43-44` (control table), `README.md:61` (audio prose), `README.md:69-70` (LED table)

**Interfaces:** None — documentation only, no code interfaces produced or consumed.

- [ ] **Step 1: Update the feature bullets**

In `README.md`, replace lines 12-13:
```markdown
- **Freeze** — holds the current modulation phase in place while held, for a static, frozen texture
- **Reverse polarity** — inverts the wet signal, the classic hardware flanger "through-zero" switch, most audible with feedback dialed up
```
with:
```markdown
- **Freeze** — press to toggle freezing the current modulation phase in place, for a static, frozen texture; press again to resume
- **Reverse polarity** — press to toggle inverting the wet signal, the classic hardware flanger "through-zero" switch, most audible with feedback dialed up
```

- [ ] **Step 2: Update the button control table**

In `README.md`, replace lines 43-44:
```markdown
| Freeze | Hold to stop the modulation sweep exactly where it is — no click, no jump, it resumes from the same point on release |
| Reverse | Hold to invert the wet signal's polarity (the classic through-zero flanger character), most audible with Reflect turned up |
```
with:
```markdown
| Freeze | Press to toggle stopping the modulation sweep exactly where it is — no click, no jump; press again to resume from the same point |
| Reverse | Press to toggle inverting the wet signal's polarity (the classic through-zero flanger character), most audible with Reflect turned up |
```

- [ ] **Step 3: Update the Audio section prose**

In `README.md`, replace line 61:
```markdown
Holding Freeze stops the sweep instantly, wherever it currently sits. Holding Reverse flips the wet signal's polarity before it's mixed back with the dry signal — with Reflect turned up, this noticeably shifts the notch/peak character of the effect.
```
with:
```markdown
Pressing Freeze toggles stopping the sweep instantly, wherever it currently sits — press again to resume. Pressing Reverse toggles flipping the wet signal's polarity before it's mixed back with the dry signal — with Reflect turned up, this noticeably shifts the notch/peak character of the effect.
```

- [ ] **Step 4: Update the LED table**

In `README.md`, replace lines 69-70:
```markdown
| Freeze LED | Lights solid white while Freeze is held. |
| Reverse LED | Lights solid white while Reverse is held. |
```
with:
```markdown
| Freeze LED | Lights solid white while Freeze is toggled on. |
| Reverse LED | Lights solid white while Reverse is toggled on. |
```

- [ ] **Step 5: Verify no stale "hold"/"held" wording remains for these controls**

Run: `grep -n -i "hold\|held" README.md`
Expected: no output referring to Freeze or Reverse (the command may legitimately match nothing at all, since no other README content uses "hold"/"held").

- [ ] **Step 6: Commit**

```bash
git add README.md
git commit -m "Document Freeze/Reverse as press-to-toggle instead of hold"
```

---

### Task 4: Full verification

**Files:** None modified — verification only.

**Interfaces:** None.

- [ ] **Step 1: Run the full host test suite**

Run: `cd aurorus/tests && make all`
Expected: Both `test_modulation` (16 test cases) and `test_blend_colour` (5 test cases) report `[doctest] Status: SUCCESS!` with 0 failed.

- [ ] **Step 2: Rebuild the firmware from a clean state**

Run (from repo root): `rm -f aurorus/build/aurorus-dev.bin && make build PROJECT=aurorus`
Expected: Build succeeds, `aurorus/build/aurorus-dev.bin` is recreated.

- [ ] **Step 3: Confirm no leftover references to the removed setters**

Run: `grep -rn "SetFreeze\|SetReversePolarity" aurorus/`
Expected: no output (both methods fully removed and all call sites migrated).

No commit for this task — it produces no file changes, only confirms Tasks 1-3 integrate cleanly.
