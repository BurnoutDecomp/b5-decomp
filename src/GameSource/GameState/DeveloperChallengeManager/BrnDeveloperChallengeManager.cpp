// ============================================================================
// b5-decomp/src/GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.cpp
// ============================================================================
// BrnGameState::DeveloperChallengeManager -- the developer-challenge gameplay watcher.
//
// SOURCE-OF-TRUTH: X360 ARTIST pseudocode + ASM is the spine (signatures/branches/stores/constants
// from the asm at 0x82372E90 (Construct) and the per-event handlers). There is NO DWARF entry for
// this class; the member layout is recovered from Construct + the handlers (see the header).
//
// FLAG (read me): several event handlers gate on developer-challenge THRESHOLD constants that the
// X360 loads from rodata (dword_82CDB99x / flt_82CDB98x @ 0x82CDB97C..0x82CDB9A8). Those literal
// VALUES are NOT present in the asm and are NOT recovered in this view; they are referenced here via
// the flagged KF_/KI_ placeholder externs declared in the header and DEFINED (as flagged
// placeholders) at the bottom of this file. NEVER trust the placeholder values for behaviour --
// ground them from the rodata table when it is recovered.
//
// FLAG: a handful of cross-object reads (the stunt scorer's running score, the per-car finish score,
// the TimerStatus sim time, the stunt-offence score/flag word, the buffered-billboard record fields)
// land at X360 offsets whose owning types are minimal/declare-only slices in this tree. Those reads
// are routed through named accessors added additively to the owning homes where the offset+behaviour
// is asm-attested; where an accessor is not yet available the dependent branch is reconstructed
// against the named member it logically reads and FLAGGED inline. No raw `*(p+off)` hacks are used.

#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h"

#include "GameShared/GameClasses/Core/CgsID.h"                              // CgsIDCompress
#include "GameSource/GameState/Progression/BrnProgressionManager.h"        // ProgressionManager::GetProfile/GetCurrentCarData
#include "GameSource/GameState/Progression/BrnProfile.h"                    // Profile::Is/SetDeveloperChallengeComplete
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"        // CarData (GetCurrentCarData id)
#include "GameSource/GameState/BrnGameStateModule.h"                        // GameStateModule accessors
#include "GameSource/GameState/BrnGameStateModuleIO.h"                      // PreWorldInputBuffer / OutputBuffer / TimerStatusInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // CgsModule::VariableEventQueue<13312,16>::AddEvent / Event
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"     // StreetManager (mpStreetManager type)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"     // ScoringSystem / CarData / scorers
#include "SharedClasses/DataLists/VehicleList.h"                            // VehicleList::GetVehicleIndex/GetVehicleData
#include "SharedClasses/DataLists/VehicleListEntry.h"                       // VehicleListEntry::GetId/GetParentId

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// Construct  (X360 0x82372E90)  [EXECUTED in goal trace]
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                                          StreetManager*                      lpStreetManager,
                                          ScoringSystem*                      lpScoringSystem,
                                          GameStateModule*                    lpGameStateModule)
{
    CGS_ASSERT(lpProgressionManager != nullptr, "lpProgressionManager");
    CGS_ASSERT(lpStreetManager      != nullptr, "lpStreetManager");
    CGS_ASSERT(lpScoringSystem      != nullptr, "lpScoringSystem");
    CGS_ASSERT(lpGameStateModule    != nullptr, "lpGameStateModule");

    // Cache the owning subsystems (Construct stores them at +0xA4/+0xA8/+0xAC/+0xB0).
    mpProgressionManager = lpProgressionManager;
    mpStreetManager      = lpStreetManager;
    mpScoringSystem      = lpScoringSystem;
    mpGameStateModule    = lpGameStateModule;

    // Bring the trackers to their empty state. The X360 zeroes:
    //   stw 0x50  -> maCollectedBillboards.miCount = 0   (Array Construct/Clear)
    //   stw 0x80/0x84/0x88 -> maRoadRageTakedownTimes read/write/length = 0 (FifoQueue Clear)
    //   std 0x90  -> mlPendingRoadRageEventData = 0
    //   std 0x98  -> maCompletedChallenges (single u64 field) = 0
    maCollectedBillboards.Clear();
    maRoadRageTakedownTimes.Clear();
    mlPendingRoadRageEventData = 0;
    maCompletedChallenges.UnSetAll();
}

