#pragma once

#include <cmath>

struct MorphWeights { float chorus, flanger, phaser; };

inline MorphWeights ComputeMorphWeights(float morph01)
{
    MorphWeights w;
    if (morph01 <= 0.5f)
    {
        float t   = morph01 / 0.5f;
        w.chorus  = 1.f - t;
        w.flanger = t;
        w.phaser  = 0.f;
    }
    else
    {
        float t   = (morph01 - 0.5f) / 0.5f;
        w.chorus  = 0.f;
        w.flanger = 1.f - t;
        w.phaser  = t;
    }
    return w;
}
