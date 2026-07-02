#pragma once

#include <cmath>

struct MorphWeights { float chorus, flanger, phaser; };

inline MorphWeights ComputeMorphWeights(float morph01)
{
    MorphWeights w;
    if (morph01 <= 0.5f)
    {
        float t   = morph01 / 0.5f;
        w.chorus  = std::sqrt(1.f - t);
        w.flanger = std::sqrt(t);
        w.phaser  = 0.f;
    }
    else
    {
        float t   = (morph01 - 0.5f) / 0.5f;
        w.chorus  = 0.f;
        w.flanger = std::sqrt(1.f - t);
        w.phaser  = std::sqrt(t);
    }
    return w;
}
