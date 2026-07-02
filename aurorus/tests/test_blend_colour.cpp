#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../blend_colour.h"

static const float kEps = 1e-4f;

TEST_CASE("blendColour - pure chorus (blue) at morph 0.0") {
    Rgb c = blendColour(0.f);
    CHECK(c.r == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(1.f).epsilon(kEps));
}

TEST_CASE("blendColour - pure flanger (green) at morph 0.5") {
    Rgb c = blendColour(0.5f);
    CHECK(c.r == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(1.f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(0.f).epsilon(kEps));
}

TEST_CASE("blendColour - pure phaser (magenta) at morph 1.0") {
    Rgb c = blendColour(1.f);
    CHECK(c.r == doctest::Approx(1.f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(1.f).epsilon(kEps));
}

TEST_CASE("blendColour - halfway between chorus and flanger at morph 0.25") {
    Rgb c = blendColour(0.25f);
    CHECK(c.r == doctest::Approx(0.f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(0.5f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(0.5f).epsilon(kEps));
}

TEST_CASE("blendColour - halfway between flanger and phaser at morph 0.75") {
    Rgb c = blendColour(0.75f);
    CHECK(c.r == doctest::Approx(0.5f).epsilon(kEps));
    CHECK(c.g == doctest::Approx(0.5f).epsilon(kEps));
    CHECK(c.b == doctest::Approx(0.5f).epsilon(kEps));
}
