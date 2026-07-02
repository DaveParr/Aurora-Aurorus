#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../led_breath.h"

static const float kEps     = 1e-3f;
static const float kHalfPi  = 1.5707963f;
static const float kThreeHalfPi = 4.7123890f;

TEST_CASE("BreathBrightness - depth 0 is steady at 1.0 regardless of phase") {
    CHECK(BreathBrightness(0.f, 0.f) == doctest::Approx(1.f).epsilon(kEps));
    CHECK(BreathBrightness(kHalfPi, 0.f) == doctest::Approx(1.f).epsilon(kEps));
    CHECK(BreathBrightness(kThreeHalfPi, 0.f) == doctest::Approx(1.f).epsilon(kEps));
}

TEST_CASE("BreathBrightness - depth 1.0 peaks at 1.0 and troughs near 0.0") {
    CHECK(BreathBrightness(kHalfPi, 1.f) == doctest::Approx(1.f).epsilon(kEps));
    CHECK(BreathBrightness(kThreeHalfPi, 1.f) == doctest::Approx(0.f).epsilon(kEps));
}

TEST_CASE("BreathBrightness - depth 0.5 swings between 1.0 and 0.5") {
    CHECK(BreathBrightness(kHalfPi, 0.5f) == doctest::Approx(1.f).epsilon(kEps));
    CHECK(BreathBrightness(kThreeHalfPi, 0.5f) == doctest::Approx(0.5f).epsilon(kEps));
}

TEST_CASE("AdvancePhase - min rate (0.05Hz) advances a small fraction per second") {
    float phase = AdvancePhase(0.f, 0.f, 1.f);
    CHECK(phase == doctest::Approx(0.31415927f).epsilon(kEps));
}

TEST_CASE("AdvancePhase - max rate (5Hz) wraps past 2*pi correctly") {
    // 5Hz * 0.9s = 4.5 cycles -> wraps to exactly pi radians past a full cycle.
    float phase = AdvancePhase(0.f, 1.f, 0.9f);
    CHECK(phase == doctest::Approx(3.14159265f).epsilon(kEps));
    CHECK(phase >= 0.f);
    CHECK(phase < 6.2831853f);
}
