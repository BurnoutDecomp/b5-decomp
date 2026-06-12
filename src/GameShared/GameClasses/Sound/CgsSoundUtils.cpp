#include "types.hpp"

#include <cmath>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826A1F70
//   (CgsSound::Utils::Slope::Slope  — constructor)
//
// Copies a 4-float source (start, end, and two trailing values) into the Slope,
// then nudges `end` away from `start` by a tiny epsilon if they are within 1e-6,
// guaranteeing a non-zero span (so later slope/divide math never sees start==end).
// Behaviour-faithful to the X360 pseudocode (returns `this`):
//
//   result[0..3] = a2[0..3];
//   if (fabs(result[1] - result[0]) < 1e-6)  result[1] += 1e-6;

namespace CgsSound
{
    namespace Utils
    {
        struct Slope
        {
            f32 mfStart;    // [0]
            f32 mfEnd;      // [1]
            f32 mf2;        // [2]
            f32 mf3;        // [3]

            Slope* Construct(const f32* lpaSource);
        };

        static const f32 KF_MIN_SPAN = 0.000001f;

        Slope* Slope::Construct(const f32* lpaSource)
        {
            mfStart = lpaSource[0];
            mfEnd   = lpaSource[1];
            mf2     = lpaSource[2];
            mf3     = lpaSource[3];

            if (fabsf(mfEnd - mfStart) < KF_MIN_SPAN)
                mfEnd += KF_MIN_SPAN;

            return this;
        }
    }
}
