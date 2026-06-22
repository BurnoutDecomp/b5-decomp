// Embed check for the Gui-E group: BrnOnlineQuickCustomCreate / BrnOnlineQuickMatch /
// BrnOnlineRivals. Compile-only (cl /c) exercise of each header's one ledger function,
// the inline virtual GetResourcesToLoad override over the committed CgsGui::State base
// and committed CgsGui::sResourceTuple. No link, so the .rdata statics and the
// out-of-line virtuals owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineQuickCustomCreate.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineQuickMatch.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineRivals.h"

// Each online state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineQuickCustomCreate),
              "OnlineQuickCustomCreate : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineQuickMatch),
              "OnlineQuickMatch : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineRivals),
              "OnlineRivals : CgsGui::State");

void brn_online_flow_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::OnlineQuickCustomCreate::*lpfnCustomCreate)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineQuickCustomCreate::GetResourcesToLoad;
    void (BrnGui::OnlineQuickMatch::*lpfnQuickMatch)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineQuickMatch::GetResourcesToLoad;
    void (BrnGui::OnlineRivals::*lpfnRivals)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineRivals::GetResourcesToLoad;
    (void)lpfnCustomCreate; (void)lpfnQuickMatch; (void)lpfnRivals;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::OnlineQuickCustomCreate* lpCustomCreate = 0;
    BrnGui::OnlineQuickMatch*        lpQuickMatch = 0;
    BrnGui::OnlineRivals*            lpRivals = 0;
    if (lpCustomCreate) lpCustomCreate->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpQuickMatch)   lpQuickMatch->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpRivals)       lpRivals->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
