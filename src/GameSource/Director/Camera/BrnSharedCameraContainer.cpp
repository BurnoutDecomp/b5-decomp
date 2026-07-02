// BrnDirector::SharedCameraContainer -- the shared gameplay-camera selection. Reconstructed
// from BURNOUT_X360_ARTIST.XEX @0x82219718, semantic-parity (not byte-matching).
//
// Bodied here (1 ledger function):
//   SharedCameraContainer::GetGameplayCameraHelperIndex @0x82219718
//     (IDB symbol truncated to "GetGameplayCameraHe"; full name from the DWARF,
//      BrnDirectorArbitratorSharedCameraContainer.h:61)
//
// The asm builds a single select bit -- (mbUseGameplayExternal != 0) AND (mbLookbackOverride
// == 0) -- and on that bit returns the external handle's helper-index word, otherwise the
// bumper handle's. The two de-inlined accessors (sub_822122F0 / sub_822124A0) each assert
// the handle is allocated then write its +0x04 word into the sret output; that is
// BehaviourHandle::GetBehaviourHelperIndex() by name.

#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"

namespace BrnDirector
{
    // @0x82219718.
    Camera::BehaviourHelperIndex SharedCameraContainer::GetGameplayCameraHelperIndex() const
    {
        // lbz 0(this) / lbz 1(this): the external cam is live only when selected AND not
        // overridden by lookback.
        const bool lbUseExternal = mbUseGameplayExternal && !mbLookbackOverride;

        return lbUseExternal ? mGameplayExternal.GetBehaviourHelperIndex()
                             : mGameplayBumper.GetBehaviourHelperIndex();
    }
}
