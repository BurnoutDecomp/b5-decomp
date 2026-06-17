// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_UpdateB.cpp
// ============================================================================
// Out-of-line method BODIES for the ScoringSystem "UpdateB" group
// (BrnScoringSystem.h lines 420-434): the part-B per-frame update pass --
// network/cumulative results, the race-car distance comparator, the
// player-driving detectors and the per-mode score / general-stat updates.
//
// SHAPE = DecFIGS DWARF; BODY = X360 pseudocode (overrides DWARF on conflict).
// Members accessed BY NAME against the keystone layout -- no offset casts.
//
// ----------------------------------------------------------------------------
// LANDED (compiling bodies):
//   CompareRaceCarDistances   (0x823125B8)
//   CheckRoadRageMedalAwarded (0x82312840)
//
// BLOCKED (body omitted -- left declare-only; each needs a type/method that has
// no committed definition reachable from a body agent; the dependency RULE
// forbids ad-hoc-slicing a shared keystone). Ledger:
//
//   UpdateNetworkPlayerResults (0x8231FA90)
//       Walks the lpResults per-player record stream (X360: a2+24, stride 28)
//       reading PlayerResultsData fields (timed-out / race-car index / finish
//       time / eliminations) and writing them onto each CarData, then fires a
//       virtual on mpCurrentOnlineModeScoring (vtable+0x18). PlayerResultsData
//       is modelled as an unnamed u8[8*28] blob on the committed
//       PlayerResultsInterface (BrnNetworkModuleIO.h) -- no named fields/accessor
//       -- and the BaseOnlineModeScoring slice declares no such virtual.
//       MISSING: named BrnNetwork::...::PlayerResultsData fields (+ accessor)
//                and the BaseOnlineModeScoring vtable+0x18 virtual.
//
//   UpdateCumulativeResults    (0x8231FCA0)
//       Per car, accumulates the round's online points into miCumulativePoints
//       (miCumulativePoints += CarScoreData[+0x58]) and, when final, stamps the
//       disconnect round + fires a virtual on mpCurrentOnlineModeScoring
//       (vtable+0x1C). The +0x58 slot (miOnlineFinishPositionScore) on the
//       committed CarScoreData has only setters (no getter), and the
//       BaseOnlineModeScoring slice declares no vtable+0x1C virtual.
//       MISSING: CarScoreData getter for miOnlineFinishPositionScore (+0x58)
//                and the BaseOnlineModeScoring vtable+0x1C virtual.
//
//   UpdateDistanceToPlayer     (0x8232B408)
//   DetectPlayerDrivingWrongWay(0x8232B6B8)
//   DetectPlayerStationary     (0x82320008)
//   UpdateGeneralStats         (0x8232B8C0)
//       All four dereference lpOutput (the ActiveRaceCarOutputInterface*): they
//       read maCarsInTheRace.GetLength() (X360 a2+512), index maCarsInTheRace,
//       call IsRaceCarActive / GetPlayerActiveRaceCarIndex and read the deep
//       per-car physics output. lpOutput's declared type is
//       BrnGameState::ActiveRaceCarOutputInterface, which the keystone only
//       forward-declares -- the GameState-side interface has no committed
//       definition (the World-side RCEntityActiveRaceCarOutputInterface is a
//       DISTINCT C++ type; aliasing the two is a keystone/layout change, out of
//       scope for a body agent -- same blocker recorded in the sibling
//       BrnScoringSystem_UpdateA.cpp ledger).
//       MISSING: BrnGameState::ActiveRaceCarOutputInterface (definition).
//
// (UpdateCrashModeScore / UpdateStuntAttackModeScore / UpdateRoadRageModeScore /
//  SetRoadRageDetails at header lines 426-431 carry only a ':NNN' DWARF line and
//  NO X360 0x82 address comment, so they are not targets of this addressed-body
//  pass and have no dossier to reconstruct from.)
// ----------------------------------------------------------------------------

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Bad-case-of-Road-Rage assert)

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // CompareRaceCarDistances  --  X360 0x823125B8  (BrnScoringSystem.cpp:533)
    // ------------------------------------------------------------------------
    // qsort(3) comparator over the maRaceCarPositioningData[8] scratch (the X360
    // call site is `qsort(&maRaceCarPositioningData, 8, sizeof(RaceCarPositioningData),
    // &CompareRaceCarDistances)`). It never touches `this`, so it is usable as a
    // C comparator even though the DWARF declares it as an ordinary member.
    //
    // Ordering (return >0 => lpA sorts AFTER lpB, the qsort convention):
    //   1. an unset slot (meActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID,
    //      i.e. -1) sorts last;
    //   2. a disconnected car sorts last;
    //   3. greater finish position sorts last;
    //   4. fewer checkpoints reached sorts last;
    //   5. greater distance-to-next-checkpoint sorts last;
    //   6. otherwise equal.
    int ScoringSystem::CompareRaceCarDistances(const void* lpA, const void* lpB)
    {
        const RaceCarPositioningData* lpPosDataA = static_cast<const RaceCarPositioningData*>(lpA);
        const RaceCarPositioningData* lpPosDataB = static_cast<const RaceCarPositioningData*>(lpB);

        if (lpPosDataA->meActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
            return 1;
        if (lpPosDataB->meActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
            return -1;

        if (lpPosDataA->mbDisconnected)
            return 1;
        if (lpPosDataB->mbDisconnected)
            return -1;

        if (lpPosDataA->miFinishPosition > lpPosDataB->miFinishPosition)
            return 1;
        if (lpPosDataA->miFinishPosition < lpPosDataB->miFinishPosition)
            return -1;

        if (lpPosDataA->miCurrentCheckpoint < lpPosDataB->miCurrentCheckpoint)
            return 1;
        if (lpPosDataA->miCurrentCheckpoint > lpPosDataB->miCurrentCheckpoint)
            return -1;

        if (lpPosDataA->mfDistanceToNextCheckpoint > lpPosDataB->mfDistanceToNextCheckpoint)
            return 1;
        if (lpPosDataA->mfDistanceToNextCheckpoint < lpPosDataB->mfDistanceToNextCheckpoint)
            return -1;

        return 0;
    }

    // ------------------------------------------------------------------------
    // CheckRoadRageMedalAwarded  --  X360 0x82312840  (BrnScoringSystem.cpp:2283)
    // ------------------------------------------------------------------------
    // Road-Rage medal ratchet, called as the player's takedown count climbs.
    // While a medal is still outstanding (meCurrentMedalAchieved != GOLD), and
    // the running takedown count has reached the current target's threshold,
    // record the achievement, advance the current target one notch toward gold
    // (BRONZE->SILVER->GOLD) and re-point the Road-Rage scorer's takedown target
    // at the next threshold.
    void ScoringSystem::CheckRoadRageMedalAwarded(u32 luTakedowns)
    {
        if (meCurrentMedalAchieved)
        {
            const ECurrentMedalTargetTime leTarget = meCurrentMedalTarget;
            if (luTakedowns >= mauiMedalScores[leTarget])
            {
                meCurrentMedalAchieved = leTarget;
                if (leTarget)
                {
                    ECurrentMedalTargetTime leNextTarget;
                    if (leTarget == E_CURRENT_MEDAL_TARGET_TIME_SILVER)
                    {
                        leNextTarget = E_CURRENT_MEDAL_TARGET_TIME_GOLD;
                    }
                    else
                    {
                        CGS_ASSERT(leTarget < E_CURRENT_MEDAL_TARGET_TIME_NONE, "Bad case of Rage Rage\n");
                        leNextTarget = E_CURRENT_MEDAL_TARGET_TIME_SILVER;
                    }
                    meCurrentMedalTarget = leNextTarget;
                }
                mRoadRageModeScoring.SetTakeDownTarget(static_cast<s32>(mauiMedalScores[meCurrentMedalTarget]));
            }
        }
    }
}
