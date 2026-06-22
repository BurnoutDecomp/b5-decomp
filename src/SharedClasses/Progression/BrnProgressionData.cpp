#include "SharedClasses/Progression/BrnProgressionData.h"
#include "SharedClasses/Progression/BrnRaceBalance.h"        // complete OpponentBalanceData (by-value return, accessors, KI_GRAPH_POINTS)
#include "SharedClasses/Progression/BrnRival.h"              // complete BrnProgression::Rival (56-byte) for the rival lookups
#include "SharedClasses/Progression/BrnOpponentData.h"       // complete BrnProgression::CarOpponentSet (144-byte) for FindCarOpponentSet
#include "SharedClasses/Progression/BrnRaceEventData.h"      // complete BrnProgression::EventJunction (16-byte) for FixDown
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameSource/GameState/BrnGameActions.h"             // complete BrnProgression::TrophyUnlockData (16-byte) for &mpaTrophyUnlocks[i]
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // ProgressionRankData (pointer-return only)
#include <cstring>                                           // memset
#include <cstdint>                                           // uintptr_t (load-time pointer relocation)

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

// X360 0x82676A90. Linear scan of the rival table for a matching id. Returns the matching index,
// or miRivalCount when no rival matches (the loop falls through with the running index == count).
s32 ProgressionData::FindRivalIndexFromId(CgsID lRivalId) const
{
    s32 liRivalIndex = 0;
    if (miRivalCount > 0)
    {
        do
        {
            if (mpaRivals[liRivalIndex].GetId() == lRivalId)
            {
                break;
            }
            ++liRivalIndex;
        }
        while (liRivalIndex < miRivalCount);
    }
    return liRivalIndex;
}

// X360 0x82676AC8. Same scan as FindRivalIndexFromId, but returns the matching Rival pointer (or
// null when the scan runs off the end without a match).
const Rival* ProgressionData::FindRival(CgsID lRivalId) const
{
    s32 liRivalIndex = 0;
    if (miRivalCount > 0)
    {
        do
        {
            if (mpaRivals[liRivalIndex].GetId() == lRivalId)
            {
                break;
            }
            ++liRivalIndex;
        }
        while (liRivalIndex < miRivalCount);
    }

    if (liRivalIndex >= miRivalCount)
    {
        return 0;
    }
    return &mpaRivals[liRivalIndex];
}

// X360 0x82676B18. Walks the CarOpponentSet table for sets whose player-car id matches lCarModelId.
// An exact rank match returns immediately; otherwise the set with the highest rank still below the
// player's race rank is returned (null when no set matches the car id at all).
CarOpponentSet* ProgressionData::FindCarOpponentSet(CgsID lCarModelId, s32 liPlayerRaceRank) const
{
    CarOpponentSet* lpNearestCarOpponentSet = 0;
    s32             liNearestRankFound      = -1;

    for (u32 luIndex = 0; luIndex < muCarOpponentsCount; ++luIndex)
    {
        CarOpponentSet& lrCarOpponentSet = mpaCarOpponentSet[luIndex];
        if (lrCarOpponentSet.GetPlayerCarId() == lCarModelId)
        {
            const s32 liOpponentSetRank = lrCarOpponentSet.GetRank();
            if (liOpponentSetRank == liPlayerRaceRank)
            {
                return &lrCarOpponentSet;
            }
            if (liOpponentSetRank > liNearestRankFound && liOpponentSetRank < liPlayerRaceRank)
            {
                liNearestRankFound      = lrCarOpponentSet.GetRank();
                lpNearestCarOpponentSet = &lrCarOpponentSet;
            }
        }
    }

    return lpNearestCarOpponentSet;
}

