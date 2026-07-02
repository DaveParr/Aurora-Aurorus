#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
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
