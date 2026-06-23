// BrnDirector::SharedCameraContainer -- the Director arbitrator's gameplay-camera-header
// lookup. Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82219718, semantic-parity (not
// byte-matching).
//
// Bodied here (1 ledger function):
//   SharedCameraContainer::GetGameplayCameraHe   @0x82219718
//
// The asm builds a single select bit -- (mbPrimaryActive != 0) AND (mbPrimarySuspended ==
// 0) -- and on that bit copies the primary handle's held header pointer into the output,
// otherwise the secondary handle's. The two callee accessors (sub_822122F0 /
// sub_822124A0) each assert the handle is allocated then write its held pointer (handle +
// 0x04) into the output's first word; that deref is reproduced inline by-name.

#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"

namespace BrnDirector
{
    // @0x82219718.
    GameplayCameraHeaderRef* SharedCameraContainer::GetGameplayCameraHe(
        GameplayCameraHeaderRef& lrOut) const
    {
        // lbz 0(this) / lbz 1(this): primary is usable only when active AND not suspended.
        const bool lbUsePrimary = mbPrimaryActive && !mbPrimarySuspended;

        // The handle accessors assert IsAllocated() on console; here we copy the held
        // header pointer (handle + 0x04) into the output, matching *a1 = *(handle + 4).
        lrOut.mpHeader = lbUsePrimary ? mPrimaryRef.mpHeld : mSecondaryRef.mpHeld;
        return &lrOut;
    }
}
