#include "SharedClasses/Progression/BrnProgressionData.h"
#include "SharedClasses/Progression/BrnRaceBalance.h"        // complete OpponentBalanceData (by-value return, accessors, KI_GRAPH_POINTS)
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameSource/GameState/BrnGameActions.h"             // complete BrnProgression::TrophyUnlockData (16-byte) for &mpaTrophyUnlocks[i]
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // ProgressionRankData (pointer-return only)
#include <cstring>                                           // memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnProgression::ProgressionData::GetProgressionRankData      @ 0x82311790
//   BrnProgression::ProgressionData::GetTrophyUnlock             @ 0x823569F0
//   BrnProgression::ProgressionData::GetInterpolatedAIBalanceGraph @ 0x82676820

namespace BrnProgression
{

// X360 0x82311790. Bounds-checked accessor into the progression-rank table (112-byte stride).
const ProgressionRankData* ProgressionData::GetProgressionRankData(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muProgressionRankCount, "luIndex < muProgressionRankCount");
    return &mpaProgressionRanks[luIndex];
}

// X360 0x823569F0. Bounds-checked accessor into the trophy-unlock table (16-byte stride).
TrophyUnlockData* ProgressionData::GetTrophyUnlock(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muTrophyUnlockCount, "luIndex < muTrophyUnlockCount");
    return &mpaTrophyUnlocks[luIndex];
}

// X360 0x82676820. Returns an OpponentBalanceData whose ahead/behind catch-up graphs are the
// per-point linear interpolation between entries liIndexA and liIndexB by lfBlend. The catch-up
// cut-off ratio is intentionally left at zero (the result is fully memset first and only the two
// 8-point graphs are written).
OpponentBalanceData ProgressionData::GetInterpolatedAIBalanceGraph(s32 liIndexA, s32 liIndexB, f32 lfBlend) const
{
    CGS_ASSERT(static_cast<u32>(liIndexA) < muAIBalanceCount, "luIndex < muAIBalanceCount");
    const OpponentBalanceData& lrBalanceA = mpaAIBalances[liIndexA];

    CGS_ASSERT(static_cast<u32>(liIndexB) < muAIBalanceCount, "luIndex < muAIBalanceCount");
    const OpponentBalanceData& lrBalanceB = mpaAIBalances[liIndexB];

    OpponentBalanceData lResult;
    memset(&lResult, 0, sizeof(lResult));

    for (s32 liPointIndex = 0; liPointIndex < OpponentBalanceData::KI_GRAPH_POINTS; ++liPointIndex)
    {
        // Ahead graph: A + (B - A) * blend.
        const f32 lfAheadA = lrBalanceA.GetAheadTime(liPointIndex);
        const f32 lfAheadB = lrBalanceB.GetAheadTime(liPointIndex);
        lResult.SetAheadTime(liPointIndex, lfAheadA + (lfAheadB - lfAheadA) * lfBlend);

        // Behind graph: A + (B - A) * blend.
        const f32 lfBehindA = lrBalanceA.GetBehindTime(liPointIndex);
        const f32 lfBehindB = lrBalanceB.GetBehindTime(liPointIndex);
        lResult.SetBehindTime(liPointIndex, lfBehindA + (lfBehindB - lfBehindA) * lfBlend);
    }

    return lResult;
}

}
