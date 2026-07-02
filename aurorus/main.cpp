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
