// Embed check for the Gui-last group: the final four Gui/Flow flow-states
// (BrnOnlineViewChallenges / BrnVideo / OnlineYouWin / BrnIdleHudState).
// Compile-only (cl /c) exercise of each header's one ledger function, the inline virtual
// GetResourcesToLoad override over the committed CgsGui::State base and committed
// CgsGui::sResourceTuple. No link, so the .rdata/.data statics and the out-of-line
// virtuals owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineViewChallenges.h"
#include "GameSource/Gui/Flow/Screen/States/BrnVideo.h"
#include "GameSource/Gui/Flow/Screen/States/OnlineYouWin.h"
#include "GameSource/Gui/Flow/hud/States/BrnIdleHudState.h"

// Each state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineViewChallenges),
              "OnlineViewChallenges : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::Video),
              "Video : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineYouWin),
              "OnlineYouWin : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::IdleHudState),
              "IdleHudState : CgsGui::State");

void brn_gui_last_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::OnlineViewChallenges::*lpfnViewChallenges)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineViewChallenges::GetResourcesToLoad;
    void (BrnGui::Video::*lpfnVideo)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::Video::GetResourcesToLoad;
    void (BrnGui::OnlineYouWin::*lpfnYouWin)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineYouWin::GetResourcesToLoad;
    void (BrnGui::IdleHudState::*lpfnIdleHud)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::IdleHudState::GetResourcesToLoad;
    (void)lpfnViewChallenges; (void)lpfnVideo; (void)lpfnYouWin; (void)lpfnIdleHud;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::OnlineViewChallenges* lpViewChallenges = 0;
    BrnGui::Video*                lpVideo = 0;
    BrnGui::OnlineYouWin*         lpYouWin = 0;
    BrnGui::IdleHudState*         lpIdleHud = 0;
    if (lpViewChallenges) lpViewChallenges->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpVideo)          lpVideo->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpYouWin)         lpYouWin->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpIdleHud)        lpIdleHud->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
