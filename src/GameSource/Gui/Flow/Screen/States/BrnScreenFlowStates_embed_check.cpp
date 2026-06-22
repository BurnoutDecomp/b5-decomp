// Embed check for the Gui-screen group: BrnCarSelectOnlineEnd /
// BrnCrashNavAccountManagement / BrnCrashNavColourCalibrate. Compile-only (cl /c)
// exercise of each header's one ledger function, the inline virtual GetResourcesToLoad
// override over the committed CgsGui::State base and committed CgsGui::sResourceTuple.
// No link, so the .rdata statics owned by these TUs stay external.

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectOnlineEnd.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavAccountManagement.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavColourCalibrate.h"

// Each screen flow state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::CarSelectOnlineEnd),
              "CarSelectOnlineEnd : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::CrashNavAccountManagement),
              "CrashNavAccountManagement : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::CrashNavColourCalibrate),
              "CrashNavColourCalibrate : CgsGui::State");

void brn_screen_flow_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::CarSelectOnlineEnd::*lpfnCarSel)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CarSelectOnlineEnd::GetResourcesToLoad;
    void (BrnGui::CrashNavAccountManagement::*lpfnAccount)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CrashNavAccountManagement::GetResourcesToLoad;
    void (BrnGui::CrashNavColourCalibrate::*lpfnColour)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CrashNavColourCalibrate::GetResourcesToLoad;
    (void)lpfnCarSel; (void)lpfnAccount; (void)lpfnColour;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::CarSelectOnlineEnd*        lpCarSel = 0;
    BrnGui::CrashNavAccountManagement* lpAccount = 0;
    BrnGui::CrashNavColourCalibrate*   lpColour = 0;
    if (lpCarSel)  lpCarSel->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpAccount) lpAccount->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpColour)  lpColour->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
