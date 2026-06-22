// Embed check for the Gui-crashnav group: BrnCrashNavDriverDetails /
// BrnCrashNavEnterOnline / BrnCrashNavSettings. Compile-only (cl /c) exercise of each
// header's one ledger function, the inline virtual GetResourcesToLoad override over the
// committed CgsGui::State base and committed CgsGui::sResourceTuple. No link, so the
// .rdata statics and the out-of-line virtuals owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavDriverDetails.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavSettings.h"

// Each crash-nav state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::CrashNavDriverDetails),
              "CrashNavDriverDetails : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::CrashNavEnterOnlineBase),
              "CrashNavEnterOnlineBase : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::CrashNavSettings),
              "CrashNavSettings : CgsGui::State");

void brn_crashnav_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::CrashNavDriverDetails::*lpfnDriver)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CrashNavDriverDetails::GetResourcesToLoad;
    void (BrnGui::CrashNavEnterOnlineBase::*lpfnOnline)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CrashNavEnterOnlineBase::GetResourcesToLoad;
    void (BrnGui::CrashNavSettings::*lpfnSettings)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CrashNavSettings::GetResourcesToLoad;
    (void)lpfnDriver; (void)lpfnOnline; (void)lpfnSettings;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::CrashNavDriverDetails*   lpDriver = 0;
    BrnGui::CrashNavEnterOnlineBase* lpOnline = 0;
    BrnGui::CrashNavSettings*        lpSettings = 0;
    if (lpDriver)   lpDriver->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpOnline)   lpOnline->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpSettings) lpSettings->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
