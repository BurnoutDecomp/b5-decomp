#include "types.hpp"

#include <cmath>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8258B090
//   (BrnNetwork::NetworkRounder::RoundFloatToAccuracy)
//
// Path note: the X360 line-info attributed this function to a PPC toolchain
// header (.../ppu/include/ppu_intrinsics_gcc.h) because its body is the inlined
// fsel float-rounding idiom. DecFIGS func attribution places it in its real
// home, GameSource/Network/Utilities/BrnNetworkRounder.cpp — used here.
//
// Behaviour-faithful to the X360 pseudocode/asm. The original is the canonical
// PowerPC round-to-nearest expansion:
//   fmuls/fdivs (single)  -> step = a2*2, scaled = *result/step
//   fadds       (single)  -> biased = scaled + 0.5
//   two fsel + ±2^52      -> round-to-nearest-even then correct == floor(biased)
//   frsp/fmuls  (single)  -> *result = floor(biased) * step
// i.e. it snaps *result to the nearest multiple of (a2*2), rounding halves up.

namespace BrnNetwork
{
    namespace NetworkRounder
    {
        float* RoundFloatToAccuracy(float* result, double a2)
        {
            const float step   = static_cast<float>(a2 * 2.0);   // fmuls f13
            const float scaled = *result / step;                 // fdivs f12
            *result = scaled;                                    // stfs (intermediate, mirrors pseudocode)
            const float biased = scaled + 0.5f;                  // fadds f0

            // PPC fsel magic-constant rounding (±2^52) followed by the 0/1
            // floor-correction is exactly floor() of the biased value.
            const double rounded = std::floor(static_cast<double>(biased));

            *result = static_cast<float>(rounded) * step;        // frsp + fmuls
            return result;
        }
    }
}
