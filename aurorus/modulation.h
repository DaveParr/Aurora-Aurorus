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
constexpr float kMaxDetune   = 0.01f; // +-1% => up to 2% L/R spread at width=1

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
    void SetWidth(float width01);
    void ToggleFreeze();
    void ToggleReversePolarity();
    bool IsFrozen() const { return freeze_; }
    bool IsReversed() const { return reverse_; }

    StereoFrame Process(StereoFrame in);

  private:
    daisysp::Chorus  chorus_;
    daisysp::Flanger flangerL_;
    daisysp::Flanger flangerR_;

    static constexpr int kPhaserPoles = 4;
    daisysp::PhaserEngine phaserL_[kPhaserPoles];
    daisysp::PhaserEngine phaserR_[kPhaserPoles];

    MorphWeights weights_ = {1.f, 0.f, 0.f};
    float        rate01_  = 0.f;
    float        mix01_   = 0.f;
    float        width01_ = 0.f;
    bool         freeze_  = false;
    bool         reverse_ = false;

    void UpdateRates();
};
