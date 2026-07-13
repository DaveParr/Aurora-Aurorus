#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <initializer_list>
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
    reversed.ToggleReversePolarity();

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
    reversed.ToggleReversePolarity();

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

TEST_CASE("freeze halts the modulation sweep") {
    // Proves freeze stops the sweep from a non-trivial running state -
    // the realistic regression to guard against. This does NOT prove the
    // held phase equals the exact pre-freeze phase (a hypothetical
    // "reset phase to 0, then freeze" bug would also produce a frozen,
    // self-repeating output and pass this test); it only shows the
    // output stops progressing once frozen. Freezing immediately after
    // Init would be a strictly weaker test than even this, since the
    // phase is already at its default 0 - the pre-warmup at least
    // confirms freeze works mid-sweep, not just at t=0.
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

    frozen.ToggleFreeze();

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

TEST_CASE("Clamp01 - clamps below zero") {
    CHECK(Clamp01(-0.3f) == doctest::Approx(0.0f).epsilon(kEps));
}

TEST_CASE("Clamp01 - clamps above one") {
    CHECK(Clamp01(1.4f) == doctest::Approx(1.0f).epsilon(kEps));
}

TEST_CASE("Clamp01 - passes through an in-range value unchanged") {
    CHECK(Clamp01(0.6f) == doctest::Approx(0.6f).epsilon(kEps));
}

TEST_CASE("Clamp01 - exact lower boundary") {
    CHECK(Clamp01(0.0f) == doctest::Approx(0.0f).epsilon(kEps));
}

TEST_CASE("Clamp01 - exact upper boundary") {
    CHECK(Clamp01(1.0f) == doctest::Approx(1.0f).epsilon(kEps));
}

TEST_CASE("MapRate01ToHz spans the documented 0.1-10 Hz range") {
    CHECK(MapRate01ToHz(0.0f) == doctest::Approx(0.1f).epsilon(kEps));
    CHECK(MapRate01ToHz(1.0f) == doctest::Approx(10.0f).epsilon(kEps));
}

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