// ----------------------------------------------------------------------------
// SetChallengeCompleted  (X360 0x823674B8)
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::SetChallengeCompleted(s32 liChallengeIndex)
{
    // The X360 guards the whole body behind a global "developer challenges enabled" debug flag
    // (byte_82FFA7F1). With asserts compiled in, the body always runs; the range asserts + the
    // Profile/local bit-set are the observable effects.
    CGS_ASSERT(liChallengeIndex >= 0,                       "Out of range developer challnge");
    CGS_ASSERT(liChallengeIndex < E_DEV_CHALLENGE_COUNT,    "Out of range developer challnge");
    CGS_ASSERT(mpProgressionManager != nullptr,             "mpProgressionManager");
    CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");

    // Persist the completion on the player's Profile, then record it locally in the bit set.
    mpProgressionManager->GetProfile()->SetDeveloperChallengeComplete(liChallengeIndex);
    maCompletedChallenges.SetBit(static_cast<u32>(liChallengeIndex));
}

// ----------------------------------------------------------------------------
// CheckCarID  (X360 0x8237F770)
// True iff lCarId, following its parent-car chain, reaches lTargetCarId.
// ----------------------------------------------------------------------------
bool DeveloperChallengeManager::CheckCarID(CgsID lCarId, CgsID lTargetCarId)
{
    CGS_ASSERT(mpGameStateModule != nullptr,                "mpGameStateModule");
    CGS_ASSERT(mpGameStateModule->GetVehicleList() != nullptr, "mpGameStateModule->GetVehicleList()");

    CgsID lCurrentId = lCarId;
    if (lCurrentId == 0)
    {
        return false;
    }

    while (true)
    {
        if (lCurrentId == lTargetCarId)
        {
            return true;
        }

        BrnResource::VehicleList* lpVehicleList = mpGameStateModule->GetVehicleList();
        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lCurrentId);
        // asm 0x8237F820: a negative index sets the entry null and shares the SAME null-entry
        // assert path as GetVehicleData()==null (no early-return-false before the assert).
        const BrnResource::VehicleListEntry* lpVehicleListEntry =
            (liVehicleIndex < 0) ? nullptr : lpVehicleList->GetVehicleData(liVehicleIndex);
        CGS_ASSERT(lpVehicleListEntry != nullptr, "lpVehicleListEntry");

        // Walk up to the parent car (0 == no parent -> chain ends).
        lCurrentId = lpVehicleListEntry->GetParentId();
        if (lCurrentId == 0)
        {
            return false;
        }
    }
}

// ----------------------------------------------------------------------------
// OnFlatSpin  (X360 0x82373258)
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::OnFlatSpin(f32 lfFlatSpinAngle)
{
    if (lfFlatSpinAngle > KF_DEV_CHALLENGE_FLAT_SPIN_MIN_ANGLE)
    {
        CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
        CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");
        if (!mpProgressionManager->GetProfile()->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_FLAT_SPIN))
        {
            SetChallengeCompleted(E_DEV_CHALLENGE_FLAT_SPIN);
        }
    }
}

// ----------------------------------------------------------------------------
// OnStuntRunComboPerformed  (X360 0x82373418)
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::OnStuntRunComboPerformed(const f32* lpComboScore)
{
    if (*lpComboScore >= KF_DEV_CHALLENGE_STUNT_COMBO_MIN)
    {
        CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
        CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");
        if (!mpProgressionManager->GetProfile()->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_STUNT_COMBO))
        {
            SetChallengeCompleted(E_DEV_CHALLENGE_STUNT_COMBO);
        }
    }
}

// ----------------------------------------------------------------------------
// OnStuntOffenceComplete  (X360 0x82373338)
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::OnStuntOffenceComplete(const u8* lpStuntOffence)
{
    // X360: gate on bit 0x40 of the offence's leading flags word (`(*a2 & 0x40) == 0x40`).
    // FLAG: lpStuntOffence is a StuntOffence record whose layout is not modelled in this tree; the
    // leading flags word and the score @+104 are read against it. Reconstructed against the raw
    // record shape the asm reads (flags @+0, score @+104) -- the owning StuntOffence type's TU
    // should replace these with named accessors.
    const u32 luFlags = *reinterpret_cast<const u32*>(lpStuntOffence);          // FLAG: StuntOffence flags @+0
    if ((luFlags & 0x40u) == 0x40u)
    {
        CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
        CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");

        const f32 lfScore = *reinterpret_cast<const f32*>(lpStuntOffence + 104); // FLAG: StuntOffence score @+104
        if (!mpProgressionManager->GetProfile()->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_STUNT_OFFENCE)
            && lfScore >= KF_DEV_CHALLENGE_STUNT_OFFENCE_MIN_SCORE)
        {
            SetChallengeCompleted(E_DEV_CHALLENGE_STUNT_OFFENCE);
        }
    }
}

