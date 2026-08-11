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
    return &GetProgressionRanks()[luIndex];
}

// The event-junction accessors BrnProgressionData.h:113-114 declares and delegates to this TU
// ("declare-only here; bodies land with the ProgressionData TU"). Semantics are the ones the
// header's own note pins down: count word muEventJunctionCount at +0x1C, base mpaEventJunctions
// at +0x18, 16-byte stride. Bound assert per the sibling accessor idiom above.
u32 ProgressionData::GetEventJunctionCount() const
{
    return muEventJunctionCount;
}

const EventJunction* ProgressionData::GetEventJunction(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muEventJunctionCount, "luIndex < muEventJunctionCount");
    return &GetEventJunctions()[luIndex];
}

// X360 0x823569F0. Bounds-checked accessor into the trophy-unlock table (16-byte stride).
TrophyUnlockData* ProgressionData::GetTrophyUnlock(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muTrophyUnlockCount, "luIndex < muTrophyUnlockCount");
    return &GetTrophyUnlocks()[luIndex];
}

// X360 0x82676820. Returns an OpponentBalanceData whose ahead/behind catch-up graphs are the
// per-point linear interpolation between entries liIndexA and liIndexB by lfBlend. The catch-up
// cut-off ratio is intentionally left at zero (the result is fully memset first and only the two
// 8-point graphs are written).
OpponentBalanceData ProgressionData::GetInterpolatedAIBalanceGraph(s32 liIndexA, s32 liIndexB, f32 lfBlend) const
{
    CGS_ASSERT(static_cast<u32>(liIndexA) < muAIBalanceCount, "luIndex < muAIBalanceCount");
    const OpponentBalanceData& lrBalanceA = GetAIBalances()[liIndexA];

    CGS_ASSERT(static_cast<u32>(liIndexB) < muAIBalanceCount, "luIndex < muAIBalanceCount");
    const OpponentBalanceData& lrBalanceB = GetAIBalances()[liIndexB];

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
            if (GetRivals()[liRivalIndex].GetId() == lRivalId)
            {
                break;
            }
            ++liRivalIndex;
        }
        while (liRivalIndex < miRivalCount);
    }
    return liRivalIndex;
}

// ---- ADDITIVE (SetupParRivals wave, 2026-08-11) -------------------------------------------
// The by-index rival accessor BrnProgressionData.h:107-108 declares and delegates to this TU.
// It has NO standalone X360 symbol -- the console folds it into every caller -- so its shape is
// recovered from two independent inline expansions, which agree instruction for instruction:
//
//   StreetManager::FindRivalsByDistrict @0x82336360 (loop body 0x82363D8..0x82363FC)
//       cmpw  cr6, r29, r11         ; r29 == liIndex, r11 == *(pd + 0x2C) == miRivalCount
//       blt   cr6, ok               ; ...the bound is a SIGNED `liIndex < miRivalCount`,
//       bl    BeginAssert           ;    with NO liIndex >= 0 half (unlike GetRoad's)
//       li    r5, 0x1CC             ; BrnProgressionData.h:460
//       <"liIndex < miRivalCount", "..\..\..\SharedClasses\Progression/BrnProgressionData.h">
//     ok: lwz  r11, 0x28(r28)       ; == mpaRivals (console +0x28)
//         add  r11, r11, r31        ; r31 steps by 0x38 == the 56-byte Rival stride
//
//   StreetManager::SetupParRivals @0x8233F560 (fallback leg 0x8233F804..0x8233F838) --
//   the SAME assert (r5 = 0x1CC, same two literals) followed by `lwz r11, 0x28(r28)` and a
//   `ld r11, 0(r11)` for GetRival(0)->GetId(). Note the console RE-EVALUATES the whole
//   accessor (assert included) once per stored id, which is why the assert lives HERE and the
//   callers never duplicate it -- exactly what the header's note says.
//
// The count word is re-read from the object on every evaluation in both sites (never hoisted),
// so it is read through the member here rather than cached by the caller. Table access is
// through the serialised-slot helper GetRivals(), so no console byte offset is transcribed.
const Rival* ProgressionData::GetRival(s32 liIndex) const
{
    CGS_ASSERT(liIndex < miRivalCount, "liIndex < miRivalCount");
    return &GetRivals()[liIndex];
}

