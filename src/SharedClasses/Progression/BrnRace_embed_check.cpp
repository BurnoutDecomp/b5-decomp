// Compile-only embedder check for BrnProgression::Race. Not part of the shipping build:
// it embeds a Race by value and drives the one accessor bodied in BrnRace.cpp (Construct),
// the way ProgressionManager::HACK_SetupRaces and the CrashNavMap events do.
#include "SharedClasses/Progression/BrnRace.h"

namespace BrnProgression
{

// The derived Race members start at byte 0x30 (BaseRace is 48 bytes); the landmark slot is
// the 2-byte LandmarkIndex. These are the proven console offsets the X360 Construct writes.
static_assert(sizeof(BrnGameState::LandmarkIndex) == 2, "LandmarkIndex is a 2-byte handle");
static_assert(sizeof(BaseRace) == 48, "BaseRace occupies 0x00..0x2F");

void Race_EmbedCheck()
{
    Race lRace;
    lRace.Construct();
}

}