// ----------------------------------------------------------------------------
// OnTakedownChain  (X360 0x8238E020)
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::OnTakedownChain(s32 liChainLength)
{
    if (liChainLength >= KI_DEV_CHALLENGE_TAKEDOWN_CHAIN_MIN)
    {
        CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
        CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");

        BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
        if (!lpProfile->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_TAKEDOWN_CHAIN))
        {
            SetChallengeCompleted(E_DEV_CHALLENGE_TAKEDOWN_CHAIN);
        }

        // Car-specific variant: the chain happened in a car descended from "PUSRI01".
        CGS_ASSERT(mpProgressionManager != nullptr,                      "mpProgressionManager");
        CGS_ASSERT(mpProgressionManager->GetCurrentCarData() != nullptr, "mpProgressionManager->GetCurrentCarData()");

        const CgsID lCurrentCarId = mpProgressionManager->GetCurrentCarData()->GetId();
        const CgsID lTargetCarId  = CgsIDCompress("PUSRI01");
        if (CheckCarID(lCurrentCarId, lTargetCarId))
        {
            if (!lpProfile->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_TAKEDOWN_CHAIN_CAR))
            {
                SetChallengeCompleted(E_DEV_CHALLENGE_TAKEDOWN_CHAIN_CAR);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// OnSetRoadRule  (X360 0x8238D860)
// liRoadRuleType: 0 == time-based (car-specific), 1 == score-based.
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::OnSetRoadRule(s32 liRoadRuleType, s32 liScore, s32 liCarId)
{
    if (liRoadRuleType == 1)
    {
        // Score-based road rule -> E_DEV_CHALLENGE_ROAD_RULE_SET when the score clears the gate.
        CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
        CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");
        if (!mpProgressionManager->GetProfile()->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_ROAD_RULE_SET)
            && liScore > KI_DEV_CHALLENGE_ROAD_RULE_SCORE_MIN)
        {
            SetChallengeCompleted(E_DEV_CHALLENGE_ROAD_RULE_SET);
        }
    }
    else if (liRoadRuleType == 0)
    {
        // Time-based road rule -> car-specific "PUSMC01" challenge when fast enough.
        CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
        CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");

        if (!mpProgressionManager->GetProfile()->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_ROAD_RULE_TIME_CAR))
        {
            // X360: (liScore * 0.001) < threshold && liCarId == 394273 (a specific road id), then
            // test the current car against "PUSMC01".
            // FLAG: the 394273 immediate is the X360 a4-compare literal (asm-grounded), and the
            //       0.001 scale is flagged; the gate threshold is dword_82CDB998
            //       (KI_..._SCORE_MIN), the SAME constant the type==1 score gate uses (asm 0x8238DA74).
            const f32 lfTime = static_cast<f32>(liScore) * 0.001f;
            if (lfTime < static_cast<f32>(KI_DEV_CHALLENGE_ROAD_RULE_SCORE_MIN) && liCarId == 394273)
            {
                CGS_ASSERT(mpProgressionManager->GetCurrentCarData() != nullptr,
                           "mpProgressionManager->GetCurrentCarData()");
                const CgsID lCurrentCarId = mpProgressionManager->GetCurrentCarData()->GetId();
                const CgsID lTargetCarId  = CgsIDCompress("PUSMC01");
                if (CheckCarID(lCurrentCarId, lTargetCarId))
                {
                    SetChallengeCompleted(E_DEV_CHALLENGE_ROAD_RULE_TIME_CAR);
                }
            }
        }
    }
    else
    {
        CGS_ASSERT(false, "dodgy road rule type");
    }
}

