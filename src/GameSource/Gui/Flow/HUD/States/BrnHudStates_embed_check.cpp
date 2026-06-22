// Embed check for the Gui-HUD-states group: BrnRaceMainHudState / BrnFBurnMainHudState
// / BrnCrashedStuntHudState. Compile-only (cl /c) exercise of each header's one ledger
// function, the inline virtual GetResourcesToLoad override over the committed CgsGui::State
// base and committed CgsGui::sResourceTuple. No link, so the .rdata statics and the
// out-of-line virtuals owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnFBurnMainHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnCrashedStuntHudState.h"

// Each derived state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::RaceMainHudState),    "RaceMainHudState : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::FBurnMainHudState),   "FBurnMainHudState : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::CrashedStuntHudState),"CrashedStuntHudState : CgsGui::State");

void brn_hud_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::RaceMainHudState::*lpfnRace)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::RaceMainHudState::GetResourcesToLoad;
    void (BrnGui::FBurnMainHudState::*lpfnFBurn)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::FBurnMainHudState::GetResourcesToLoad;
    void (BrnGui::CrashedStuntHudState::*lpfnCrash)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CrashedStuntHudState::GetResourcesToLoad;
    (void)lpfnRace; (void)lpfnFBurn; (void)lpfnCrash;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::RaceMainHudState* lpRace = 0;
    BrnGui::FBurnMainHudState* lpFBurn = 0;
    BrnGui::CrashedStuntHudState* lpCrash = 0;
    if (lpRace)  lpRace->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpFBurn) lpFBurn->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpCrash) lpCrash->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