// X360 0x8267F220. Load-time pointer relocation ("fix down"): converts every stored pointer from
// an absolute load address back to a serialised offset by subtracting the load-base delta. It
// rebases the nine top-level array bases, the two event pointers inside every EventJunction and
// the checkpoint pointer inside every RaceEventData. This is the X360 inverse of FixUp; the
// returned value (the object itself in the X360 binary) is unused by the resource-type forwarder,
// so the int-delta contract just returns the delta.
//
// Pointer relocation is intrinsically address arithmetic, so the rebased pointers are handled as
// integers here (the established pattern for these resource FixUp/FixDown routines). The pointer
// members themselves are reached BY NAME; only the within-RaceEventData checkpoint pointer is
// reached through a local relocation view, because the complete RaceEventData layout cannot be
// pulled into this TU (a partial BrnProgression::RaceEventData is already visible here via
// BrnGameActions.h/BrnGameModeParams.h -- see dep_flags). Its checkpoint pointer sits at the
// console offset +0x18 inside the 248-byte record.
int ProgressionData::FixDown(int liDelta)
{
    // Per-EventJunction: rebase the two event pointers (by name).
    for (u32 luEventJunctionIndex = 0; luEventJunctionIndex < muEventJunctionCount; ++luEventJunctionIndex)
    {
        EventJunction& lrJunction = mpaEventJunctions[luEventJunctionIndex];
        if (lrJunction.mpOfflineEvent)
        {
            lrJunction.mpOfflineEvent = reinterpret_cast<const RaceEventData*>(
                reinterpret_cast<uintptr_t>(lrJunction.mpOfflineEvent) - liDelta);
        }
        if (lrJunction.mpOnlineEvent)
        {
            lrJunction.mpOnlineEvent = reinterpret_cast<const RaceEventData*>(
                reinterpret_cast<uintptr_t>(lrJunction.mpOnlineEvent) - liDelta);
        }
    }

    // Per-RaceEventData: rebase the checkpoint pointer (console offset +0x18 in the 248-byte
    // record). Reached through a relocation view because the complete type is not visible here.
    struct EventRelocationView
    {
        u32       _0[6];        // 0x00..0x17 leading scalars / id / car id
        uintptr_t mpaCheckpoints; // 0x18 checkpoint table pointer (32-bit on the console)
    };
    EventRelocationView* lpEvents = reinterpret_cast<EventRelocationView*>(mpaEvents);
    for (u32 luEventIndex = 0; luEventIndex < muEventCount; ++luEventIndex)
    {
        lpEvents[luEventIndex].mpaCheckpoints -= liDelta;
    }

    // The nine top-level array bases (by name).
    mpaPlayerCarIds     = reinterpret_cast<CgsID*>(reinterpret_cast<uintptr_t>(mpaPlayerCarIds) - liDelta);
    mpaProgressionRanks = reinterpret_cast<ProgressionRankData*>(reinterpret_cast<uintptr_t>(mpaProgressionRanks) - liDelta);
    mpaEventJunctions   = reinterpret_cast<EventJunction*>(reinterpret_cast<uintptr_t>(mpaEventJunctions) - liDelta);
    mpaEvents           = reinterpret_cast<RaceEventData*>(reinterpret_cast<uintptr_t>(mpaEvents) - liDelta);
    mpaRivals           = reinterpret_cast<Rival*>(reinterpret_cast<uintptr_t>(mpaRivals) - liDelta);
    mpaAIBalances       = reinterpret_cast<OpponentBalanceData*>(reinterpret_cast<uintptr_t>(mpaAIBalances) - liDelta);
    mpaPersonalities    = reinterpret_cast<EventRacerPersonality*>(reinterpret_cast<uintptr_t>(mpaPersonalities) - liDelta);
    mpaTrophyUnlocks    = reinterpret_cast<TrophyUnlockData*>(reinterpret_cast<uintptr_t>(mpaTrophyUnlocks) - liDelta);
    mpaCarOpponentSet   = reinterpret_cast<CarOpponentSet*>(reinterpret_cast<uintptr_t>(mpaCarOpponentSet) - liDelta);

    return liDelta;
}

}
