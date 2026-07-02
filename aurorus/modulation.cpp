#include "modulation.h"

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
    for (int i = 0; i < kPhaserPoles; i++)
    {
        phaserL_[i].SetLfoDepth(depth01);
        phaserR_[i].SetLfoDepth(depth01);
    }
}

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

void ModulationEngine::SetMix(float mix01)
{
    mix01_ = mix01;
}

void ModulationEngine::SetWidth(float width01)
{
    width01_ = width01;
    UpdateRates();
}

void ModulationEngine::SetFreeze(bool freeze)
{
    freeze_ = freeze;
    UpdateRates();
}

void ModulationEngine::SetReversePolarity(bool reversed)
{
    reverse_ = reversed;
}

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