// ----------------------------------------------------------------------------
// OnCollectStunt  (X360 0x82373028)
// luStuntType == 2 buffers a billboard collect for the burst-window check.
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::OnCollectStunt(u32 luStuntType, CgsID lBillboardId)
{
    if (luStuntType < 2u)
    {
        return;
    }
    if (luStuntType > 2u)
    {
        CGS_ASSERT(false, "dodgy stunt type");
        return;
    }

    // luStuntType == 2 (billboard).
    CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
    CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");
    if (mpProgressionManager->GetProfile()->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_BILLBOARD_BURST))
    {
        return;
    }

    // De-dupe: skip if this billboard id is already buffered.
    bool lbAlreadyBuffered = false;
    for (u32 luIndex = 0; luIndex < maCollectedBillboards.GetLength(); ++luIndex)
    {
        const CollectedBillboard& lrEntry = maCollectedBillboards[luIndex];
        // FLAG: CollectedBillboard fields are provisional (16-byte stub). The X360 reads the buffered
        // billboard id from the record's leading qword; compared against lBillboardId.
        const CgsID lBufferedId = *reinterpret_cast<const CgsID*>(lrEntry.maBlob);   // FLAG: id @+0
        if (lBufferedId == lBillboardId)
        {
            lbAlreadyBuffered = true;
            break;
        }
    }

    if (!lbAlreadyBuffered)
    {
        // Append {id, collectTime} and, if the buffer now tops the burst count, trip the challenge.
        CollectedBillboard lNewEntry;
        *reinterpret_cast<CgsID*>(lNewEntry.maBlob)         = lBillboardId;            // FLAG: id @+0
        *reinterpret_cast<f32*>(lNewEntry.maBlob + 8)       = mlfCurrentRaceTime;      // FLAG: collect time @+8
        maCollectedBillboards.Append(lNewEntry);

        if (static_cast<s32>(maCollectedBillboards.GetLength()) == KI_DEV_CHALLENGE_BILLBOARD_BURST_COUNT)
        {
            SetChallengeCompleted(E_DEV_CHALLENGE_BILLBOARD_BURST);
            maCollectedBillboards.Clear();
        }
    }
}

// ----------------------------------------------------------------------------
// OnTakedown  (X360 0x8238E208)
// liEventType: 0 == per-player "all takedowns" sweep, 3 == buffer this road-rage takedown.
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::OnTakedown(s32 liEventType, u32 luPlayerIndex)
{
    if (liEventType == 3)
    {
        // Road-rage takedown -> buffer the time for the burst-window check.
        CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
        CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");
        if (!mpProgressionManager->GetProfile()->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_ROAD_RAGE_BURST))
        {
            const f32 lfNow = mlfCurrentRaceTime;
            maRoadRageTakedownTimes.Push(&lfNow);
            if (maRoadRageTakedownTimes.GetLength() == KI_DEV_CHALLENGE_ROAD_RAGE_BURST_COUNT)
            {
                SetChallengeCompleted(E_DEV_CHALLENGE_ROAD_RAGE_BURST);
                maRoadRageTakedownTimes.Clear();
            }
        }
        return;
    }

    if (liEventType != 0)
    {
        return;
    }

    // liEventType == 0: the "took down every rival once" sweep. The X360 sets player luPlayerIndex's
    // bit in a per-player FastBitArray<8> dirty set, then iterates the set; when every active rival
    // has been taken down (the iterated count == rival count - 1) it trips E_DEV_CHALLENGE_ALL_TAKEDOWNS.
    CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
    CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");
    if (mpProgressionManager->GetProfile()->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_ALL_TAKEDOWNS))
    {
        return;
    }

    CGS_ASSERT(luPlayerIndex < 8u, "Index is out of range (max bits: 8)");

    // FLAG: the per-player taken-down dirty set is a FastBitArray<8> the X360 keeps inline in the
    // manager; it is NOT one of the layout members recovered from Construct (Construct does not zero
    // it, so it lives in a region this slice does not yet model). Modelled here as a function-local
    // sweep over a transient set to preserve the asm's iterate-count logic without inventing a member
    // offset. The owning member + the real rival-count source must replace this when the full layout
    // lands.
    CgsContainers::FastBitArray<8> lTakenDownPlayers;   // FLAG: should be a persisted manager member
    lTakenDownPlayers.UnSetAll();
    lTakenDownPlayers.SetBit(luPlayerIndex);

    s32 liTakenDownCount = 0;
    for (s32 liBit = lTakenDownPlayers.GetFirstBitSet();
         liBit != CgsContainers::FastBitArray<8>::KI_INVALID_BIT_INDEX;
         liBit = lTakenDownPlayers.GetNextBitSet(liBit))
    {
        ++liTakenDownCount;
    }

    // FLAG: the X360 compares liTakenDownCount against (rival count - 1) read from mpScoringSystem
    // (ss field +20200). Routed through the named accessor below; see GetActiveRivalCount note.
    CGS_ASSERT(mpScoringSystem != nullptr, "mpScoringSystem");
    const s32 liRivalCount = mpScoringSystem->GetCarCount();   // FLAG: X360 reads ss+20200 (rival count)
    if (liTakenDownCount == (liRivalCount - 1))
    {
        SetChallengeCompleted(E_DEV_CHALLENGE_ALL_TAKEDOWNS);
    }
}

