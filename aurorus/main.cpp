/** aurorus
 *
 *  Morphing chorus / flanger / phaser modulation effect.
 *  KNOB_WARP morphs chorus -> flanger -> phaser.
 *  KNOB_TIME, KNOB_BLUR, KNOB_REFLECT, KNOB_MIX, KNOB_ATMOSPHERE, and
 *  KNOB_WARP drive rate, depth, feedback, wet/dry mix, stereo width,
 *  and morph position. Each has a matching CV input, summed with its
 *  knob and clamped to 0-1 (CV_WARP's raw value is uncalibrated per
 *  the SDK - it's normally reserved for V/oct pitch tracking - but
 *  works fine as a plain offset here). The LED blend colour reflects
 *  the combined (post-CV) Warp value, not just the knob.
 *  SW_FREEZE toggles freezing the current modulation phase.
 *  SW_REVERSE toggles inverting the wet signal polarity.
 *  The LEDs breathe at the combined (post-CV) Time rate, with the combined
 *  Blur value setting how deep the pulse swings; SW_FREEZE holds the
 *  breath too.
 */
#include "aurora.h"
#include "modulation.h"
#include "blend_colour.h"
#include "led_breath.h"

using namespace daisy;
using namespace aurora;

Hardware         hw;
ModulationEngine engine;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();

    engine.SetMorph(Clamp01(hw.GetKnobValue(KNOB_WARP) + hw.GetCvValue(CV_WARP)));
    engine.SetRate(Clamp01(hw.GetKnobValue(KNOB_TIME) + hw.GetCvValue(CV_TIME)));
    engine.SetDepth(Clamp01(hw.GetKnobValue(KNOB_BLUR) + hw.GetCvValue(CV_BLUR)));
    engine.SetFeedback(Clamp01(hw.GetKnobValue(KNOB_REFLECT) + hw.GetCvValue(CV_REFLECT)));
    engine.SetMix(Clamp01(hw.GetKnobValue(KNOB_MIX) + hw.GetCvValue(CV_MIX)));
    engine.SetWidth(Clamp01(hw.GetKnobValue(KNOB_ATMOSPHERE) + hw.GetCvValue(CV_ATMOSPHERE)));
    if (hw.GetButton(SW_FREEZE).RisingEdge())
        engine.ToggleFreeze();
    if (hw.GetButton(SW_REVERSE).RisingEdge())
        engine.ToggleReversePolarity();

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

    float breath_phase = 0.f;

    while (1)
    {
        hw.ClearLeds();

        float rate01  = Clamp01(hw.GetKnobValue(KNOB_TIME) + hw.GetCvValue(CV_TIME));
        float depth01 = Clamp01(hw.GetKnobValue(KNOB_BLUR) + hw.GetCvValue(CV_BLUR));
        bool  frozen  = engine.IsFrozen();

        // dt is nominal, not measured: LED SPI writes and knob reads add a
        // little real time on top of the Delay below, so the breath runs
        // marginally slower than the labelled Time rate. Fine for a mood LED.
        if (!frozen)
            breath_phase = AdvancePhase(breath_phase, rate01, kLedUpdateIntervalMs / 1000.f);

        float brightness = BreathBrightness(breath_phase, depth01);

        Rgb c = blendColour(Clamp01(hw.GetKnobValue(KNOB_WARP) + hw.GetCvValue(CV_WARP)));
        c.r *= brightness;
        c.g *= brightness;
        c.b *= brightness;

        for (int i = 0; i < 6; i++)
            hw.SetLed(numberedLeds[i], c.r, c.g, c.b);

        for (int i = 0; i < 3; i++)
            hw.SetLed(bottomLeds[i], c.r, c.g, c.b);

        if (engine.IsFrozen())
            hw.SetLed(LED_FREEZE, 1.f, 1.f, 1.f);

        if (engine.IsReversed())
            hw.SetLed(LED_REVERSE, 1.f, 1.f, 1.f);

        hw.WriteLeds();

        System::Delay(static_cast<uint32_t>(kLedUpdateIntervalMs));
    }
}
