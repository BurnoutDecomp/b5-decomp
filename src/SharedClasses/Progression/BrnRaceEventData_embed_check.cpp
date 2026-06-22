// Compile-only embedder check for BrnProgression::EventRacerPersonality. Not part of the
// shipping build: it embeds the record by value and drives the one accessor bodied in
// BrnRaceEventData.cpp (Construct), the way ModeManager::SetupOpponentData does.
#include "SharedClasses/Progression/BrnRaceEventData.h"

namespace BrnProgression
{

// Construct only ever stores zeros; the record is four contiguous f32 (16 bytes).
static_assert(sizeof(EventRacerPersonality) == 16, "EventRacerPersonality is four f32");

void EventRacerPersonality_EmbedCheck()
{
    EventRacerPersonality lPersonality;
    lPersonality.Construct();
}

}