// ----------------------------------------------------------------------------
// OnEventEnd  (X360 0x82396940)
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::OnEventEnd(s32 liGameModeType, bool lbWon)
{
    if (lbWon)
    {
        OnEventWin(liGameModeType);
    }
    // X360 then clears the pending road-rage event payload word (`*(this+144)=0`).
    mlPendingRoadRageEventData = 0;
}

// ----------------------------------------------------------------------------
// OnEventWin  (X360 0x8238DB10)
// liGameModeType: 3 == race (flawless + stunt-score-car), 7 == stunt run (stunt-score gates).
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::OnEventWin(s32 liGameModeType)
{
    if (liGameModeType == 3)
    {
        CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");
        const ::EActiveRaceCarIndex lePlayerRaceCarIndex = mpGameStateModule->GetPlayerActiveRaceCarIndex();
        CGS_ASSERT((lePlayerRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID)
                   && (lePlayerRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "( leLocalPlayerRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) "
                   "&& ( leLocalPlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

        CGS_ASSERT(mpScoringSystem != nullptr, "mpScoringSystem");
        CarData* lpCarData = mpScoringSystem->GetCarData(lePlayerRaceCarIndex);   // X360 sub_8231DCD0

        // FLAG: the X360 reads the player's finish score word at CarData+252 ( > 0 == a real score)
        // and the perfect/flawless flag at CarData+32 (== 0 -> flawless). Routed through the named
        // accessors below; see GetFinishScore / IsFlawless notes (declare-only on CarData).
        if (lpCarData->GetFinishScore() > 0)
        {
            // Car-specific stunt-win: the active car descends from "PUSCV01".
            CGS_ASSERT(mpGameStateModule->IsActiveRaceCarStillPresent(lePlayerRaceCarIndex),
                       "lpActiveCarInterface");
            const CgsID lActiveCarId = mpGameStateModule->GetActivePlayerCarId();
            const CgsID lTargetCarId = CgsIDCompress("PUSCV01");

            CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
            CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");
            BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();

            if (CheckCarID(lActiveCarId, lTargetCarId))
            {
                if (!lpProfile->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_EVENT_WIN))
                {
                    SetChallengeCompleted(E_DEV_CHALLENGE_EVENT_WIN);
                }
            }

            // Flawless win (no damage / perfect).
            // FLAG (asm 0x8238DF78): the binary reads +0x20 of the RaceCarEntityModuleIO ACTIVE-CAR
            // OUTPUT INTERFACE (RCEntit(mpGameStateModule+0x397E0, raceCarIndex)), NOT CarData+0x20.
            // Modelled provisionally as CarData::IsFlawless() (same offset, wrong object) until that
            // output-interface accessor is reconstructed -- consistent with the GetCarModelId flag.
            if (lpCarData->IsFlawless())
            {
                if (!lpProfile->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_EVENT_WIN_FLAWLESS))
                {
                    SetChallengeCompleted(E_DEV_CHALLENGE_EVENT_WIN_FLAWLESS);
                }
            }
        }
    }
    else if (liGameModeType == 7)
    {
        CGS_ASSERT(mpScoringSystem != nullptr, "mpScoringSystem");

        // The online vs offline stunt scorer is selected by game mode.
        const bool lbOnline = mpGameStateModule->IsOnlineGameMode();
        StuntModeScoring* lpStuntScorer = lbOnline ? mpScoringSystem->GetOnlineStuntScorer()
                                                   : mpScoringSystem->GetStuntScorer();
        CGS_ASSERT(lpStuntScorer != nullptr, "lpStuntModeScorer");

        // FLAG: the X360 reads the stunt scorer's running score (scorer+16) and its best multiplier
        // (scorer+456). Routed through the named accessors below; see GetScore / GetMultiplierScore
        // (declare-only on StuntModeScoring).
        const s32 liStuntScore = lpStuntScorer->GetCurrentScore();   // FLAG: X360 reads scorer+16

        CGS_ASSERT(mpProgressionManager != nullptr,               "mpProgressionManager");
        CGS_ASSERT(mpProgressionManager->GetProfile() != nullptr, "mpProgressionManager->GetProfile()");
        BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();

        // Stunt-score gate.
        if (!lpProfile->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_STUNT_SCORE)
            && liStuntScore >= KI_DEV_CHALLENGE_STUNT_SCORE_MIN)
        {
            SetChallengeCompleted(E_DEV_CHALLENGE_STUNT_SCORE);
        }

        // Car-specific stunt-score gate ("PUSRI01").
        if (!lpProfile->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_STUNT_SCORE_CAR)
            && liStuntScore >= KI_DEV_CHALLENGE_STUNT_SCORE_CAR_MIN)
        {
            CGS_ASSERT(mpProgressionManager->GetCurrentCarData() != nullptr,
                       "mpProgressionManager->GetCurrentCarData()");
            const CgsID lCurrentCarId = mpProgressionManager->GetCurrentCarData()->GetId();
            const CgsID lTargetCarId  = CgsIDCompress("PUSRI01");
            if (CheckCarID(lCurrentCarId, lTargetCarId))
            {
                SetChallengeCompleted(E_DEV_CHALLENGE_STUNT_SCORE_CAR);
            }
        }

        // Multiplier-driven win.
        if (!lpProfile->IsDeveloperChallengeComplete(E_DEV_CHALLENGE_EVENT_WIN_STUNT))
        {
            const s32 liMultiplierScore = lpStuntScorer->GetBestStuntScore();   // FLAG: X360 reads scorer+456
            if (liMultiplierScore >= KI_DEV_CHALLENGE_ROAD_RULE_TIME_MIN)
            {
                SetChallengeCompleted(E_DEV_CHALLENGE_EVENT_WIN_STUNT);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// PreWorldUpdate  (X360 0x8238D698)
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::PreWorldUpdate(GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer,
                                               GameStateModuleIO::OutputBuffer*         lpOutput)
{
    CGS_ASSERT(lpPreWorldInputBuffer != nullptr, "lpPreWorldInputBuffer");
    CGS_ASSERT(lpPreWorldInputBuffer->GetTimerStatusInterface() != nullptr,
               "lpPreWorldInputBuffer->GetTimerStatusInterface()");

    // Current race time = (s32 whole) + (f32 fraction) from the sim-timer entry of the timer status.
    // FLAG: the X360 reads the int at TimerStatusInterface+0x28 and the f32 at +0x2C; those land in
    // the second TimerStatusInterface::Entry (entry[1] base +0x18 -> miWord10 @+0x28, mfValue14 @+0x2C).
    const GameStateModuleIO::TimerStatusInterface* lpTimerStatus =
        lpPreWorldInputBuffer->GetTimerStatusInterface();
    mlfCurrentRaceTime = static_cast<f32>(lpTimerStatus->maEntries[1].miWord10)
                       + lpTimerStatus->maEntries[1].mfValue14;

    // The X360 guards the publish + age-out behind the developer-challenges-enabled debug flag; with
    // the flag on (the shipping/dev default for this watcher) it always runs.
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");

    // If a buffered road-rage takedown is pending AND the road-rage ring buffer is empty, publish the
    // pending event onto the output game-action queue. (The X360 walks the FifoQueue slots to test
    // "no buffered takedowns".)
    const bool lbNoBufferedTakedowns = (maRoadRageTakedownTimes.GetLength() == 0);
    if (lbNoBufferedTakedowns)
    {
        CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");

        // Event 298 (0x12A) carries the 8-byte pending payload; event 55 (0x37) is a 1-byte trailer.
        // Routed through the concrete-typed VariableEventQueue<13312,16> twin (GetGuiOutputQueue),
        // mirroring the PaybackManager precedent (the forward-declared GameActionQueue is incomplete).
        CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
        const u64 lPayload = mlPendingRoadRageEventData;
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPayload), 298, 8);
        const u8 luTrailer = 0;
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&luTrailer), 55, 1);
    }

    // Clear the pending payload, then age out the buffered takedowns + billboards.
    mlPendingRoadRageEventData = 0;
    UpdateBufferedRoadRageTakedowns();
    UpdateBufferedCollectedBillboards();
}

