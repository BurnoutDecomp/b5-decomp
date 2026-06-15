#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"

// BrnPhysics::Deformation::DeformationOutputInterface::Construct  @ 0x8228F1B0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Constructs the five fixed-capacity
// deformation event queues (each points itself at its inline buffer, sets the
// capacity, and clears the live count) and clears the two scalar flags. The X360
// pseudocode additionally re-zeroes each queue's length immediately after
// constructing it; that is redundant (Construct already cleared it) and is omitted.

namespace BrnPhysics
{
namespace Deformation
{
    void DeformationOutputInterface::Construct()
    {
        mBrokenJointNotificationQueue.Construct();
        mDetachedPartNotificationQueue.Construct();
        mJointedPartStateQueue.Construct();
        mGlassSmashOrCrackQueue.Construct();
        mDetachedPartCurrentPositionQueue.Construct();

        muClearedFlag0 = 0;
        muClearedFlag1 = 0;
    }
}
}
