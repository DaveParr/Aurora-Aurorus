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
