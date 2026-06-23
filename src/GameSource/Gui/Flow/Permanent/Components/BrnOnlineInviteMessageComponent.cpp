#include "BrnOnlineInviteMessageComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::OnlineInviteMessageComponent::DoTransitionComplete @ 0x824162E0
//
// The transition-out completion handler: assert the component was transitioning out,
// snapshot the secondary state, reset the showing-state to HIDDEN, and -- if the
// secondary state was 4 -- latch the dismissed flag (a byte store, hence a bool field).

namespace BrnGui
{
namespace
{
    // The secondary-state value (+0x34) that means the message should latch as dismissed.
    const s32 KI_SecondaryStateDismiss = 4;
}

void* OnlineInviteMessageComponent::DoTransitionComplete()
{
    // Guest: lwz r11, 0x30(this); cmpwi r11, 2 -- must be transitioning out.
    CGS_ASSERT(meShowingState == E_SHOWINGSTATE_TRANSITIONING_OUT,
               "meShowingState == E_SHOWINGSTATE_TRANSITIONING_OUT");

    // Guest: lwz r10, 0x34(this) -- snapshot the secondary state BEFORE clearing +0x30.
    const s32 liSecondaryState = miSecondaryState;

    // Guest: stw 0, 0x30(this) -- drop back to HIDDEN.
    meShowingState = E_SHOWINGSTATE_HIDDEN;

    // Guest: cmpwi r10, 4; stb 1, 0x68(this) -- latch dismissed on the match.
    if (liSecondaryState == KI_SecondaryStateDismiss)
    {
        mbDismissed = true;
    }

    // The X360 leaves `this` (r31/r3) in the result on the non-assert path.
    return this;
}
}
