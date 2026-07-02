#pragma once

struct Rgb { float r, g, b; };

inline Rgb lerp(const Rgb &a, const Rgb &b, float t)
{
    return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

inline Rgb blendColour(float morph01)
{
    constexpr Rgb kChorus  = {0.f, 0.f, 1.f}; // blue
    constexpr Rgb kFlanger = {0.f, 1.f, 0.f}; // green
    constexpr Rgb kPhaser  = {1.f, 0.f, 1.f}; // magenta

    if (morph01 <= 0.5f)
        return lerp(kChorus, kFlanger, morph01 / 0.5f);

    return lerp(kFlanger, kPhaser, (morph01 - 0.5f) / 0.5f);
}