Rival* ProgressionData::GetRival(s32 liIndex)
{
    CGS_ASSERT(liIndex < miRivalCount, "liIndex < miRivalCount");
    return &GetRivals()[liIndex];
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
            if (GetRivals()[liRivalIndex].GetId() == lRivalId)
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
    return &GetRivals()[liRivalIndex];
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
        CarOpponentSet& lrCarOpponentSet = GetCarOpponentSets()[luIndex];
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

// X360 0x8267F220. Load-time relocation ("fix down"): converts every stored table address back
// into a serialised file-relative offset by subtracting the load-base delta. It rebases the nine
// top-level array-base slots, the two event slots inside every EventJunction and the checkpoint
// slot inside every RaceEventData. The returned value (the object itself in the X360 binary) is
// unused by the resource-type forwarder, so the int-delta contract just returns the delta.
//
// EVERY RELOCATED SLOT IS A 32-BIT SERIALISED WORD (see the banner in BrnProgressionData.h): the
// console reads the whole root as `result[N]` u32 words, `v3 += 16` strides the junction table and
// `v8 += 248` the event table, touching `+4`/`+8` and `+24` respectively. So the arithmetic below
// is plain u32 arithmetic on named members -- no host-pointer casts, no relocation views.
//
// ORDER MATTERS AND MATCHES THE CONSOLE: the array walks run FIRST (while the base slots are still
// absolute addresses and the tables are therefore reachable), and only then are the nine bases
// converted back to offsets.
int ProgressionData::FixDown(int liDelta)
{
    const u32 luDelta = static_cast<u32>(liDelta);

    // Per-EventJunction: the two event slots. The console null-tests each one before rebasing
    // (a junction with no online counterpart stores 0), which must not become `-delta`.
    EventJunction* lpaEventJunctions = GetEventJunctions();
    for (u32 luEventJunctionIndex = 0; luEventJunctionIndex < muEventJunctionCount; ++luEventJunctionIndex)
    {
        EventJunction& lrJunction = lpaEventJunctions[luEventJunctionIndex];
        if (lrJunction.muOfflineEventOffset != 0)
        {
            lrJunction.muOfflineEventOffset -= luDelta;
        }
        if (lrJunction.muOnlineEventOffset != 0)
        {
            lrJunction.muOnlineEventOffset -= luDelta;
        }
    }

    // Per-RaceEventData: the checkpoint-table slot at +0x18. The console does NOT null-test this
    // one (`*(v9 + 24) -= a2` unconditionally), so neither does this.
    RaceEventData* lpaEvents = GetEvents();
    for (u32 luEventIndex = 0; luEventIndex < muEventCount; ++luEventIndex)
    {
        lpaEvents[luEventIndex].muaCheckpointsOffset -= luDelta;
    }

    // The nine top-level array bases (X360 words 2/4/6/8/10/12/14/16/18).
    muaPlayerCarIds     -= luDelta;
    muaProgressionRanks -= luDelta;
    muaEventJunctions   -= luDelta;
    muaEvents           -= luDelta;
    muaRivals           -= luDelta;
    muaAIBalances       -= luDelta;
    muaPersonalities    -= luDelta;
    muaTrophyUnlocks    -= luDelta;
    muaCarOpponentSet   -= luDelta;

    return liDelta;
}

// X360 0x8267F338 -- the strict inverse of FixDown above: it turns every serialised offset into an
// absolute address by ADDING the load-base delta, so the nine base slots must be relocated FIRST
// (the two per-record walks below can only reach their tables through already-fixed bases).
//
// EXPORT GAP, STATED PLAINLY: 0x8267F338 has no per-function JSON in
// .ida-exports/BURNOUT_X360_ARTIST.XEX (the address is only recoverable from
// CgsDev::Assert::FireAssert's xref list and from ProgressionResourceType::FixUp @0x8267F490's
// tail-branch), so this body is derived from its attested inverse rather than read off the
// console. Two consequences are recorded rather than papered over:
//   * the console body DOES fire an assert (that is how its address surfaced, via the FireAssert
//     xref). Its predicate and message could not be read, so NO assert is invented here.
//   * the relocated set is FixDown's set, on FixDown's evidence.
int ProgressionData::FixUp(int liDelta)
{
    const u32 luDelta = static_cast<u32>(liDelta);

    // The nine top-level array bases first -- everything else is reached through them.
    muaPlayerCarIds     += luDelta;
    muaProgressionRanks += luDelta;
    muaEventJunctions   += luDelta;
    muaEvents           += luDelta;
    muaRivals           += luDelta;
    muaAIBalances       += luDelta;
    muaPersonalities    += luDelta;
    muaTrophyUnlocks    += luDelta;
    muaCarOpponentSet   += luDelta;

    // Per-EventJunction: the two event slots, null-preserving (FixDown's null test).
    EventJunction* lpaEventJunctions = GetEventJunctions();
    for (u32 luEventJunctionIndex = 0; luEventJunctionIndex < muEventJunctionCount; ++luEventJunctionIndex)
    {
        EventJunction& lrJunction = lpaEventJunctions[luEventJunctionIndex];
        if (lrJunction.muOfflineEventOffset != 0)
        {
            lrJunction.muOfflineEventOffset += luDelta;
        }
        if (lrJunction.muOnlineEventOffset != 0)
        {
            lrJunction.muOnlineEventOffset += luDelta;
        }
    }

    // Per-RaceEventData: the checkpoint-table slot at +0x18 (unconditional, as in FixDown).
    RaceEventData* lpaEvents = GetEvents();
    for (u32 luEventIndex = 0; luEventIndex < muEventCount; ++luEventIndex)
    {
        lpaEvents[luEventIndex].muaCheckpointsOffset += luDelta;
    }

    return liDelta;
}

}
