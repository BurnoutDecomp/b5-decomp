// Embed check for the Gui-A group: BrnCrashNavStats / BrnCrashNavTrax / BrnCredits.
// Compile-only (cl /c) exercise of each header's one ledger function, the inline virtual
// GetResourcesToLoad override over the committed CgsGui::State base and committed
// CgsGui::sResourceTuple. No link, so the .rdata statics and the out-of-line virtuals
// owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavStats.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavTrax.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCredits.h"

// Each Gui-A state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::CrashNavStats),
              "CrashNavStats : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::CrashNavTrax),
              "CrashNavTrax : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::Credits),
              "Credits : CgsGui::State");

void brn_gui_a_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::CrashNavStats::*lpfnStats)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CrashNavStats::GetResourcesToLoad;
    void (BrnGui::CrashNavTrax::*lpfnTrax)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CrashNavTrax::GetResourcesToLoad;
    void (BrnGui::Credits::*lpfnCredits)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::Credits::GetResourcesToLoad;
    (void)lpfnStats; (void)lpfnTrax; (void)lpfnCredits;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::CrashNavStats* lpStats   = 0;
    BrnGui::CrashNavTrax*  lpTrax    = 0;
    BrnGui::Credits*       lpCredits = 0;
    if (lpStats)   lpStats->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpTrax)    lpTrax->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpCredits) lpCredits->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
