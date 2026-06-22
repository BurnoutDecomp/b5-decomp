// Embed check for the Gui-C group: BrnOnlineGameOptionsSummary / BrnOnlineLoading /
// BrnOnlineNews. Compile-only (cl /c) exercise of each header's one ledger function, the
// inline virtual GetResourcesToLoad override over the committed CgsGui::State base and the
// committed CgsGui::sResourceTuple. No link, so the .rdata statics owned by these TUs stay
// external.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptionsSummary.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineLoading.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineNews.h"

// Each screen flow state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineGameOptionsSummary),
              "OnlineGameOptionsSummary : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineLoading),
              "OnlineLoading : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::OnlineNews),
              "OnlineNews : CgsGui::State");

void brn_online_flow_states_c_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::OnlineGameOptionsSummary::*lpfnSummary)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineGameOptionsSummary::GetResourcesToLoad;
    void (BrnGui::OnlineLoading::*lpfnLoading)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineLoading::GetResourcesToLoad;
    void (BrnGui::OnlineNews::*lpfnNews)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::OnlineNews::GetResourcesToLoad;
    (void)lpfnSummary; (void)lpfnLoading; (void)lpfnNews;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::OnlineGameOptionsSummary* lpSummary = 0;
    BrnGui::OnlineLoading*            lpLoading = 0;
    BrnGui::OnlineNews*               lpNews = 0;
    if (lpSummary) lpSummary->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpLoading) lpLoading->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpNews)    lpNews->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
