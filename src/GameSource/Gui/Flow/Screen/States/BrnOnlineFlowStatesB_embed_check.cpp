// Embed check for the Gui-B group: BrnOnlineCreateFreeburn / BrnOnlineCustomMatch /
// BrnOnlineGameOptions. Compile-only (cl /c) exercise of each header's one ledger
// function, the inline virtual GetResourcesToLoad override over the committed
// CgsGui::State base and committed CgsGui::sResourceTuple. No link, so the .rdata statics
// and the out-of-line virtuals owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCreateFreeburn.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"

// Each online state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineCreateFreeburn),
              "OnlineCreateFreeburn : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineCustomMatch),
              "OnlineCustomMatch : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineGameOptions),
              "OnlineGameOptions : CgsGui::State");

void brn_online_flow_states_b_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::OnlineCreateFreeburn::*lpfnFreeburn)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineCreateFreeburn::GetResourcesToLoad;
    void (BrnGui::OnlineCustomMatch::*lpfnCustomMatch)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineCustomMatch::GetResourcesToLoad;
    void (BrnGui::OnlineGameOptions::*lpfnGameOptions)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineGameOptions::GetResourcesToLoad;
    (void)lpfnFreeburn; (void)lpfnCustomMatch; (void)lpfnGameOptions;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::OnlineCreateFreeburn* lpFreeburn    = 0;
    BrnGui::OnlineCustomMatch*    lpCustomMatch = 0;
    BrnGui::OnlineGameOptions*    lpGameOptions = 0;
    if (lpFreeburn)    lpFreeburn->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpCustomMatch) lpCustomMatch->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpGameOptions) lpGameOptions->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
