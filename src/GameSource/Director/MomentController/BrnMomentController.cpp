// Out-of-line bodies for the BrnDirector::MomentController nested helpers.
// Reconstructed from BURNOUT_X360_ARTIST.XEX, semantic-parity.
//
// Bodied here:
//   BrnDirector::MomentController::MomentHandle::GetMoment  @0x821F5798
//
// MomentDescription is a plain POD (no out-of-line member needs a body here); it is homed
// purely by the header and instantiated through Array<MomentDescription,10> in its own TU.

#include "GameSource/Director/MomentController/BrnMomentController.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (mbIsAllocated guard)

namespace BrnDirector
{

// @0x821F5798. Asserts mbIsAllocated (this+0x00; "mbIsAllocated" at BrnMomentController.h:150)
// then returns the held moment pointer (lwz r3, 4(r31) == this+0x04 == mpMoment).
Moment* MomentController::MomentHandle::GetMoment() const
{
    CGS_ASSERT(mbIsAllocated, "mbIsAllocated");
    return mpMoment;
}

} // namespace BrnDirector
