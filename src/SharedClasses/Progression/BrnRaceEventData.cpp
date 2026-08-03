#include "SharedClasses/Progression/BrnRaceEventData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (RaceEventData bounds checks)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnProgression::EventRacerPersonality::Construct   @ 0x826767B8
//   BrnProgression::RaceEventData::GetCheckpointData   @ 0x8230F808
//   BrnProgression::RaceEventData::GetRankScore        @ 0x823543D0
//   BrnProgression::RaceEventData::GetRankTime         @ 0x8230F748

namespace BrnProgression
{

// X360 0x826767B8. Resets the four AI tuning values to zero. The X360 build loads a single
// 0.0f constant (flt_82001CC0) and stores it into all four float slots.
void EventRacerPersonality::Construct()
{
    mfMinAggression = 0.0f;
    mfMaxAggression = 0.0f;
    mfSkill         = 0.0f;
    mfSpeed         = 0.0f;
}

// X360 0x8230F808. Bounds-checked accessor into the per-event checkpoint table (40-byte stride).
// Asserts 0 <= liCheckpointIndex < miCheckpointCount (BrnRaceEventData.h:953), then returns
// &mpaCheckpoints[liCheckpointIndex] (X360: `return 40 * index + *(this + 0x18)`).
const CheckpointData* RaceEventData::GetCheckpointData(s32 liCheckpointIndex) const
{
    CGS_ASSERT(liCheckpointIndex >= 0 && liCheckpointIndex < miCheckpointCount,
               "liCheckpointIndex >= 0 && liCheckpointIndex < miCheckpointCount");
    // CheckpointData is now a complete 40-byte type (static_assert in the header), so the
    // console's `40 * index + base` is plain element indexing -- no byte arithmetic needed.
    return mpaCheckpoints + liCheckpointIndex;
}

// X360 0x823543D0. Returns the target score for rank luRank. The X360 build asserts the rank is
// in range (the signed `index < 0 || index >= 6` test collapses to the unsigned `luRank < 6`
// here; BrnRaceEventData.h:883), then returns maiRankScores[luRank] (X360: `*(this + 4*(rank+10))`
// == this + 0x28 + rank*4).
s32 RaceEventData::GetRankScore(u32 luRank) const
{
    CGS_ASSERT(luRank < KU_NUM_RANKS, "Invalid rank");
    return maiRankScores[luRank];
}

// X360 0x8230F748. Returns the target time (seconds) for rank luRank as an f32. The X360 build
// asserts the rank is in range (BrnRaceEventData.h:898), then loads the float at
// this + 4*(rank+0x10) == this + 0x40 + rank*4 (the `lfsx f1` proves a 32-bit float load/return;
// Hex-Rays mis-rendered the return as a _DWORD* through the assert-only path).
f32 RaceEventData::GetRankTime(u32 luRank) const
{
    CGS_ASSERT(luRank < KU_NUM_RANKS, "Invalid rank");
    return mafRankTimes[luRank];
}

}
