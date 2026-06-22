// Embed check for the Gui-boot-states group: BrnBootLegal / BrnBootPreload /
// BrnBootProfile. Compile-only (cl /c) exercise of each header's one ledger function,
// the inline virtual GetResourcesToLoad override over the committed CgsGui::State base
// and committed CgsGui::sResourceTuple. No link, so the .rdata statics and the
// out-of-line virtuals owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/HUD/States/BrnBootLegal.h"
#include "GameSource/Gui/Flow/HUD/States/BrnBootPreload.h"
#include "GameSource/Gui/Flow/HUD/States/BrnBootProfile.h"

// Each boot state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::BootLegal),   "BootLegal : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::BootPreload), "BootPreload : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::BootProfile), "BootProfile : CgsGui::State");

// The committed (id,type) tuple is two u32-sized fields; the accessor hands back a
// pointer to a table of them plus a count, so a tuple is at least 8 bytes.
static_assert(sizeof(CgsGui::sResourceTuple) >= 8, "sResourceTuple is at least {u32 id, enum type}");

void brn_boot_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::BootLegal::*lpfnLegal)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::BootLegal::GetResourcesToLoad;
    void (BrnGui::BootPreload::*lpfnPreload)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::BootPreload::GetResourcesToLoad;
    void (BrnGui::BootProfile::*lpfnProfile)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::BootProfile::GetResourcesToLoad;
    (void)lpfnLegal; (void)lpfnPreload; (void)lpfnProfile;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::BootLegal*   lpLegal = 0;
    BrnGui::BootPreload* lpPreload = 0;
    BrnGui::BootProfile* lpProfile = 0;
    if (lpLegal)   lpLegal->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpPreload) lpPreload->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpProfile) lpProfile->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
