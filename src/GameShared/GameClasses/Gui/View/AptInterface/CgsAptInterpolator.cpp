#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824E5BC8
//   (CgsGui::Interpolator::GetCurrentValue)
//
// Behaviour-faithful to the X360 pseudocode:
//     return *(this + 20);          // returned as double
//
// The current interpolated value is stored at byte offset 0x14 and returned as a
// double. (Hex-Rays types the load as a double at +0x14; modelled as such.)

namespace CgsGui
{
    struct Interpolator
    {
        u8   mPad[20];          // [0x00] opaque
        f64  mfCurrentValue;    // [0x14] current interpolated value

        double GetCurrentValue() const;
    };

    double Interpolator::GetCurrentValue() const
    {
        return mfCurrentValue;
    }
}
