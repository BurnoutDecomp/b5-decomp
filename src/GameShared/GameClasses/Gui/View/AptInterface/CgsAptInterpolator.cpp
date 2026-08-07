#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824E5BC8
//   (CgsGui::Interpolator::GetCurrentValue)
//
// Behaviour-faithful to the X360 asm:
//     lfs f1, 0x14(r3)  ;  blr        // load a 32-bit float at +0x14, return it
//
// The current interpolated value is stored at byte offset 0x14 as a 32-bit FLOAT
// and loaded with lfs (load float single), not lfd. (Evidence: the Hex-Rays
// pseudocode types the load as `double` at +0x14, but the ARTIST asm @0x824E5BC8
// is `lfs` and DecFIGS @0xBB3D08 demangles to a `float32_t`-returning
// GetCurrentValue with the same `lfs 0x14` -- so the member is an f32, loaded as
// f32. A double member + lfd would read 8 bytes and misinterpret the bit pattern.)

namespace CgsGui
{
    struct Interpolator
    {
        u8   mPad[20];          // [0x00] opaque
        f32  mfCurrentValue;    // [0x14] current interpolated value (32-bit float)

        float GetCurrentValue() const;
    };

    float Interpolator::GetCurrentValue() const
    {
        return mfCurrentValue;
    }
}
