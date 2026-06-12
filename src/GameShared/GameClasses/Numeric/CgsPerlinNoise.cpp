#include "types.hpp"

#include <cmath>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8289CCA0
//   (CgsNumeric::PerlinNoise::Noise)
//
// 1-D value noise with cosine interpolation between two integer-lattice samples.
// The X360 body is the classic integer-hash noise; reconstructed from the idiom:
//
//   floor   = floor(x)                       (computed via fsel in the asm)
//   frac    = x - floor
//   weight  = (1 - cos(frac * PI)) * 0.5     (cosine interpolation factor)
//   sample(n): h = (n << 13) ^ n;
//              r = (h * (h*h*15731 + 789221) + 1376312589) & 0x7FFFFFFF;
//              return 1 - r * (2 / 2^31)     (== 1 - r * 9.3132257e-10, in [-1,1])
//   return sample(floor)*(1 - weight) + sample(floor + 1)*weight
//
// Constant notes:
//   * 9.3132257e-10 == 2.0 / 2147483648.0 (2 / 2^31): maps r in [0, 2^31) to [0,2],
//     and `1 - that` to [-1, 1].
//   * the asm's `- 771171059` equals `+ 1376312589` after the `& 0x7FFFFFFF` mask
//     (771171059 + 1376312589 = 2^31), i.e. the standard noise additive constant.

namespace CgsNumeric
{
    namespace PerlinNoise
    {
        namespace
        {
            // Standard integer noise hash, normalised to [-1, 1].
            inline double LatticeSample(s32 liLattice)
            {
                u32 luHash = (static_cast<u32>(liLattice) << 13) ^ static_cast<u32>(liLattice);
                u32 luNoise = (luHash * (luHash * luHash * 15731u + 789221u) + 1376312589u)
                              & 0x7FFFFFFFu;
                return 1.0 - static_cast<double>(luNoise) * 9.3132257e-10;
            }
        }

        double Noise(double lfX)
        {
            const s32    liFloor = static_cast<s32>(floor(lfX));
            const double lfFrac  = lfX - static_cast<double>(liFloor);
            const double lfWeight = (1.0 - cos(lfFrac * 3.1415927)) * 0.5;

            const double lfSample0 = LatticeSample(liFloor);
            const double lfSample1 = LatticeSample(liFloor + 1);

            return lfSample0 * (1.0 - lfWeight) + lfSample1 * lfWeight;
        }
    }
}
