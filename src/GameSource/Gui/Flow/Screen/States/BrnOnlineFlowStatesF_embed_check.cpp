// Embed check for the Gui-F group: BrnOnlineScoreboards / BrnOnlineSelectRoute /
// BrnOnlineStats. Compile-only (cl /c) exercise of each header's one ledger function, the
// inline virtual GetResourcesToLoad override over the committed CgsGui::State base and committed
// CgsGui::sResourceTuple. No link, so the .rdata statics and the out-of-line virtuals owned by
// the class: TUs stay external.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineStats.h"

// Each online state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineScoreboards),
              "OnlineScoreboards : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineSelectRoute),
              "OnlineSelectRoute : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineStats),
              "OnlineStats : CgsGui::State");

void brn_online_flow_states_f_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::OnlineScoreboards::*lpfnScoreboards)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineScoreboards::GetResourcesToLoad;
    void (BrnGui::OnlineSelectRoute::*lpfnRoute)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineSelectRoute::GetResourcesToLoad;
    void (BrnGui::OnlineStats::*lpfnStats)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineStats::GetResourcesToLoad;
    (void)lpfnScoreboards; (void)lpfnRoute; (void)lpfnStats;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::OnlineScoreboards* lpScoreboards = 0;
    BrnGui::OnlineSelectRoute* lpRoute       = 0;
    BrnGui::OnlineStats*       lpStats       = 0;
    if (lpScoreboards) lpScoreboards->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpRoute)       lpRoute->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpStats)       lpStats->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
