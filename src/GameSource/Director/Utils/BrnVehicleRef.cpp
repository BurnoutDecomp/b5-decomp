#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x821F7798
//   (BrnDirector::VehicleRef::Construct)
//
// Behaviour-faithful to the X360 pseudocode:
//     *(result + 12) = 0;
//     return result;
//
// Zeroes the single owned reference at +0x0C and returns `this`.

namespace BrnDirector
{
    struct VehicleRef
    {
        u8    mPad0[12];   // [0x00] opaque
        void* mpRef;       // [0x0C] cleared reference

        VehicleRef* Construct();
    };

    VehicleRef* VehicleRef::Construct()
    {
        mpRef = nullptr;
        return this;
    }
}
