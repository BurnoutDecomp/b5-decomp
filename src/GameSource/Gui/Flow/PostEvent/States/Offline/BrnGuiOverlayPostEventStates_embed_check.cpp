// Embed check for the Gui-overlay-postevent group: BrnPreloadOverlayState /
// BrnCompletedGame / BrnOfflineInstantResults. Compile-only (cl /c) exercise of each
// header's one ledger function, the inline virtual GetResourcesToLoad override over the
// committed CgsGui::State base and committed CgsGui::sResourceTuple. No link, so the
// .rdata statics and the out-of-line virtuals owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/Overlay/States/BrnPreloadOverlayState.h"
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnCompletedGame.h"
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineInstantResults.h"

// Each flow state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::PreloadOverlayState), "PreloadOverlayState : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::CompletedGame),       "CompletedGame : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::InstantResultsState), "InstantResultsState : CgsGui::State");

// The committed (id,type) tuple is two u32-sized fields; the accessor hands back a
// pointer to a table of them plus a count, so a tuple is at least 8 bytes.
static_assert(sizeof(CgsGui::sResourceTuple) >= 8, "sResourceTuple is at least {u32 id, enum type}");

void brn_gui_overlay_postevent_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::PreloadOverlayState::*lpfnPreload)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::PreloadOverlayState::GetResourcesToLoad;
    void (BrnGui::CompletedGame::*lpfnCompleted)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::CompletedGame::GetResourcesToLoad;
    void (BrnGui::InstantResultsState::*lpfnInstant)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::InstantResultsState::GetResourcesToLoad;
    (void)lpfnPreload; (void)lpfnCompleted; (void)lpfnInstant;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::PreloadOverlayState* lpPreload   = 0;
    BrnGui::CompletedGame*       lpCompleted = 0;
    BrnGui::InstantResultsState* lpInstant   = 0;
    if (lpPreload)   lpPreload->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpCompleted) lpCompleted->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpInstant)   lpInstant->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
