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
