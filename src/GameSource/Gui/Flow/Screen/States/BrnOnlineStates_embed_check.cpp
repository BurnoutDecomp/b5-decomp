// Embed check for the Gui-D group: BrnOnlinePause / BrnOnlinePlay / BrnOnlinePreEvent.
// Compile-only (cl /c) exercise of each header's one ledger function, the inline virtual
// GetResourcesToLoad override over the committed CgsGui::State base and committed
// CgsGui::sResourceTuple. No link, so the .rdata statics and the out-of-line virtuals
// owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePause.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePlay.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePreEvent.h"

// Each online state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlinePause),
              "OnlinePause : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlinePlay),
              "OnlinePlay : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlinePreEvent),
              "OnlinePreEvent : CgsGui::State");

void brn_online_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::OnlinePause::*lpfnPause)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlinePause::GetResourcesToLoad;
    void (BrnGui::OnlinePlay::*lpfnPlay)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlinePlay::GetResourcesToLoad;
    void (BrnGui::OnlinePreEvent::*lpfnPreEvent)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlinePreEvent::GetResourcesToLoad;
    (void)lpfnPause; (void)lpfnPlay; (void)lpfnPreEvent;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::OnlinePause*    lpPause = 0;
    BrnGui::OnlinePlay*     lpPlay = 0;
    BrnGui::OnlinePreEvent* lpPreEvent = 0;
    if (lpPause)    lpPause->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpPlay)     lpPlay->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpPreEvent) lpPreEvent->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
