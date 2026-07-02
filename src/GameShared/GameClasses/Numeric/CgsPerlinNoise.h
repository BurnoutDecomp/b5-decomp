#ifndef GAMESHARED_GAMECLASSES_NUMERIC_CGS_PERLIN_NOISE_H
#define GAMESHARED_GAMECLASSES_NUMERIC_CGS_PERLIN_NOISE_H

// CgsNumeric::PerlinNoise -- 1-D value noise with cosine interpolation. Home header for
// the committed body in CgsPerlinNoise.cpp (@0x8289CCA0); see that TU for the derivation.

namespace CgsNumeric
{
    namespace PerlinNoise
    {
        // @0x8289CCA0. Classic integer-hash 1-D value noise in [-1, 1], cosine-interpolated
        // between the two neighbouring lattice samples.
        double Noise(double lfX);
    }
}

#endif // GAMESHARED_GAMECLASSES_NUMERIC_CGS_PERLIN_NOISE_H