// ----------------------------------------------------------------------------
// UpdateBufferedRoadRageTakedowns  (X360 0x82367428)
// Drop buffered road-rage takedown times older than the window.
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::UpdateBufferedRoadRageTakedowns()
{
    const f32 lfCutoff = mlfCurrentRaceTime - KF_DEV_CHALLENGE_ROAD_RAGE_WINDOW;

    // Pop from the front while the oldest buffered time is older than the cutoff.
    f32 lfFront;
    while (maRoadRageTakedownTimes.Peek(&lfFront))
    {
        if (lfFront >= lfCutoff)
        {
            break;
        }
        maRoadRageTakedownTimes.Pop(&lfFront);
    }
}

// ----------------------------------------------------------------------------
// UpdateBufferedCollectedBillboards  (X360 0x82372F80)
// Drop buffered billboard collects older than the window.
// ----------------------------------------------------------------------------
void DeveloperChallengeManager::UpdateBufferedCollectedBillboards()
{
    const f32 lfCutoff = mlfCurrentRaceTime - KF_DEV_CHALLENGE_BILLBOARD_WINDOW;

    s32 liIndex = 0;
    while (liIndex < static_cast<s32>(maCollectedBillboards.GetLength()))
    {
        const CollectedBillboard& lrEntry = maCollectedBillboards[static_cast<u32>(liIndex)];
        // FLAG: collect time is the f32 @+8 of the provisional 16-byte record (see OnCollectStunt).
        const f32 lfCollectTime = *reinterpret_cast<const f32*>(lrEntry.maBlob + 8);   // FLAG: time @+8
        if (lfCollectTime < lfCutoff)
        {
            maCollectedBillboards.Erase(static_cast<u32>(liIndex));
            // Erase shifts the tail down; re-examine the same index.
        }
        else
        {
            ++liIndex;
        }
    }
}

