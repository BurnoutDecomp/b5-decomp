#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x821FB3F8
//   (BrnDirector::Camera::BehaviourSpirallingDeathcam::Prepare)
//
// Behaviour-faithful to the X360 pseudocode:
//     *(this + 8) = 0;          // reset a state field
//     return 1;                 // prepared OK
//
// (Replacement TU for the blocked Zone::IsPointInZone, to keep the batch at 50.)

namespace BrnDirector { namespace Camera {

    struct BehaviourSpirallingDeathcam
    {
        u8   mPad[8];           // [0x00] opaque (base Behaviour fields)
        u32  muField8;          // [0x08] reset on Prepare

        bool Prepare();
    };

    bool BehaviourSpirallingDeathcam::Prepare()
    {
        muField8 = 0;
        return true;
    }

}}
