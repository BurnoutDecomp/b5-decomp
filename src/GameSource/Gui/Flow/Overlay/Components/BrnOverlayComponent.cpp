#include "GameSource/Gui/Flow/Overlay/Components/BrnOverlayComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
//   TransitionCompleteCallback @0x8241C198 -- the apt "transition complete" timeline
//   callback. The apt system hands the OverlayComponent back as the user-data pointer;
//   the callback asserts it is non-null, then raises the component's transition-complete
//   flag. The X360 inlines SetTransitionComplete to a direct `stb 1, 0x14`; we restore the
//   named setter call. The second callback argument (a u16 timeline/frame tag) is unused
//   by this handler (the X360 never touches r4).

namespace BrnGui
{

// @ 0x8241C198
void OverlayComponent::TransitionCompleteCallback(void* lpUserData, u16 /*luArg*/)
{
    CGS_ASSERT(NULL != lpUserData, "NULL != lpUserData");

    OverlayComponent* lpThis = static_cast<OverlayComponent*>(lpUserData);
    lpThis->SetTransitionComplete(true);
}

} // namespace BrnGui