// ----------------------------------------------------------------------------
// FLAGGED PLACEHOLDER threshold constants (rodata @ 0x82CDB97C..0x82CDB9A8).
// The literal values are NOT recoverable from the asm view (loaded from .data, no immediates).
// Defined here as flagged placeholders (0) so the TU compiles; NEVER trust these for behaviour.
// Ground from the rodata table when recovered.
// ----------------------------------------------------------------------------
const f32 KF_DEV_CHALLENGE_FLAT_SPIN_MIN_ANGLE     = 0.0f;   // FLAG: placeholder (flt_82CDB97C unknown)
const f32 KF_DEV_CHALLENGE_ROAD_RAGE_WINDOW        = 0.0f;   // FLAG: placeholder (flt_82CDB980 unknown)
const f32 KF_DEV_CHALLENGE_BILLBOARD_WINDOW        = 0.0f;   // FLAG: placeholder (flt_82CDB984 unknown)
const f32 KF_DEV_CHALLENGE_STUNT_OFFENCE_MIN_SCORE = 0.0f;   // FLAG: placeholder (flt_82CDB988 unknown)
const s32 KI_DEV_CHALLENGE_ROAD_RAGE_BURST_COUNT   = 0;      // FLAG: placeholder (dword_82CDB98C unknown)
const s32 KI_DEV_CHALLENGE_BILLBOARD_BURST_COUNT   = 0;      // FLAG: placeholder (dword_82CDB990 unknown)
const s32 KI_DEV_CHALLENGE_TAKEDOWN_CHAIN_MIN      = 0;      // FLAG: placeholder (dword_82CDB994 unknown)
const s32 KI_DEV_CHALLENGE_ROAD_RULE_SCORE_MIN     = 0;      // FLAG: placeholder (dword_82CDB998 unknown)
const s32 KI_DEV_CHALLENGE_STUNT_SCORE_MIN         = 0;      // FLAG: placeholder (dword_82CDB99C unknown)
const s32 KI_DEV_CHALLENGE_STUNT_SCORE_CAR_MIN     = 0;      // FLAG: placeholder (dword_82CDB9A0 unknown)
const s32 KI_DEV_CHALLENGE_ROAD_RULE_TIME_MIN      = 0;      // FLAG: placeholder (dword_82CDB9A4 unknown)
const f32 KF_DEV_CHALLENGE_STUNT_COMBO_MIN         = 0.0f;   // FLAG: placeholder (flt_82CDB9A8 unknown)

}
