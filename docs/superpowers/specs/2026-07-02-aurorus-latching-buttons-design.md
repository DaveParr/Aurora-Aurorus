# Aurorus: Latching Freeze & Reverse Buttons

**Date:** 2026-07-02
**Project:** `aurorus`

## Overview

Change `SW_FREEZE` and `SW_REVERSE` from momentary (effect active only while
the button is held) to latching (a press toggles the effect on; it stays on
until pressed again). Both `LED_FREEZE`/`LED_REVERSE` switch from "lit while
held" to "lit while toggled on," so the LED always reflects current state
rather than current button contact.

This changes user-facing behavior only — the underlying DSP (freeze via
`SetLfoFreq(0)`, reverse via wet-signal sign inversion) is unchanged. See
`docs/superpowers/specs/2026-07-02-aurorus-modulation-design.md` for that.

---

## Current Behavior (baseline)

```cpp
// main.cpp AudioCallback
engine.SetFreeze(hw.GetButton(SW_FREEZE).Pressed());
engine.SetReversePolarity(hw.GetButton(SW_REVERSE).Pressed());

// main.cpp LED loop
if (hw.GetButton(SW_FREEZE).Pressed())
    hw.SetLed(LED_FREEZE, 1.f, 1.f, 1.f);
if (hw.GetButton(SW_REVERSE).Pressed())
    hw.SetLed(LED_REVERSE, 1.f, 1.f, 1.f);
```

`daisy::Switch::Pressed()` is read directly every audio callback and passed
straight through as engine state — no edge detection, no toggle bookkeeping.
Button state and LED state are each read independently from `hw`, once in the
audio callback and once in the LED loop.

`daisy::Switch` (`lib/Aurora-SDK/libs/libDaisy/src/hid/switch.h`) already
provides `RisingEdge()` in addition to `Pressed()`. `Debounce()` — and
therefore the window in which `RisingEdge()` is valid — runs once per audio
callback via `Hardware::ProcessAllControls()`, matching where button state is
read today. This gives a reliable one-shot-per-press signal without adding
any new debounce logic.

---

## New Behavior

- First press of `SW_FREEZE` turns freeze on; it stays on — through knob
  changes, releasing the button, indefinitely — until `SW_FREEZE` is pressed
  again.
- Same for `SW_REVERSE`.
- The two buttons toggle independently.
- State is not persisted across power cycles; both default to off at boot,
  matching every other control in this firmware (no calibration/persistent
  storage exists — see Out of Scope in the modulation design).

---

## `ModulationEngine` API changes

`aurorus/modulation.h` / `aurorus/modulation.cpp`

Remove:
```cpp
void SetFreeze(bool freeze);
void SetReversePolarity(bool reversed);
```

Add:
```cpp
void ToggleFreeze();
void ToggleReversePolarity();
bool IsFrozen() const   { return freeze_; }
bool IsReversed() const { return reverse_; }
```

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

`freeze_`/`reverse_` keep their existing `false` default-member-initializers.
`UpdateRates()` is still called on every freeze change, same as today, so the
`SetLfoFreq(0)` freeze mechanism from the modulation design is untouched.

The engine becomes the single source of truth for freeze/reverse state —
previously `main.cpp` read `hw.GetButton(...).Pressed()` independently in two
places (audio callback and LED loop); now both read through the engine.

---

## `main.cpp` changes

Audio callback — toggle on rising edge instead of passing `Pressed()` through:

```cpp
if (hw.GetButton(SW_FREEZE).RisingEdge())
    engine.ToggleFreeze();
if (hw.GetButton(SW_REVERSE).RisingEdge())
    engine.ToggleReversePolarity();
```

LED loop — read engine state instead of button state:

```cpp
if (engine.IsFrozen())
    hw.SetLed(LED_FREEZE, 1.f, 1.f, 1.f);
if (engine.IsReversed())
    hw.SetLed(LED_REVERSE, 1.f, 1.f, 1.f);
```

The file header comment (`main.cpp:7-8`, "SW_FREEZE holds the current
modulation phase" / "SW_REVERSE inverts the wet signal polarity") is updated
to describe toggle behavior instead of hold behavior.

---

## Testing

`aurorus/tests/test_modulation.cpp` gets new doctest cases exercising the
toggle API directly on `ModulationEngine` — no hardware dependency, same
pattern as the file's existing cases:

- Freeze starts off (`IsFrozen() == false` on a freshly `Init`'d engine)
- Reverse starts off (`IsReversed() == false`)
- One `ToggleFreeze()` call turns freeze on
- A second `ToggleFreeze()` call turns freeze back off
- Same on/off/on pattern for `ToggleReversePolarity()`
- Toggling one does not change the other's state

No changes to `test_blend_colour.cpp` — LED colour math is unaffected.

---

## Documentation

`README.md` currently describes both buttons as hold-to-activate in four
places; all four are updated to press-to-toggle language:

- Feature bullets (Freeze / Reverse polarity descriptions)
- Front-panel control table (Freeze / Reverse rows)
- "Audio" section prose describing what holding each button does
- LED table (Freeze LED / Reverse LED rows)

---

## Out of Scope

- `SW_SHIFT` (still unused/dead)
- CV gate inputs for Freeze/Reverse (still unused — front-panel button only)
- Persisting latch state across power cycles
- Debounce tuning (existing `daisy::Switch` debounce is unchanged and sufficient)
