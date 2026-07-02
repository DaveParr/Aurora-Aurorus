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
