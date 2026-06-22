// Embed check for the Gui-event group: BrnShowtimeInstantResults / BrnPreRaceFlyBy.
// Compile-only (cl /c) exercise of each header's one ledger function, the inline virtual
// GetResourcesToLoad override over the committed CgsGui::State base and committed
// CgsGui::sResourceTuple. No link, so the .rdata statics and the out-of-line virtuals
// owned by the class: TUs stay external.

#include "GameSource/Gui/Flow/PostEvent/States/Showtime/BrnShowtimeInstantResults.h"
#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"

// Each flow state must actually be a CgsGui::State (reuse-by-name, not a fork).
static_assert(__is_base_of(CgsGui::State, BrnGui::ShowtimeInstantResultsState), "ShowtimeInstantResultsState : CgsGui::State");
static_assert(__is_base_of(CgsGui::State, BrnGui::PreRaceFlyByState),           "PreRaceFlyByState : CgsGui::State");

// The committed (id,type) tuple is two u32-sized fields; the accessor hands back a
// pointer to a table of them plus a count, so a tuple is at least 8 bytes.
static_assert(sizeof(CgsGui::sResourceTuple) >= 8, "sResourceTuple is at least {u32 id, enum type}");

void brn_gui_event_flow_states_embed_check()
{
    const CgsGui::sResourceTuple* lpTuples = 0;
    u32 luCount = 0;

    // The override signature must match the committed base virtual exactly.
    void (BrnGui::ShowtimeInstantResultsState::*lpfnShowtime)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::ShowtimeInstantResultsState::GetResourcesToLoad;
    void (BrnGui::PreRaceFlyByState::*lpfnPreRace)(const CgsGui::sResourceTuple**, u32*) const
        = &BrnGui::PreRaceFlyByState::GetResourcesToLoad;
    (void)lpfnShowtime; (void)lpfnPreRace;

    // Exercise the inline bodies directly (compile only: vtable/statics resolve at link).
    BrnGui::ShowtimeInstantResultsState* lpShowtime = 0;
    BrnGui::PreRaceFlyByState*           lpPreRace  = 0;
    if (lpShowtime) lpShowtime->GetResourcesToLoad(&lpTuples, &luCount);
    if (lpPreRace)  lpPreRace->GetResourcesToLoad(&lpTuples, &luCount);
    (void)lpTuples; (void)luCount;
}
