// Embed check for the Gui-postevent-2 group: BrnOfflineRivalShutdown /
// BrnOfflineTrophyCarUnlock / BrnOnlineInstantResultsState. Compile-only (cl /c)
// exercise of each header's one ledger function, the inline virtual
// GetResourcesToLoad override over the committed CgsGui::State base and committed
// CgsGui::sResourceTuple. No link, so the .rdata statics owned by these TUs stay
// external.

#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineRivalShutdown.h"
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineTrophyCarUnlock.h"
#include "GameSource/Gui/Flow/PostEvent/States/Online/BrnOnlineInstantResults.h"

// Each post-event state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::OfflineRivalShutdown),
              "OfflineRivalShutdown : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OfflineTrophyCarUnlock),
              "OfflineTrophyCarUnlock : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineInstantResultsState),
              "OnlineInstantResultsState : CgsGui::State");

void brn_postevent_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::OfflineRivalShutdown::*lpfnRival)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OfflineRivalShutdown::GetResourcesToLoad;
    void (BrnGui::OfflineTrophyCarUnlock::*lpfnTrophy)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OfflineTrophyCarUnlock::GetResourcesToLoad;
    void (BrnGui::OnlineInstantResultsState::*lpfnInstant)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineInstantResultsState::GetResourcesToLoad;
    (void)lpfnRival; (void)lpfnTrophy; (void)lpfnInstant;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::OfflineRivalShutdown*      lpRival = 0;
    BrnGui::OfflineTrophyCarUnlock*    lpTrophy = 0;
    BrnGui::OnlineInstantResultsState* lpInstant = 0;
    if (lpRival)   lpRival->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpTrophy)  lpTrophy->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpInstant) lpInstant->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
