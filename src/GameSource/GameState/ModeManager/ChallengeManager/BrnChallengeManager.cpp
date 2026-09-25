// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- the top-level CHALLENGE-mode manager (FILE/CLASS TU,
// 57 functions in the X360 ARTIST build). KEYSTONE STATE: the type is homed with NAMED
// members at X360-asm-attested offsets (see BrnChallengeManager.h for the layout evidence);
// the methods below are the previously-committed foundation bodies re-expressed over the
// named members (behaviour-identical; the old opaque-buffer FieldAt reads map 1:1 onto the
// members at the same offsets). The remaining methods are bodied by the wave-B partfiles
// (BrnChallengeManager_wB_*.cpp) and consolidated here.
//
// BODIED (this file):
//   Construct, UpdateResultsTimer, ReceivedSuccessUpdatesFromAllPlayers,
//   GetNumPlayerSucceeding, GetNumPlayersContributing,
//   GetChallengeStyle, CountCompletedChallenges, ResetActionData, UpdateStuntScores.
//   (GetFreeburnChallengeList / GetProgressionManager / GetLocalChallengeCompletionData are
//    now inline in the header.)
//
// BLOCKED (NOT bodied, VMX128 toolchain): CheckCurrentLocation @0x82333878,
//   IsPointInTriggerRegion @0x82333368, UpdateLeaptCars @0x82333BB8 (lvx128/vmsum3fp128/
//   vperm vector code; see the wave-B spec).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "SharedClasses/DataLists/ChallengeListEntry.h" // BrnResource::ChallengeListEntry::GetNumPlayers/GetChallengeStyle
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent::Register
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint  ([fburn] diag)

#include <cstddef>   // offsetof (layout assert)
#include <stdlib.h>  // getenv     ([fburn] diag)

// includes folded in from the BrnChallengeManager_w*.cpp partfiles (2026-09-15)
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "SharedClasses/DataLists/ChallengeList.h"          // BrnResource::ChallengeList::GetChallengeCount/GetChallengeData
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // BrnProgression::ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                 // BrnProgression::Profile
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface, BoostOutputInfo
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // TGameActionQueue::AddEvent, CgsModule::Event
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h" // CgsModule::BaseEventQueue<T>::GetLength/GetEvent
#include <cstring>                                           // memcpy
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // IsSimTimerFrequency50Hz
#include "SharedClasses/StreetData/BrnChallengeData.h"            // BrnStreetData::{ChallengePlayerScoreEntry, ScoreType}
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"       // ScoringSystem::GetCarData
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                 // ModeManager::GetScoringSystem
#include "GameSource/GameState/BrnGameStateModule.h"                         // GameStateModule::GetPlayerActiveRaceCarIndex / GetModeManager
#include "GameSource/GameState/BrnGameStateModuleIO.h"                       // OutputBuffer accessors
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // GameStateToNetworkInterface::SetPlayerInFreeburnChallenge
#include "GameSource/Network/BrnNetworkModuleIO.h"                // BrnNetworkModuleIO::E_CHALLENGE_EVENT_* (values 0..3)
#include "GameShared/GameClasses/Development/CgsStrStream.h"      // CgsDev::StrStream (runtime-value assert message)
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"     // [net] fburn status witness (PC harness)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"        // CgsContainers::BitArray<8> (case-6 rank scratch)
#include "SharedClasses/DataLists/VehicleList.h"                 // BrnResource::VehicleList::GetVehicleIndex / GetVehicleData
#include "SharedClasses/DataLists/VehicleListEntry.h"            // BrnResource::VehicleListEntry::GetCarType
#include <cfloat>                                                // FLT_MAX (flt_82020AFC)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"             // CgsWorld::WorldMap2D::GetValue
#include "SharedClasses/Trigger/BrnTriggerData.h"                   // BrnTrigger::TriggerData::GetRegion
#include "SharedClasses/Trigger/BrnTriggerBase.h"                   // BrnTrigger::TriggerRegion (GetId / GetType / GetBoxRegion)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                 // BrnTrigger::GenericRegion::GetType (the sub-type byte)
#include "SharedClasses/Trigger/BrnRegion.h"                        // BrnTrigger::BoxRegion (GetPosition / GetDimension* / ComputeTransform)
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // GetTriggerData / GetActiveTriggerCount / GetActiveTrigger
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"     // RoadRulesManager::GetCurrentRoadID
#include "GameSource/Math/BrnMathUtils.h"                           // BrnMath::Flatten / BrnMath::IsPointInsideBox
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // BrnPhysics::Vehicle::RaceCarState members
#include "rw/math/vpu/vector3_operation.h"                          // operator- / operator* / MagnitudeSquared
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include <cmath>                                                  // std::floor (the de-inlined fsel/magic-constant floor)

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// File-scope constants the bodied methods use. KF_RESULTS_TIME_OUT is the DWARF-named
// const (BrnChallengeManager.cpp:35); the X360 immediate is 5.5f (flt_8202127C).
// ----------------------------------------------------------------------------
static const f32 KF_RESULTS_TIME_OUT = 5.5f;

// The two "make all challenges N-player" debug toggles (DWARF `extern bool` statics; X360
// backs them with byte_82FAD9C0/C1). Default-cleared, exactly as Construct re-clears the globals.
bool ChallengeManager::mbChallengesAreAllOnePlayer = false;
bool ChallengeManager::mbChallengesAreAllTwoPlayer = false;

// ----------------------------------------------------------------------------
// External-data byte-offset helper: ONLY for the un-homed stunt-score snapshot blob
// UpdateStuntScores receives (external-data-by-attested-offset allowance). The manager's own
// state is reached exclusively through named members.
// ----------------------------------------------------------------------------
namespace
{
    template <typename T>
    inline const T& ExternalFieldAt(const u8* lpBlob, u32 luOffset)
    {
        return *reinterpret_cast<const T*>(lpBlob + luOffset);
    }
}

// ----------------------------------------------------------------------------
// Construct -- X360 0x82332DB0. EXECUTED in the boot-trace milestone. Store-for-store
// reconstruction of the X360 body (same field set, same values, same call order):
// scalar/timer/flag resets, the per-slot completion-record init (-1 player ids), the
// per-player success-mask clears, ClearPlayerSuccessData, the local completion clear,
// the back-pointer asserts + caches, the embedded debug component init + Register, the
// static debug-toggle re-clears, ClearFreeburnSkillsThisFrame, and the cached-skill sweep.
// ----------------------------------------------------------------------------
void ChallengeManager::Construct(GameStateModule*                    lpGameStateModule,
                                 ModeManager*                        lpModeManager,
                                 BrnProgression::ProgressionManager* lpProgression,
                                 const RoadRulesManager*             lpRoadRulesManager,
                                 const TriggerQueryManager*          lpTriggerQueryManager)
{
    mfChallengeTimer = 0.0f;                                    // X360 stfs 0.0,+0xE18
    mfConvoyTimer    = 0.0f;                                    // X360 stfs 0.0,+0xE20
    mfResultsTimer   = 0.0f;                                    // X360 stfs 0.0,+0xE28
    meChallengeManagerStatus = E_CHALLENGE_MANAGER_STATUS_NONE; // X360 stw 0,+0xE08
    mfLeapCarsValidTimer = -1.0f;                               // X360 stfs -1.0,+0x110
    mbChallengeTimerRunning = false;                            // X360 stb 0,+0xE1C
    mbConvoyTimerRunning    = false;                            // X360 stb 0,+0xE24
    mbResultsTimerRunning   = false;                            // X360 stb 0,+0xE2C
    mbRemoteStartPending    = false;                            // X360 stb 0,+0xE35
    mbRemoteTriggerPending  = false;                            // X360 stb 0,+0xE36
    mbRemoteEndPending      = false;                            // X360 stb 0,+0xE37
    mbChallengeRequiresLeapCars = false;                        // X360 stb 0,+0xE34
    mpCurrentChallenge       = 0;                               // X360 stw 0,+0xE0C
    miCurrentChallengeAction = 0;                               // X360 stw 0,+0xE10
    miCurrentArbitrationIndex = 0;                              // X360 stw 0,+0xE14
    mpVehicleList = 0;                                          // X360 stw 0,+0x5C0
    miLastChallengeResetFrame = -1;                             // X360 stw -1,+0x1000
    mbUpdateLeaptCars = false;                                  // X360 stb 0,+0x118
    miNumCarsLeapt = 0;                                         // X360 stw 0,+0x114

    // Per remote-player completion record: clear the bit store, free the slot (-1), clear the
    // finalised flag (X360 loop base +0x5C8, stride 0x108: 32 std, stw -1 @+0x100, stb 0 @+0x104).
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        maChallengeCompletionData[liSlot].mCompletedChallenges.UnSetAll();
        maChallengeCompletionData[liSlot].mNetworkPlayerID = -1;
        maChallengeCompletionData[liSlot].mbFinalised      = false;
    }

    // Per-player success-update masks (X360 loop base +0x3A8: 2 std per player).
    for (s32 liPlayer = 0; liPlayer < KI_MAX_CHALLENGE_PLAYERS; ++liPlayer)
    {
        maPlayerSuccessUpdateArray[liPlayer].UnSetAll();
    }

    ClearPlayerSuccessData();

    mLocalChallengeCompletionData.UnSetAll();  // X360 loop base +0xD00: 32 std
    mLastSecondSuccessStatus.UnSetAll();       // X360 std 0,+0xE00
    mPotentiallyLeaptCars.Construct();         // X360 std 0,+0x100 (occupancy only; Prepare Clear()s)
    miNumCarsLeapt = 0;                        // X360 re-clears +0x114
    maBillboardsCollected.Construct();         // X360 stw 0,+0x3A0 (miCount = 0)

    // Back-pointer non-null asserts (X360 order + message strings; BrnChallengeManager.cpp:265-269).
    CGS_ASSERT(lpProgression != 0, "lpProgression");
    CGS_ASSERT(lpRoadRulesManager != 0, "lpRoadRulesManager");
    CGS_ASSERT(lpTriggerQueryManager != 0, "lpTriggerQueryManager");
    CGS_ASSERT(lpGameStateModule != 0, "lpGameStateModule");
    CGS_ASSERT(lpModeManager != 0, "lpModeManager");

    mpGameStateModule    = lpGameStateModule;     // X360 stw,+0xE48
    mpModeManager        = lpModeManager;         // X360 stw,+0xE4C (X360-only member)
    mpProgression        = lpProgression;         // X360 stw,+0xE44
    mpRoadRulesManager   = lpRoadRulesManager;    // X360 stw,+0xE3C
    mpTriggerQueryManager = lpTriggerQueryManager; // X360 stw,+0xE40

    // Embedded debug component init (X360 inline stores over +0x1004..+0x1018) + registration.
    mChallengeManagerDebugComponent.mpChallengeManager = this;             // stw this,+0x1010
    mChallengeManagerDebugComponent.mbDebugBeginChallengePending = false;  // stb 0,+0x1018
    mChallengeManagerDebugComponent.miChallengeIndex = 0;                  // stw 0,+0x1014
    mChallengeManagerDebugComponent.Register();

    // Re-clear the two debug "make all challenges N-player" globals (X360 stb 0,byte_82FAD9C0/C1).
    mbChallengesAreAllOnePlayer = false;
    mbChallengesAreAllTwoPlayer = false;
    mbWereAllChallengesOnePlayer = false;   // X360 stb 0,+0x101C
    mbWereAllChallengesTwoPlayer = false;   // X360 stb 0,+0x101D

    ClearFreeburnSkillsThisFrame();

    // Cached per-skill location-enter values (X360 loop base +0xF34, 38 stfs; the post-increment
    // operator carries the "leEnumIndex <= E_FREEBURN_SKILL_COUNT" assert, BrnChallengeManager.h:113).
    for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START;
         static_cast<s32>(leSkill) < KI_FREEBURN_SKILL_COUNT_X360;
         leSkill++)
    {
        mafCachedActiveSkillValueOnLocationEnter[leSkill] = 0.0f;
    }

    mScoresSetThisFrameBitArray.UnSetAll();   // X360 std 0,+0xFD0

    // ==============================================================================================
    // [DIAG] NOT IN THE X360 BINARY -- the `[fburn]` witness (added 2026-09-07, the round that
    // embedded mChallengeManager in ModeManager). Gated on BRN_MODEMGR_DIAG, the same env the
    // `[evt-finish]` / `[queue-hwm]` / `[mode-results]` witnesses in this subsystem stand behind.
    // ==============================================================================================
    // WHY IT EARNS ITS PLACE. Construct is the FIRST observable moment of the whole freeburn
    // subsystem, and it is the one fact that separates "the ChallengeManager mount landed and the
    // ModeManager call was un-parked" from "the member is embedded but nothing calls it" -- which is
    // exactly the state this file's callers are in today. Without it a run tells you nothing either
    // way. One line per Construct, i.e. once per ModeManager::Construct, so it is not a sampler.
    // ⛔ NO SIDE EFFECTS: prints the two back-pointers' non-nullness and nothing else.
    // DELETE-WHEN the freeburn bring-up is done.
    {
        static const bool sbFburnDiag = (getenv("BRN_MODEMGR_DIAG") != 0);
        if (sbFburnDiag && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[fburn] ChallengeManager::Construct DONE"
                << " modeMgr "    << (mpModeManager != 0 ? 1 : 0)
                << " gameState "  << (mpGameStateModule != 0 ? 1 : 0)
                << " roadRules "  << (mpRoadRulesManager != 0 ? 1 : 0)
                << " triggerQry " << (mpTriggerQueryManager != 0 ? 1 : 0) << "\n";
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateResultsTimer -- X360 0x82323408. Arms the results countdown on first call
// (5.5s when the local challenge finished with SUCCESS, else 0.0), then counts it down one
// frame and returns true once it has expired. The X360 uses fsel to clamp at 0.0.
// ----------------------------------------------------------------------------
bool ChallengeManager::UpdateResultsTimer(f32 lfTimeStep)
{
    if (!mbResultsTimerRunning)
    {
        if (meLocalChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)   // X360 lwz +0xE30 == 1
        {
            mfResultsTimer = KF_RESULTS_TIME_OUT;  // 5.5
        }
        else
        {
            mfResultsTimer = 0.0f;
        }
        mbResultsTimerRunning = true;
    }

    const bool lbWasRunning = mbResultsTimerRunning;
    if (!lbWasRunning)
    {
        return true;   // X360 LABEL_11 path: v5==0 -> v5==0 returns 1
    }

    if (mfResultsTimer > 0.0f)
    {
        // f0 = timer - dt; fsel(-(timer-dt), 0.0, f0) clamps the result at 0.0 (X360 fsel f0,f12,f13,f0).
        const f32 lfNew = mfResultsTimer - lfTimeStep;
        mfResultsTimer = (lfNew >= 0.0f) ? lfNew : 0.0f;
    }

    // Expired iff the timer is running AND has reached <= 0.0 (X360: v8=0 unless still >0.0).
    if (mfResultsTimer > 0.0f)
    {
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// ReceivedSuccessUpdatesFromAllPlayers -- X360 0x82316DE8. True once the count of per-player
// received-update flags (mabReceivedSuccessUpdates[0..7]) equals the current challenge's player
// count. Asserts the current challenge is set and that the count never exceeds the player count.
// ----------------------------------------------------------------------------
bool ChallengeManager::ReceivedSuccessUpdatesFromAllPlayers()
{
    CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");

    s32 liNumUpdatesReceived = (mabReceivedSuccessUpdates[0] != 0) ? 1 : 0;
    for (s32 liPlayer = 1; liPlayer < KI_MAX_CHALLENGE_PLAYERS; ++liPlayer)
    {
        if (mabReceivedSuccessUpdates[liPlayer])
        {
            ++liNumUpdatesReceived;
        }
    }

    const s32 liNumPlayers = mpCurrentChallenge->GetNumPlayers();
    CGS_ASSERT(liNumUpdatesReceived <= liNumPlayers,
               "liNumUpdatesReceived <= mpCurrentChallenge->GetNumPlayers()");

    return liNumPlayers == liNumUpdatesReceived;
}

// ----------------------------------------------------------------------------
// GetNumPlayerSucceeding -- X360 0x82316F00. Tally the players whose per-action success status
// is DONE (==3) for the given action slot (X360 indexes the grid as base 0x428 + 8*player +
// 4*action == maaePlayersSuccessStatus[player][action]).
// ----------------------------------------------------------------------------
s32 ChallengeManager::GetNumPlayerSucceeding(s32 liActionIndex)
{
    s32 liNumPlayersSucceeding = 0;
    for (s32 liPlayer = 0; liPlayer < KI_MAX_CHALLENGE_PLAYERS; ++liPlayer)
    {
        if (maaePlayersSuccessStatus[liPlayer][liActionIndex] ==
            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
        {
            ++liNumPlayersSucceeding;
        }
    }

    CGS_ASSERT(liNumPlayersSucceeding <= mpCurrentChallenge->GetNumPlayers(),
               "liNumPlayersSucceeding <= mpCurrentChallenge->GetNumPlayers()");

    return liNumPlayersSucceeding;
}

// ----------------------------------------------------------------------------
// GetNumPlayersContributing -- X360 0x82317020. As above but counts CONTRIBUTING (==2).
// ----------------------------------------------------------------------------
s32 ChallengeManager::GetNumPlayersContributing(s32 liActionIndex)
{
    s32 liNumPlayersContributing = 0;
    for (s32 liPlayer = 0; liPlayer < KI_MAX_CHALLENGE_PLAYERS; ++liPlayer)
    {
        if (maaePlayersSuccessStatus[liPlayer][liActionIndex] ==
            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING)
        {
            ++liNumPlayersContributing;
        }
    }

    CGS_ASSERT(liNumPlayersContributing <= mpCurrentChallenge->GetNumPlayers(),
               "liNumPlayersContributing <= mpCurrentChallenge->GetNumPlayers()");

    return liNumPlayersContributing;
}

// ----------------------------------------------------------------------------
// GetChallengeStyle -- X360 0x82355FA8 (DWARF BrnChallengeManager.h:196). Returns the active
// challenge's freeburn style, or E_FREEBURN_STYLE_NONE unless the manager is RUNNING.
// ----------------------------------------------------------------------------
BrnResource::ChallengeListEntry::EFreeburnChallengeStyle ChallengeManager::GetChallengeStyle() const
{
    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz +0xE08 != 2
    {
        return BrnResource::ChallengeListEntry::E_FREEBURN_STYLE_NONE;
    }

    CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");

    return mpCurrentChallenge->GetChallengeStyle();
}

// ----------------------------------------------------------------------------
// CountCompletedChallenges -- X360 0x8233E530. Number of set bits in the local completion bit
// array (mLocalChallengeCompletionData @+0xD00). The X360 body is the container's set-bit iterator
// fully inlined: it walks 32 u64 fields (v1 bound 0x20) with a 2000-bit index bound (0x7D0), which
// is exactly FastBitArray<2000> (ceil(2000/64)==32 fields). The iterator lives entirely on the
// stack, so the function only reads the array. This is the value-identical
// GetFirstBitSet/GetNextBitSet scan over the same FastBitArray<2000>.
// ----------------------------------------------------------------------------
s32 ChallengeManager::CountCompletedChallenges()
{
    s32 liNumCompleted = 0;
    for (s32 liBit = mLocalChallengeCompletionData.GetFirstBitSet();
         liBit != CgsContainers::FastBitArray<2000>::KI_INVALID_BIT_INDEX;
         liBit = mLocalChallengeCompletionData.GetNextBitSet(liBit))
    {
        ++liNumCompleted;
    }
    return liNumCompleted;
}

// ----------------------------------------------------------------------------
// ResetActionData -- X360 0x823246F0. Reset ONE action's scratch (the single-action sibling of
// ResetCurrentChallengeData in wB_02): re-seed the action's remaining target from its first
// target value (X360 lwz action+0x34 == maiTargetValue[0]; the constant-index guard of
// GetTargetValue(0) folds away), drop its "individual success update sent" flag, then for every
// active-race-car slot clear that action's cumulative contribution + success status, and zero
// the banked score of the skill this action's type maps to. The action-type -> skill lookup
// (KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL, dumped from the XEX and re-verified against
// dword_82021288 -- 41 entries proven by this function's own `cmplwi 0x29` GetActionType guard
// AND by the next rodata item byte_8202132C starting at entry [41]'s address) is loop-invariant
// but sits INSIDE the car loop exactly as the X360 emits it (the lbz + range assert repeat per
// iteration). 38 == KI_FREEBURN_SKILL_COUNT_X360 is the "no skill" sentinel; the BILLBOARDS
// skill additionally drops the collected-billboards tally (X360 stw 0,+0x3A0 == miCount).
// ----------------------------------------------------------------------------
void ChallengeManager::ResetActionData(s32 liActionToResetIndex)
{
    CGS_ASSERT(liActionToResetIndex < mpCurrentChallenge->GetNumActions(),
               "liActionToResetIndex < mpCurrentChallenge->GetNumActions()");

    const BrnResource::ChallengeListEntryAction* lpAction =
        mpCurrentChallenge->GetAction(liActionToResetIndex);

    maiRemainingTarget[liActionToResetIndex]                    = lpAction->GetTargetValue(0); // X360 lwz +0x34 -> stw +0x588+4*idx
    mabIndividualActionsSuccessUpdateSent[liActionToResetIndex] = false;                       // X360 stb 0,+0xFDA+idx

    // The post-increment carries the X360 in-loop "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT"
    // assert (BurnoutConstants.h:39), matching the asm's per-iteration guard.
    for (EActiveRaceCarIndex leCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         static_cast<s32>(leCarIndex) < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leCarIndex++)
    {
        maafCumulativeContributions[leCarIndex][liActionToResetIndex] = 0.0f;  // X360 stfs 0.0,+0x4A8+8*car+4*idx
        maaePlayersSuccessStatus[leCarIndex][liActionToResetIndex] =
            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;              // X360 stw 0,+0x428+8*car+4*idx

        const s32 liSkill = KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
        if (liSkill != KI_FREEBURN_SKILL_COUNT_X360)   // 38 == "no skill for this action type"
        {
            mafBankedActionScores[liSkill] = 0.0f;     // X360 stfs 0.0,+0x4E8+4*skill
            if (liSkill == E_FREEBURN_SKILL_BILLBOARDS)
            {
                maBillboardsCollected.Clear();         // X360 stw 0,+0x3A0 (miCount = 0)
            }
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateStuntScores -- X360 0x82334740. Push a completed stunt run's per-skill tallies into the
// current-frame freeburn skill scores: for each non-zero field of the incoming stunt-score struct,
// call SetCurrentSkillScore with the matching skill id and the field value (as f32).
//
// lpStuntScoreInfo is an un-homed external stunt-score snapshot handed in by PostWorldUpdate. It is
// not reconstructable as a named type here, so its fields are read by attested byte offset
// (external-data allowance). The X360 first block-copies [0x00..0x06] (7 bytes) then [0x08..0x13]
// (six u16s) into stack scratch before reading them back; that copy is a no-op for our purposes, so
// the fields are read directly at their source offsets: per-skill count bytes at [0x00..0x06], a
// 1-byte gap at [0x07], three u16 counts interleaved with bytes across [0x08..0x12], a s32
// air-distance value at [0x14], and a flag byte at [0x18]. Each non-zero field is pushed with
// lbScoredThisFrame==true, except the [0x14] branch which forwards the [0x18] flag.
//
// X360/PS3-DWARF DRIFT: the skill ids below (21..36) exceed the PS3 EFreeburnSkill enumerator set
// (named through 18). The X360 build's EFreeburnSkill carries additional stunt-combo skills
// (KI_FREEBURN_SKILL_COUNT_X360 == 38); the literal ids are attested verbatim in the X360 asm
// (li r4,0x15..0x24) and are cast to the enum without inventing names for the drifted enumerators.
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateStuntScores(const void* lpStuntScoreInfo)
{
    CGS_ASSERT(lpStuntScoreInfo != 0, "lpStuntScoreInfo");

    const u8* lpInfo = static_cast<const u8*>(lpStuntScoreInfo);

    if (ExternalFieldAt<u8>(lpInfo, 0x06) != 0)
    {
        SetCurrentSkillScore(static_cast<EFreeburnSkill>(27), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x06)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x00) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(21), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x00)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x01) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(22), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x01)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x02) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(23), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x02)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x03) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(24), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x03)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x04) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(25), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x04)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x05) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(26), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x05)), true);
    }

    if (ExternalFieldAt<u8>(lpInfo, 0x12) != 0)
    {
        SetCurrentSkillScore(static_cast<EFreeburnSkill>(35), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x12)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x0E) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(28), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x0E)), true);
        if (ExternalFieldAt<u16>(lpInfo, 0x08) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(29), static_cast<f32>(ExternalFieldAt<u16>(lpInfo, 0x08)), true);
        if (ExternalFieldAt<u16>(lpInfo, 0x0A) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(30), static_cast<f32>(ExternalFieldAt<u16>(lpInfo, 0x0A)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x0F) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(31), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x0F)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x10) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(32), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x10)), true);
        if (ExternalFieldAt<u16>(lpInfo, 0x0C) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(33), static_cast<f32>(ExternalFieldAt<u16>(lpInfo, 0x0C)), true);
        if (ExternalFieldAt<u8>(lpInfo, 0x11) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(34), static_cast<f32>(ExternalFieldAt<u8>(lpInfo, 0x11)), true);
    }

    const s32 liAirDistance = ExternalFieldAt<s32>(lpInfo, 0x14);
    if (liAirDistance > 0)
    {
        SetCurrentSkillScore(static_cast<EFreeburnSkill>(36),
                             static_cast<f32>(liAirDistance),
                             ExternalFieldAt<u8>(lpInfo, 0x18) != 0);
    }
}

// ----------------------------------------------------------------------------
// _AssertLayout -- never called. Pins the X360-attested layout on the x64 gate:
// ABSOLUTE offsets through the pointer-free prefix (everything before mpVehicleList, PLUS the
// pointer-offset-coincident run through meChallengeManagerStatus -- the X360 pads +0x5C4..0x5C8
// where x64 widens the pointer to 8, so +0x5C8..+0xE0C stay aligned across both ABIs until the
// next pointer member), RELATIVE anchors inside the pointer-free runs after that.
// ----------------------------------------------------------------------------
void ChallengeManager::_AssertLayout()
{
    // -- pointer-free prefix: absolute X360 offsets hold on x64 --
    static_assert(offsetof(ChallengeManager, mPotentiallyLeaptCars)   == 0x000, "pool @0x000");
    static_assert(offsetof(ChallengeManager, mfLeapCarsValidTimer)    == 0x110, "mfLeapCarsValidTimer @0x110");
    static_assert(offsetof(ChallengeManager, miNumCarsLeapt)          == 0x114, "miNumCarsLeapt @0x114");
    static_assert(offsetof(ChallengeManager, mbUpdateLeaptCars)       == 0x118, "mbUpdateLeaptCars @0x118");
    static_assert(offsetof(ChallengeManager, maBillboardsCollected)   == 0x120, "maBillboardsCollected @0x120");
    static_assert(offsetof(ChallengeManager, maPlayerSuccessUpdateArray) == 0x3A8, "maPlayerSuccessUpdateArray @0x3A8");
    static_assert(offsetof(ChallengeManager, maaePlayersSuccessStatus)   == 0x428, "maaePlayersSuccessStatus @0x428");
    static_assert(offsetof(ChallengeManager, maafCurrentActionsScores)   == 0x468, "maafCurrentActionsScores @0x468");
    static_assert(offsetof(ChallengeManager, maafCumulativeContributions) == 0x4A8, "maafCumulativeContributions @0x4A8");
    static_assert(offsetof(ChallengeManager, mafBankedActionScores)   == 0x4E8, "mafBankedActionScores @0x4E8");
    static_assert(offsetof(ChallengeManager, mafCumulativeActionScores) == 0x580, "mafCumulativeActionScores @0x580");
    static_assert(offsetof(ChallengeManager, maiRemainingTarget)      == 0x588, "maiRemainingTarget @0x588");
    static_assert(offsetof(ChallengeManager, mabReceivedSuccessUpdates) == 0x590, "mabReceivedSuccessUpdates @0x590");
    static_assert(offsetof(ChallengeManager, mabPlayerStartedChallenge) == 0x598, "mabPlayerStartedChallenge @0x598");
    static_assert(offsetof(ChallengeManager, maiCrashedWithChallengePlayer) == 0x5A0, "maiCrashedWithChallengePlayer @0x5A0");
    static_assert(offsetof(ChallengeManager, mpVehicleList)           == 0x5C0, "mpVehicleList @0x5C0 (first pointer)");

    // -- ABI-coincident run (X360 pointer+pad == x64 pointer): still absolute --
    static_assert(offsetof(ChallengeManager, maChallengeCompletionData)    == 0x5C8, "maChallengeCompletionData @0x5C8");
    static_assert(offsetof(ChallengeManager, mLocalChallengeCompletionData) == 0xD00, "mLocalChallengeCompletionData @0xD00");
    static_assert(offsetof(ChallengeManager, mLastSecondSuccessStatus)     == 0xE00, "mLastSecondSuccessStatus @0xE00");
    static_assert(offsetof(ChallengeManager, meChallengeManagerStatus)     == 0xE08, "meChallengeManagerStatus @0xE08");

    // -- completion record: pointer-free, both ABIs identical --
    static_assert(sizeof(ChallengeCompletionData) == 0x108, "ChallengeCompletionData stride 0x108");
    static_assert(offsetof(ChallengeCompletionData, mNetworkPlayerID) == 0x100, "mNetworkPlayerID @+0x100");
    static_assert(offsetof(ChallengeCompletionData, mbFinalised)      == 0x104, "mbFinalised @+0x104");

    // -- pointer-free run miCurrentChallengeAction(+0xE10 X360)..mbRemoteEndPending(+0xE37):
    //    relative anchors (mpCurrentChallenge widens on x64) --
    static_assert(offsetof(ChallengeManager, mbRemoteEndPending) - offsetof(ChallengeManager, miCurrentChallengeAction) == 0x27,
                  "timer/flag run spans 0x27 (X360 0xE10..0xE37)");
    static_assert(offsetof(ChallengeManager, mfResultsTimer) - offsetof(ChallengeManager, miCurrentChallengeAction) == 0x18,
                  "mfResultsTimer +0x18 into the run (X360 0xE28)");
    static_assert(offsetof(ChallengeManager, meLocalChallengeStatus) - offsetof(ChallengeManager, miCurrentChallengeAction) == 0x20,
                  "meLocalChallengeStatus +0x20 into the run (X360 0xE30)");

    // -- pointer-free run mabBankedSkillThisFrame(+0xE50 X360)..miLastChallengeResetFrame(+0x1000):
    //    relative anchors (five back-pointers widen on x64) --
    static_assert(offsetof(ChallengeManager, mabActiveSkillThisFrame) - offsetof(ChallengeManager, mabBankedSkillThisFrame) == 0x26,
                  "mabActiveSkillThisFrame +0x26 (X360 0xE76)");
    static_assert(offsetof(ChallengeManager, mafActiveSkillValue) - offsetof(ChallengeManager, mabBankedSkillThisFrame) == 0x4C,
                  "mafActiveSkillValue +0x4C (X360 0xE9C)");
    static_assert(offsetof(ChallengeManager, mafCachedActiveSkillValueOnLocationEnter) - offsetof(ChallengeManager, mabBankedSkillThisFrame) == 0xE4,
                  "mafCachedActiveSkillValueOnLocationEnter +0xE4 (X360 0xF34)");
    static_assert(offsetof(ChallengeManager, mScoresSetThisFrameBitArray) - offsetof(ChallengeManager, mabBankedSkillThisFrame) == 0x180,
                  "mScoresSetThisFrameBitArray +0x180 (X360 0xFD0)");
    static_assert(offsetof(ChallengeManager, mabIsLocationOk) - offsetof(ChallengeManager, mabBankedSkillThisFrame) == 0x188,
                  "mabIsLocationOk +0x188 (X360 0xFD8)");
    static_assert(offsetof(ChallengeManager, mabIndividualActionsSuccessUpdateSent) - offsetof(ChallengeManager, mabBankedSkillThisFrame) == 0x18A,
                  "mabIndividualActionsSuccessUpdateSent +0x18A (X360 0xFDA)");
    static_assert(offsetof(ChallengeManager, maChallengePlayerCounts) - offsetof(ChallengeManager, mabBankedSkillThisFrame) == 0x18C,
                  "maChallengePlayerCounts +0x18C (X360 0xFDC)");
    static_assert(offsetof(ChallengeManager, miFramesSinceNetworkStart) - offsetof(ChallengeManager, mabBankedSkillThisFrame) == 0x1AC,
                  "miFramesSinceNetworkStart +0x1AC (X360 0xFFC)");
    static_assert(offsetof(ChallengeManager, miLastChallengeResetFrame) - offsetof(ChallengeManager, mabBankedSkillThisFrame) == 0x1B0,
                  "miLastChallengeResetFrame +0x1B0 (X360 0x1000)");

    // -- trailing flags stay adjacent after the embedded debug component --
    static_assert(offsetof(ChallengeManager, mbWereAllChallengesTwoPlayer) - offsetof(ChallengeManager, mbWereAllChallengesOnePlayer) == 1,
                  "were-all flags adjacent (X360 0x101C/0x101D)");
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_00.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_00.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the skill-score WRITE core.
// Store-for-store reconstructions of three X360 ARTIST methods over the keystone's NAMED
// members (see BrnChallengeManager.h for the layout evidence):
//
//   BankSkillScore               @ 0x82317140  (DWARF :692)  -- the actual bank primitive
//   SetCurrentSkillScore         @ 0x82324290  (DWARF :686)  -- per-frame skill-score writer
//   UpdateLocationOKStatusChange @ 0x82323FF8  (DWARF :590)  -- on location-exit, bank pending
//
// BankSkillScore is bodied first because both SetCurrentSkillScore and
// UpdateLocationOKStatusChange call it. All three use the X360-recovered per-skill flag
// tables (KAB_FREEBURN_SKILL_IS_CONTINUOUS / KAB_FREEBURN_SKILL_BANK_ON_LOCATION_EXIT) and
// the mScoresSetThisFrameBitArray FastBitArray<38>; the per-skill loops use the header's
// EFreeburnSkill operator++ (which reproduces the inlined h:113 count assert).
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// BankSkillScore -- X360 0x82317140. Commit one skill's running value to the banked stores
// for the current challenge action. Non-negative scores only (a negative score is a no-op:
// the X360 compares against flt_82001CC0 (0.0) and skips the whole body when score < 0.0).
// On a valid score it flags the skill banked + active this frame, records the running value,
// then adds-or-replaces the per-skill banked-action total depending on the current action's
// co-op type (INDIVIDUAL_ACCUMULATION accumulates; every other type overwrites).
// ----------------------------------------------------------------------------
void ChallengeManager::BankSkillScore(EFreeburnSkill leFreeburnSkill, f32 lfScore)
{
    if (lfScore >= 0.0f)   // X360 fcmpu f31,flt_82001CC0(0.0); blt -> skip body
    {
        mabBankedSkillThisFrame[leFreeburnSkill] = true;   // X360 stb 1,+0xE50 (base + skill)
        mabActiveSkillThisFrame[leFreeburnSkill] = true;   // X360 stb 1,+0xE76 (base + skill)
        mafActiveSkillValue[leFreeburnSkill]     = lfScore; // X360 stfsx +0xE9C ((skill+935)*4)

        // action = mpCurrentChallenge->GetAction(miCurrentChallengeAction) (X360 lwz +0xE0C,+0xE10)
        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction(miCurrentChallengeAction);

        if (lpAction->GetCoopType() ==
            BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION)  // X360 lbz action+1 == 2
        {
            mafBankedActionScores[leFreeburnSkill] += lfScore;  // X360 lfsx/fadds/stfsx +0x4E8 ((skill+314)*4)
        }
        else
        {
            mafBankedActionScores[leFreeburnSkill] = lfScore;   // X360 stfsx +0x4E8
        }
    }
}

// ----------------------------------------------------------------------------
// SetCurrentSkillScore -- X360 0x82324290. The per-frame entry point skill sources call to
// report a skill value. Range-asserts the skill id (non-fatal), then:
//   * If the manager is RUNNING and the current action's location is ok:
//       - bank-immediately  -> BankSkillScore(skill, score - cachedOnLocationEnter[skill])
//       - otherwise         -> flag active this frame + store the (score - cached) running value
//   * Otherwise (not RUNNING, or location not ok): if the skill is a CONTINUOUS-value skill,
//     re-cache the incoming score as the location-enter baseline.
// Finally, for CONTINUOUS-value skills, set this frame's "score was set" bit.
//
// The two "cache the score" arms below are the single shared X360 tail (LABEL_23) that both
// the non-RUNNING branch and the RUNNING-but-location-not-ok branch fall into.
// ----------------------------------------------------------------------------
void ChallengeManager::SetCurrentSkillScore(EFreeburnSkill leFreeburnSkill, f32 lfScore, bool lbBankImmediately)
{
    // Non-fatal range asserts (X360 cmpwi r29,0 / cmpwi 0x26; both continue on failure). The
    // upper bound is the X360-drift count (38) per KI_FREEBURN_SKILL_COUNT_X360; the verbatim
    // string names the enumerator E_FREEBURN_SKILL_COUNT.
    CGS_ASSERT(static_cast<s32>(leFreeburnSkill) >= 0, "leFreeburnSkill >= 0");
    CGS_ASSERT(static_cast<s32>(leFreeburnSkill) < KI_FREEBURN_SKILL_COUNT_X360,
               "leFreeburnSkill < E_FREEBURN_SKILL_COUNT");

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz +0xE08 == 2
    {
        CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");
        CGS_ASSERT(miCurrentChallengeAction < mpCurrentChallenge->GetNumActions(),
                   "miCurrentChallengeAction < mpCurrentChallenge->GetNumActions()");

        if (mabIsLocationOk[miCurrentChallengeAction])   // X360 lbz +0xFD8 (base + miCurrentChallengeAction)
        {
            if (lbBankImmediately)   // X360 clrlwi r27,24; bne
            {
                BankSkillScore(leFreeburnSkill,
                               lfScore - mafCachedActiveSkillValueOnLocationEnter[leFreeburnSkill]);  // X360 lfsx +0xF34; fsubs; bl BankSkillScore
            }
            else
            {
                mabActiveSkillThisFrame[leFreeburnSkill] = true;   // X360 stb 1,+0xE76
                mafActiveSkillValue[leFreeburnSkill] =
                    lfScore - mafCachedActiveSkillValueOnLocationEnter[leFreeburnSkill];  // X360 lfsx +0xF34; fsubs; stfsx +0xE9C
            }
        }
        else
        {
            if (KAB_FREEBURN_SKILL_IS_CONTINUOUS[leFreeburnSkill])   // X360 LABEL_23: lbzx byte_8202132C
            {
                mafCachedActiveSkillValueOnLocationEnter[leFreeburnSkill] = lfScore;  // X360 stfsx +0xF34
            }
        }
    }
    else
    {
        if (KAB_FREEBURN_SKILL_IS_CONTINUOUS[leFreeburnSkill])   // X360 LABEL_23 (non-RUNNING fall-through)
        {
            mafCachedActiveSkillValueOnLocationEnter[leFreeburnSkill] = lfScore;
        }
    }

    if (KAB_FREEBURN_SKILL_IS_CONTINUOUS[leFreeburnSkill])   // X360 2nd lbzx byte_8202132C
    {
        // X360 inlines FastBitArray<38>::SetBit here (with the "max bits: 38" range assert);
        // call the committed container method (spec pitfall 5).
        mScoresSetThisFrameBitArray.SetBit(static_cast<u32>(leFreeburnSkill));
    }
}

// ----------------------------------------------------------------------------
// UpdateLocationOKStatusChange -- X360 0x82323FF8. Called by UpdateAction on a location-ok
// transition. On an ok -> not-ok edge (was ok, now not ok), walk every skill and bank the
// ones that carry a non-zero running value AND opt into banking on location exit
// (KAB_FREEBURN_SKILL_BANK_ON_LOCATION_EXIT). Returns whether anything was banked. The
// per-skill loop uses the header's EFreeburnSkill operator++ (which carries the inlined
// "leEnumIndex <= E_FREEBURN_SKILL_COUNT" assert at BrnChallengeManager.h:113).
// ----------------------------------------------------------------------------
bool ChallengeManager::UpdateLocationOKStatusChange(bool lbWasLocationOk, bool lbIsLocationOk)
{
    bool lbBankedAny = false;   // X360 li r25,0

    if (!lbIsLocationOk && lbWasLocationOk)   // X360 clrlwi r5; bne skip / clrlwi r4; beq skip
    {
        for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START;
             static_cast<s32>(leSkill) < KI_FREEBURN_SKILL_COUNT_X360;
             leSkill++)
        {
            if (mafActiveSkillValue[leSkill] != 0.0f &&               // X360 lfs +0xE9C; fcmpu vs flt_82001CC0(0.0)
                KAB_FREEBURN_SKILL_BANK_ON_LOCATION_EXIT[leSkill])    // X360 lbzx byte_82021354
            {
                BankSkillScore(leSkill, mafActiveSkillValue[leSkill]); // X360 f1 == the just-loaded running value
                lbBankedAny = true;                                    // X360 li r25,1
            }
        }
    }

    return lbBankedAny;
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_01.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_01.cpp
// ============================================================================
// Wave-B partfile 01 of BrnGameState::ChallengeManager -- the skill-score READ/eval side.
// Bodies reconstructed store-for-store from the X360 ARTIST asm onto the keystone-frozen
// named layout in BrnChallengeManager.h:
//
//   GetScaleFactor           @ 0x8233B070  (all-one/all-two-player debug score scaling)
//   GetCurrentSkillScore     @ 0x8233B2A0  (per-coop-type skill-score read; FLT_MAX sentinel)
//   UpdateCurrentActionScore @ 0x82316968  (writes the maafCurrentActionsScores[arci][action] grid)
//
// The recovered rodata floats are used as their attested literal values (BURNOUT_X360_ARTIST.XEX,
// big-endian): flt_82001C98 == 1.0f, flt_82001DA0 == 0.5f, flt_820037C8 == -1.0f,
// flt_82020AFC == 3.4028235e38f (FLT_MAX, the "uncapped" sentinel).
//
// BLOCKED in this group (reported in funcs_blocked, NOT bodied here):
//   IsSkillScoreCurrentlySuccessful @ 0x823167A0 -- the X360 build drifted this to a
//   SEVEN-parameter function (score f1 shadowing r4; action r5; then r6/r7/r8/r9/r10), whereas
//   the keystone-frozen private declaration in BrnChallengeManager.h has only FIVE params
//   (f32, const ChallengeListEntryAction*, bool, s32, bool). The asm consumes two register
//   inputs (r9 = the maafCumulativeContributions grid ROW index; r10 = the top-level
//   target-comparison branch selector) that have NO parameter to carry in the frozen decl,
//   and it uses r7/r8 as a GetTargetValue index / grid COLUMN index rather than the decl's
//   liNumPlayers / lbIsOnline. Bodying it against the 5-param decl would drop or fabricate
//   that logic; the header cannot be edited, so the function is skipped.
// ============================================================================


namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // GetScaleFactor @ 0x8233B070
    // ------------------------------------------------------------------------
    // When the "make every challenge N-player" debug toggles are set, the freeburn scores
    // are scaled by the challenge's ORIGINAL player count (the pre-hack count backed up in the
    // high nibble of ChallengeListEntry byte +0xD3 == GetOriginalNumPlayers()): x1 per original
    // player when all-one-player, xHALF that when all-two-player. The GetChallengeIndex call is
    // kept for its (debug-print/assert) side effects; its result is discarded in the asm.
    // Default scale is 1.0f (flt_82001C98).
    f32 ChallengeManager::GetScaleFactor()
    {
        f32 lfScaleFactor = 1.0f;                                   // flt_82001C98

        if ( mbChallengesAreAllOnePlayer )                          // byte_82FAD9C1
        {
            GetChallengeIndex( mpCurrentChallenge->GetChallengeID() );
            return static_cast<f32>( mpCurrentChallenge->GetOriginalNumPlayers() );
        }
        else if ( mbChallengesAreAllTwoPlayer )                     // byte_82FAD9C0
        {
            GetChallengeIndex( mpCurrentChallenge->GetChallengeID() );
            return static_cast<f32>( mpCurrentChallenge->GetOriginalNumPlayers() ) * 0.5f; // flt_82001DA0
        }

        return lfScaleFactor;
    }

    // ------------------------------------------------------------------------
    // GetCurrentSkillScore @ 0x8233B2A0
    // ------------------------------------------------------------------------
    // Reads the freeburn skill's current running value for one challenge action. When the
    // player is currently at the action's location (lbIsLocationOk), the score is the action's
    // cumulative running total scaled by GetScaleFactor() and the frame step. Otherwise the
    // per-skill banked/active values are combined per the action's BANK_FOR_SUCCESS modifier and
    // its coop type; if the skill has no active/banked value this frame the "uncapped" sentinel
    // is returned (-1.0f online, FLT_MAX offline).
    f32 ChallengeManager::GetCurrentSkillScore( s32 liActionIndex, bool lbIsLocationOk,
                                                EFreeburnSkill leFreeburnSkill,
                                                BrnResource::ChallengeListEntryAction::EChallengeCoopType leCoopType,
                                                bool lbIsOnline, f32 lfTimeStep )
    {
        if ( lbIsLocationOk )
        {
            CGS_ASSERT( liActionIndex < mpCurrentChallenge->GetNumActions(),
                        "liActionIndex < mpCurrentChallenge->GetNumActions()" );

            const f32 lfScaleFactor = GetScaleFactor();
            return lfScaleFactor * mafCumulativeActionScores[ liActionIndex ] * lfTimeStep;
        }

        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction( liActionIndex );

        if ( ( lpAction->GetModifier() & BrnResource::ChallengeListEntryAction::KX_MODIFIER_BANK_FOR_SUCCESS )
                 == BrnResource::ChallengeListEntryAction::KX_MODIFIER_BANK_FOR_SUCCESS )
        {
            if ( mabBankedSkillThisFrame[ leFreeburnSkill ] )
            {
                return mafBankedActionScores[ leFreeburnSkill ] * lfTimeStep;
            }
        }
        else if ( mabActiveSkillThisFrame[ leFreeburnSkill ] )
        {
            if ( leCoopType == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION )
            {
                if ( mabBankedSkillThisFrame[ leFreeburnSkill ] )
                {
                    return mafBankedActionScores[ leFreeburnSkill ] * lfTimeStep;
                }
                return ( mafActiveSkillValue[ leFreeburnSkill ] + mafBankedActionScores[ leFreeburnSkill ] ) * lfTimeStep;
            }
            return mafActiveSkillValue[ leFreeburnSkill ] * lfTimeStep;
        }

        if ( lbIsOnline )
        {
            return -1.0f;              // flt_820037C8
        }
        return 3.4028235e38f;         // flt_82020AFC (FLT_MAX -- the uncapped sentinel)
    }

    // ------------------------------------------------------------------------
    // UpdateCurrentActionScore @ 0x82316968
    // ------------------------------------------------------------------------
    // Snapshots the freeburn skill's live value into the local player's per-action score grid
    // (maafCurrentActionsScores[arci][action]) scaled by lfScore. Does nothing while the player
    // is at the action's location (lbIsLocationOk) -- the grid only tracks off-location running
    // totals. The banked-vs-active selection mirrors GetCurrentSkillScore: BANK_FOR_SUCCESS
    // actions read the banked score; other actions read the active value (plus banked, for
    // INDIVIDUAL_ACCUMULATION coop).
    void ChallengeManager::UpdateCurrentActionScore( s32 liActionIndex, bool lbIsLocationOk,
                                                     EFreeburnSkill leFreeburnSkill,
                                                     EActiveRaceCarIndex leActiveRaceCarIndex,
                                                     f32 lfScore )
    {
        CGS_ASSERT( ( leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID )
                        && ( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ),
                    "( leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )" );

        if ( lbIsLocationOk )
        {
            return;
        }

        CGS_ASSERT( mpCurrentChallenge, "mpCurrentChallenge" );
        CGS_ASSERT( liActionIndex < mpCurrentChallenge->GetNumActions(),
                    "liActionIndex < mpCurrentChallenge->GetNumActions()" );

        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction( liActionIndex );

        f32 lfValue;
        if ( ( lpAction->GetModifier() & BrnResource::ChallengeListEntryAction::KX_MODIFIER_BANK_FOR_SUCCESS )
                 == BrnResource::ChallengeListEntryAction::KX_MODIFIER_BANK_FOR_SUCCESS )
        {
            if ( !mabBankedSkillThisFrame[ leFreeburnSkill ] )
            {
                return;
            }
            lfValue = mafBankedActionScores[ leFreeburnSkill ];
        }
        else
        {
            if ( !mabActiveSkillThisFrame[ leFreeburnSkill ] )
            {
                return;
            }

            lpAction = mpCurrentChallenge->GetAction( liActionIndex );
            if ( lpAction->GetCoopType() == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION )
            {
                if ( mabBankedSkillThisFrame[ leFreeburnSkill ] )
                {
                    lfValue = mafBankedActionScores[ leFreeburnSkill ];
                }
                else
                {
                    lfValue = mafActiveSkillValue[ leFreeburnSkill ] + mafBankedActionScores[ leFreeburnSkill ];
                }
            }
            else
            {
                lfValue = mafActiveSkillValue[ leFreeburnSkill ];
            }
        }

        maafCurrentActionsScores[ leActiveRaceCarIndex ][ liActionIndex ] = lfValue * lfScore;
    }
}

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_02.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_02.cpp
// ============================================================================
// Wave-B partfile 02 of BrnGameState::ChallengeManager -- the per-frame / per-action
// clear helpers. Bodies reconstructed store-for-store from the X360 ARTIST asm onto the
// keystone-frozen named layout in BrnChallengeManager.h:
//
//   ClearFreeburnSkillsThisFrame  @ 0x82323498  (per-skill scratch clear)
//   ClearPlayerSuccessData        @ 0x82323158  (per-player success/contribution clear)
//   ResetCurrentChallengeData     @ 0x823244F8  (per-action scratch reset)
//
// The inlined FastBitArray<38> range-assert StrStream machinery around the skill loops is
// NOT reproduced -- the committed container methods (IsBitSet / UnSetAll) carry it (spec
// pitfall 5). The per-skill loops drive the frozen EFreeburnSkill operator++ (which carries
// the verbatim "leEnumIndex <= E_FREEBURN_SKILL_COUNT" assert vs 38); the ResetCurrentChallengeData
// player loop drives the committed EActiveRaceCarIndex operator++ (its own count assert).
// ============================================================================


namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // ClearFreeburnSkillsThisFrame @ 0x82323498
    // ------------------------------------------------------------------------
    // Called once a frame (Construct / UpdateFreeburnSkillsThisFrame): drop every skill's
    // per-frame banked/active flags and running value, and drop the cached location-enter
    // value for any skill whose score was NOT set this frame, then reset the whole
    // "scores set this frame" bit array.
    void ChallengeManager::ClearFreeburnSkillsThisFrame()
    {
        for (EFreeburnSkill le = E_FREEBURN_SKILL_START; (s32)le < KI_FREEBURN_SKILL_COUNT_X360; le++)
        {
            mabActiveSkillThisFrame[le] = 0;
            mabBankedSkillThisFrame[le] = 0;
            mafActiveSkillValue[le]     = 0.0f;

            if (!mScoresSetThisFrameBitArray.IsBitSet(le))
                mafCachedActiveSkillValueOnLocationEnter[le] = 0.0f;
        }

        mScoresSetThisFrameBitArray.UnSetAll();
    }

    // ------------------------------------------------------------------------
    // ClearPlayerSuccessData @ 0x82323158
    // ------------------------------------------------------------------------
    // Wipe every per-player / per-action success-tracking scratch field back to zero
    // (challenge begin/end, results, remote-end). The full mafBankedActionScores[38] sweep
    // is emitted redundantly inside the per-action loop by the X360 build -- kept verbatim.
    void ChallengeManager::ClearPlayerSuccessData()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "CLEARING CUMULATIVE CONTRIBUTIONS\n";

        for (s32 liPlayer = 0; liPlayer < KI_MAX_CHALLENGE_PLAYERS; liPlayer++)
        {
            maPlayerSuccessUpdateArray[liPlayer].UnSetAll();
            mabReceivedSuccessUpdates[liPlayer]     = 0;
            mabPlayerStartedChallenge[liPlayer]     = 0;
            maiCrashedWithChallengePlayer[liPlayer] = 0;

            for (s32 liAction = 0; liAction < KI_MAX_CHALLENGE_ACTIONS; liAction++)
            {
                maafCurrentActionsScores[liPlayer][liAction]   = 0.0f;
                maaePlayersSuccessStatus[liPlayer][liAction]   = GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
                maafCumulativeContributions[liPlayer][liAction] = 0.0f;
                maiRemainingTarget[liAction]                   = 0;
                mafCumulativeActionScores[liAction]            = 0.0f;
                mabIsLocationOk[liAction]                      = 0;
                mabIndividualActionsSuccessUpdateSent[liAction] = 0;

                for (EFreeburnSkill le = E_FREEBURN_SKILL_START; (s32)le < KI_FREEBURN_SKILL_COUNT_X360; le++)
                    mafBankedActionScores[le] = 0.0f;
            }
        }
    }

    // ------------------------------------------------------------------------
    // ResetCurrentChallengeData @ 0x823244F8
    // ------------------------------------------------------------------------
    // Reset the per-action scratch for the current challenge from liActionIndex onward:
    // re-seed each remaining action's target from its ChallengeListEntryAction target value,
    // clear the "individual success update sent" flag, then wipe the per-player cumulative
    // grids (and the redundant banked-action-score sweep) and clear the billboard tally.
    void ChallengeManager::ResetCurrentChallengeData(s32 liActionIndex)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "CLEARING CUMULATIVE CONTRIBUTIONS\n";

        for (s32 liAction = liActionIndex; liAction < mpCurrentChallenge->GetNumActions(); liAction++)
        {
            // (The liActionIndex range asserts the X360 emits here are the ones INLINED from the
            // committed const ChallengeListEntry::GetAction below -- do not duplicate them.)
            const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liAction);
            maiRemainingTarget[liAction]                    = lpAction->GetTargetValue(0);
            mabIndividualActionsSuccessUpdateSent[liAction] = 0;
        }

        for (EActiveRaceCarIndex le = E_ACTIVE_RACE_CAR_INDEX_0; (s32)le < E_ACTIVE_RACE_CAR_INDEX_COUNT; le++)
        {
            for (s32 liAction = liActionIndex; liAction < mpCurrentChallenge->GetNumActions(); liAction++)
            {
                maafCumulativeContributions[le][liAction] = 0.0f;
                maaePlayersSuccessStatus[le][liAction]    = GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;

                for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START; (s32)leSkill < KI_FREEBURN_SKILL_COUNT_X360; leSkill++)
                    mafBankedActionScores[leSkill] = 0.0f;
            }
        }

        maBillboardsCollected.Clear();
    }
}

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_03.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_03.cpp
// ============================================================================
// Wave-B partfile 03 -- lifecycle + list. Store-for-store reconstructions of three X360
// ChallengeManager methods, bodied onto the keystone-frozen named layout:
//   * Destruct              (X360 0x8233A9C0) -- mirror of Construct minus the pool/asserts.
//   * Prepare               (X360 0x8233AAF8) -- cache the list, histogram per player count,
//                                                refill the leaping-car pool.
//   * UnHackAllChallenges   (X360 0x82333040) -- restore each entry's backed-up player count.
//
// The manager's own state is reached exclusively through named members. All offsets quoted in
// comments are X360-asm-attested (see the frozen header BrnChallengeManager.h).
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// Destruct -- X360 0x8233A9C0. The tear-down mirror of Construct with the pool refill and the
// back-pointer asserts dropped: scalar/timer/flag resets, the pointers nulled,
// miLastChallengeResetFrame back to -1, the local completion bit store + last-second success
// mask cleared, the billboard list + leaping-car pool re-constructed, ClearPlayerSuccessData,
// the per-slot completion records freed (-1 id / not finalised -- the bit stores are NOT
// touched here, unlike Construct), the cached-skill sweep, the scores-set mask cleared, and
// finally the embedded debug component's back-pointer nulled + the component torn down.
//
// PITFALL 1: the pseudocode's `= 0xFFFFFFFF00000000uLL` stores are ZERO stores (`li r30,0`
// then `std r30`) -- container UnSetAll()/Construct() clears, not sentinel writes.
// PITFALL 2: the pseudocode tail `BaseCollisionGenerator::Destruct(this+0x1004)` is ICF for
// CgsDev::DebugComponent::Destruct on the embedded component.
// ----------------------------------------------------------------------------
void ChallengeManager::Destruct()
{
    mfLeapCarsValidTimer = -1.0f;                 // X360 stfs -1.0,+0x110
    mbUpdateLeaptCars = false;                    // X360 stb 0,+0x118
    mbChallengeTimerRunning = false;              // X360 stb 0,+0xE1C
    mbConvoyTimerRunning    = false;              // X360 stb 0,+0xE24
    mbResultsTimerRunning   = false;              // X360 stb 0,+0xE2C
    mfChallengeTimer = 0.0f;                      // X360 stfs 0.0,+0xE18
    mbRemoteStartPending = false;                 // X360 stb 0,+0xE35
    mfConvoyTimer = 0.0f;                         // X360 stfs 0.0,+0xE20
    mbRemoteTriggerPending = false;               // X360 stb 0,+0xE36
    mfResultsTimer = 0.0f;                        // X360 stfs 0.0,+0xE28
    mbRemoteEndPending = false;                   // X360 stb 0,+0xE37
    mbChallengeRequiresLeapCars = false;          // X360 stb 0,+0xE34
    mpCurrentChallenge = 0;                       // X360 stw 0,+0xE0C
    miCurrentChallengeAction = 0;                 // X360 stw 0,+0xE10
    miCurrentArbitrationIndex = 0;                // X360 stw 0,+0xE14
    miLastChallengeResetFrame = -1;               // X360 stw -1,+0x1000
    mpProgression = 0;                            // X360 stw 0,+0xE44
    mpGameStateModule = 0;                        // X360 stw 0,+0xE48
    mpModeManager = 0;                            // X360 stw 0,+0xE4C
    miNumCarsLeapt = 0;                           // X360 stw 0,+0x114

    mLocalChallengeCompletionData.UnSetAll();     // X360 loop base +0xD00: 32 std of 0 (PITFALL 1)
    maBillboardsCollected.Construct();            // X360 stw 0,+0x3A0 (miCount = 0)
    mLastSecondSuccessStatus.UnSetAll();          // X360 std 0,+0xE00 (PITFALL 1)
    mPotentiallyLeaptCars.Construct();            // X360 std 0,+0x100 (occupancy only; PITFALL 1)

    ClearPlayerSuccessData();

    // Per remote-player completion record: free the slot (-1) + clear the finalised flag. The
    // bit store is deliberately left as-is here (X360 loop base +0x6C8, stride 0x108:
    // stw -1 @+0x100, stb 0 @+0x104 -- no queue clear).
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        maChallengeCompletionData[liSlot].mNetworkPlayerID = -1;
        maChallengeCompletionData[liSlot].mbFinalised      = false;
    }

    // Cached per-skill location-enter values (X360 loop base +0xF34, 38 stfs; the post-increment
    // operator carries the "leEnumIndex <= E_FREEBURN_SKILL_COUNT" assert, BrnChallengeManager.h:113).
    for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START;
         static_cast<s32>(leSkill) < KI_FREEBURN_SKILL_COUNT_X360;
         leSkill++)
    {
        mafCachedActiveSkillValueOnLocationEnter[leSkill] = 0.0f;
    }

    mScoresSetThisFrameBitArray.UnSetAll();       // X360 std 0,+0xFD0 (PITFALL 1)

    // Embedded debug component tear-down (X360 stw 0,+0x1010 then the ICF'd DebugComponent::Destruct).
    mChallengeManagerDebugComponent.mpChallengeManager = 0;   // X360 stw 0,+0x1010
    mChallengeManagerDebugComponent.Destruct();               // X360 ICF tail (PITFALL 2)
}

// ----------------------------------------------------------------------------
// Prepare -- X360 0x8233AAF8. Caches the freeburn challenge list, rebuilds the per-player-count
// histogram (one bucket per challenge, keyed by the entry's num-players), and refills the
// potentially-leapt-cars pool. Returns true.
// ----------------------------------------------------------------------------
bool ChallengeManager::Prepare(const BrnResource::ChallengeList* lpFreeburnChallengeList)
{
    CGS_ASSERT(lpFreeburnChallengeList != 0, "lpFreeburnChallengeList");
    mpFreeburnChallengeList = lpFreeburnChallengeList;   // X360 stw,+0xE38

    for (s32 liPlayerCount = 0; liPlayerCount < KI_MAX_CHALLENGE_PLAYERS; ++liPlayerCount)
    {
        maChallengePlayerCounts[liPlayerCount] = 0;      // X360 loop base +0xFDC, 8 stw
    }

    for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
    {
        const BrnResource::ChallengeListEntry* lpChallenge = mpFreeburnChallengeList->GetChallengeData(liChallenge);
        CGS_ASSERT(lpChallenge != NULL, "lpChallengeListEntry != NULL");
        // X360: index == ((byte0xD3 & 0xF) + 0x3F6) word -> maChallengePlayerCounts[GetNumPlayers()-1].
        ++maChallengePlayerCounts[lpChallenge->GetNumPlayers() - 1];
    }

    mPotentiallyLeaptCars.Clear();   // X360 free-queue refill @+0xE0..+0x100

    // ==============================================================================================
    // [DIAG] NOT IN THE X360 BINARY -- the `[fburn]` witness, second rung (2026-09-07). Gated on
    // BRN_MODEMGR_DIAG, the same env as the Construct rung in BrnChallengeManager.cpp.
    // ==============================================================================================
    // WHY IT EARNS ITS PLACE. Construct proves the object exists; only Prepare proves it has DATA.
    // The challenge count it prints is the single number that says whether the freeburn challenge
    // list actually loaded -- a zero there and a missing line are two very different bugs, and the
    // Construct rung alone cannot tell them apart. One line per Prepare, i.e. once per level load.
    // ⛔ NO SIDE EFFECTS: re-reads the already-cached list pointer and the histogram just built.
    // DELETE-WHEN the freeburn bring-up is done.
    {
        static const bool sbFburnDiag = (getenv("BRN_MODEMGR_DIAG") != 0);
        if (sbFburnDiag && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[fburn] ChallengeManager::Prepare DONE challenges "
                << mpFreeburnChallengeList->GetChallengeCount()
                << " onePlayer " << maChallengePlayerCounts[0]
                << " twoPlayer " << maChallengePlayerCounts[1] << "\n";
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// UnHackAllChallenges -- X360 0x82333040. Undoes HackAllChallenges: for every challenge in the
// list, restore the active num-players (byte +0xD3 low nibble) from the backup HackAllChallenges
// stashed in the high nibble. The range asserts ("liNumPlayers >= 1" / "<= KI_MAX_PLAYERS",
// ChallengeListEntry.h:888/890) live inside the entry's GetOriginalNumPlayers accessor.
// ----------------------------------------------------------------------------
void ChallengeManager::UnHackAllChallenges()
{
    for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
    {
        BrnResource::ChallengeListEntry* lpChallenge =
            const_cast<BrnResource::ChallengeListEntry*>(mpFreeburnChallengeList->GetChallengeData(liChallenge));
        // X360: newByte = (byte0xD3 & 0xF0) | (byte0xD3 >> 4) -> SetNumPlayers(GetOriginalNumPlayers()).
        lpChallenge->SetNumPlayers(lpChallenge->GetOriginalNumPlayers());
    }
}

}   // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_04.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_04.cpp
// ============================================================================
// Wave-B partfile (group 4) for BrnGameState::ChallengeManager. Challenge-list + profile
// walkers, store-for-store from the X360 ARTIST asm over the keystone's named members:
//   GetChallengeIndex               @ 0x82333238
//   CheckForOnlineChallengeUnlocks  @ 0x82333100
//   OnProfileLoaded                 @ 0x82334B00
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// GetChallengeIndex -- X360 0x82333238. Forwards to the cached freeburn challenge list's
// index lookup after asserting the list is present; on a missing id the X360 builds a
// "Failed to find index for challenge <id> from list of <count> challenges" assert message
// via StrStream and fires it (the message machinery is the assert front-end -- represented
// here by the failure-condition CGS_ASSERT, exactly as the committed foundation bodies do).
// ----------------------------------------------------------------------------
s32 ChallengeManager::GetChallengeIndex(CgsID lChallengeID) const
{
    CGS_ASSERT(mpFreeburnChallengeList != 0, "mpFreeburnChallengeList");   // X360 asserts +0xE38 non-null (line 2566)

    const s32 liChallengeIndex = mpFreeburnChallengeList->GetChallengeIndex(lChallengeID);
    CGS_ASSERT(liChallengeIndex >= 0, "liChallengeIndex >= 0");            // X360 asserts index found (line 2570)

    return liChallengeIndex;
}

// ----------------------------------------------------------------------------
// GetChallengeFromID. The list entry for an id. A missing id leaves the result
// NULL and fires the streamed "Could not find challenge <id> from list of <count> challenges"
// assert; so does an index whose entry is NULL. Callers: the remote start / trigger handlers.
// ----------------------------------------------------------------------------
const BrnResource::ChallengeListEntry* ChallengeManager::GetChallengeFromID(CgsID lChallengeID) const
{
    CGS_ASSERT(mpFreeburnChallengeList != 0, "mpFreeburnChallengeList");

    const BrnResource::ChallengeListEntry* lpChallenge = 0;
    const s32 liChallengeIndex = mpFreeburnChallengeList->GetChallengeIndex(lChallengeID);
    if (liChallengeIndex >= 0)
    {
        lpChallenge = mpFreeburnChallengeList->GetChallengeData(liChallengeIndex);
    }

    if (lpChallenge == 0)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Could not find challenge " << static_cast<u64>(lChallengeID)
                   << " from list of " << static_cast<s32>(mpFreeburnChallengeList->GetChallengeCount())
                   << " challenges";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    return lpChallenge;
}

// ----------------------------------------------------------------------------
// RemoteBeginChallenge / RemoteTriggerFreeburnChallenge. No out-of-line copy
// on the console: both are inlined into ModeManager's remote start / trigger handlers, which is
// where their bodies are read from (the pending flag at +0xE35 / +0xE36, then the current
// challenge and action index). UpdateRemoteRequests consumes the flags on the next tick.
// ----------------------------------------------------------------------------
void ChallengeManager::RemoteBeginChallenge(CgsID lChallengeID)
{
    mbRemoteStartPending     = true;
    mpCurrentChallenge       = GetChallengeFromID(lChallengeID);
    miCurrentChallengeAction = 0;
}

void ChallengeManager::RemoteTriggerFreeburnChallenge(CgsID lChallengeID)
{
    mbRemoteTriggerPending   = true;
    mpCurrentChallenge       = GetChallengeFromID(lChallengeID);
    miCurrentChallengeAction = 0;
}

// ----------------------------------------------------------------------------
// GetCurrentFreeburnChallengeID. Inlined into ModeManager's forwarder: 0 while no
// list is loaded or no challenge is current, else the current entry's id.
// ----------------------------------------------------------------------------
CgsID ChallengeManager::GetCurrentFreeburnChallengeID()
{
    if (mpFreeburnChallengeList == 0)
    {
        return 0;
    }
    if (mpCurrentChallenge == 0)
    {
        return 0;
    }
    return mpCurrentChallenge->GetChallengeID();
}

// ----------------------------------------------------------------------------
// CheckForOnlineChallengeUnlocks -- X360 0x82333100. Tally, by challenge player-count, how many
// challenges the local player has completed (mpProgression->GetProfile()->HasPlayerCompleted...),
// then for the multi-player player-count categories 1..7 count how many are fully cleared
// (completed >= the challenge-list histogram maChallengePlayerCounts). Once two categories are
// fully cleared, unlock trophy 10 and notify the achievement manager (fires for every qualifying
// category from the second onwards -- the X360 shape kept verbatim).
// ----------------------------------------------------------------------------
void ChallengeManager::CheckForOnlineChallengeUnlocks()
{
    // X360 zeroes an 8-entry stack tally (init loop of 8 word stores).
    s32 laiCompletedPerPlayerCount[KI_MAX_CHALLENGE_PLAYERS];
    for (s32 liInit = 0; liInit < KI_MAX_CHALLENGE_PLAYERS; ++liInit)
    {
        laiCompletedPerPlayerCount[liInit] = 0;
    }

    for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
    {
        const BrnResource::ChallengeListEntry* lpChallengeListEntry =
            mpFreeburnChallengeList->GetChallengeData(liChallenge);
        CGS_ASSERT(lpChallengeListEntry != 0, "lpChallengeListEntry != NULL");   // X360 line 2392

        if (mpProgression->GetProfile()->HasPlayerCompletedFreeburnChallenge(lpChallengeListEntry->GetChallengeID()))
        {
            ++laiCompletedPerPlayerCount[lpChallengeListEntry->GetNumPlayers() - 1];   // X360 tally[nibble-1]++
        }
    }

    s32 liNumFullyCompletedCategories = 0;
    for (s32 liPlayerCount = 1; liPlayerCount < KI_MAX_CHALLENGE_PLAYERS; ++liPlayerCount)
    {
        if (laiCompletedPerPlayerCount[liPlayerCount] >= maChallengePlayerCounts[liPlayerCount] &&
            ++liNumFullyCompletedCategories >= 2)
        {
            mpProgression->OnTrophyUnlock(10);                                             // X360 li r4,0xA
            mpProgression->GetAchievementManager()->OnFreeburnChallengeBlockComplete();    // X360 lwzx progression+0x20938
        }
    }
}

// ----------------------------------------------------------------------------
// OnProfileLoaded -- X360 0x82334B00. Rebuilds the local completion bit array from the freshly
// loaded profile: for every challenge the profile records as completed, set that challenge's bit
// in mLocalChallengeCompletionData (the inlined FastBitArray<2000> range assert is folded into
// the committed SetBit).
// ----------------------------------------------------------------------------
void ChallengeManager::OnProfileLoaded()
{
    CGS_ASSERT(mpProgression != 0, "mpProgression");                    // X360 line 4895
    CGS_ASSERT(mpFreeburnChallengeList != 0, "mpFreeburnChallengeList"); // X360 line 4896

    for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
    {
        const BrnResource::ChallengeListEntry* lpChallengeListEntry =
            mpFreeburnChallengeList->GetChallengeData(liChallenge);
        if (mpProgression->GetProfile()->HasPlayerCompletedFreeburnChallenge(lpChallengeListEntry->GetChallengeID()))
        {
            mLocalChallengeCompletionData.SetBit(liChallenge);
        }
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_05.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_05.cpp
// ============================================================================
// Wave-B partfile (group 5) for BrnGameState::ChallengeManager. Player-car action gates,
// store-for-store from the X360 ARTIST asm over the keystone's named members + committed
// interface accessors:
//   CheckForModifiers  @ 0x823166C0
//   UpdateBurnouts     @ 0x823345B0
//
// BLOCKED (reported to the conductor): CheckCurrentCar @ 0x823336E8 -- its car-class gate
// reads a boost/restriction-class byte at BrnResource::VehicleListEntry + 0xE8
// (`lbz r11,0xE8(r31)`), which the committed VehicleListEntry.h models only as unnamed pad
// (maPad224[16], +0xE0..0xEF). No named accessor exists for that byte and this partfile may
// not edit the header, so the function cannot be bodied faithfully here (see funcs_blocked).
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// CheckForModifiers -- X360 0x823166C0. Gate an action's success on its modifier flags: with
// no modifier byte the action is unconditionally allowed; otherwise (currently only the IN_AIR
// modifier is decoded) the player must be off the gas -- the X360 returns whether the player
// race-car state's mfGas is non-zero. `this` is unused (the check is over the passed action +
// interface only), exactly as the X360 body.
// ----------------------------------------------------------------------------
bool ChallengeManager::CheckForModifiers(
    const BrnResource::ChallengeListEntryAction*                                 lpAction,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface)
{
    CGS_ASSERT(lpAction, "lpAction");                                        // X360 line 2818
    CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface"); // X360 line 2819

    bool lbResult;
    if (lpAction->GetModifier())                                             // X360 lbz +2 (mxModifier) non-zero
    {
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
            lpActiveRaceCarOutputInterface->GetPlayerRaceCarState();         // X360 sub_82310240 (ICF)
        CGS_ASSERT(lpRaceCarState, "lpRaceCarState");                        // X360 line 2832

        lbResult = true;
        if ((lpAction->GetModifier() & BrnResource::ChallengeListEntryAction::KX_MODIFIER_IN_AIR) ==
            BrnResource::ChallengeListEntryAction::KX_MODIFIER_IN_AIR)       // X360 rlwinm 0,28,28 (& 8) == 8
        {
            lbResult = (lpRaceCarState->mfGas != 0.0f);                      // X360 lfs +0x404 (mfGas) != 0.0
        }
    }
    else
    {
        lbResult = true;
    }
    return lbResult;
}

// ----------------------------------------------------------------------------
// UpdateBurnouts -- X360 0x823345B0. Feed the player's boost telemetry into the freeburn skill
// scorer: with a live boost chain, score the chain length under the BURNOUTS skill; with the
// current boosting time positive, score it under the BOOST_TIME skill. The two boost-active /
// boost-inactive branches carry IDENTICAL bodies in the X360 asm (only the SetCurrentSkillScore
// bank-immediately flag differs: false while boosting, true when not) -- the duplicated shape is
// kept verbatim (spec pitfall 9).
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateBurnouts(
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface)
{
    CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface"); // X360 line 4104

    const EActiveRaceCarIndex lePlayerRaceCarIndex =
        lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex();      // inlined +10328 read + "index set" assert (h:980)
    CGS_ASSERT(lePlayerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lePlayerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");        // X360 line 4108
    CGS_ASSERT(lePlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lePlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");     // X360 line 4109

    const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* lpBoostInfo =
        lpActiveRaceCarOutputInterface->GetBoostOutputInfoN(lePlayerRaceCarIndex);
    CGS_ASSERT(lpBoostInfo, "lpBoostInfo");                                 // X360 line 4111

    if (lpBoostInfo->mbIsBoosting)                                          // X360 lbz +0 (mbIsBoosting)
    {
        if (lpBoostInfo->muNumChained)                                      // X360 lwz +0xC (muNumChained)
        {
            SetCurrentSkillScore(E_FREEBURN_SKILL_BURNOUTS,
                                 static_cast<f32>(lpBoostInfo->muNumChained), false);
        }
    }
    if (!lpBoostInfo->mbIsBoosting)                                         // duplicated branch (spec pitfall 9)
    {
        if (lpBoostInfo->muNumChained)
        {
            SetCurrentSkillScore(E_FREEBURN_SKILL_BURNOUTS,
                                 static_cast<f32>(lpBoostInfo->muNumChained), true);
        }
    }

    if (lpBoostInfo->mfCurrentBoostingTime > 0.0f)                          // X360 lfs +0x18 (mfCurrentBoostingTime) > 0.0
    {
        SetCurrentSkillScore(E_FREEBURN_SKILL_BOOST_TIME, lpBoostInfo->mfCurrentBoostingTime, false);
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_06.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_06.cpp
// ============================================================================
// Wave-B partfile (group 6) for BrnGameState::ChallengeManager. Store-for-store
// reconstructions of the challenge-timer arming/countdown and the per-frame freeburn-skill
// driver, over the keystone-frozen NAMED members (see BrnChallengeManager.h).
//
// BODIED HERE:
//   UpdateTimer                    -- X360 0x823232B0
//   UpdateFreeburnSkillsThisFrame  -- X360 0x8233AFF8
//
// NOT bodied (reported as blocked): UpdateResults @0x82345FD0 -- posts the 32-byte
//   E_ACTION_FREEBURN_CHALLENGE (id 153) action, but no struct for that action is declared
//   in any frozen header (only the 155 / 158 / 159 actions exist in BrnGameActions.h). See
//   the wave report for the exact field layout the keystone needs to add.
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// UpdateTimer -- X360 0x823232B0. Arms the per-action challenge countdown on first call
// from the current action's time limit (only when that limit is > 0), then counts it down
// one frame with the fsel clamp-at-zero idiom. Returns true while the timer is still running
// (or was never armed), and false only once an armed timer has expired -- UpdateRunning uses
// the false result as the "challenge action timed out" signal it feeds to UpdateArbitration.
// ----------------------------------------------------------------------------
bool ChallengeManager::UpdateTimer(f32 lfTimeStep)
{
    if (!mbChallengeTimerRunning)
    {
        CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");
        CGS_ASSERT(miCurrentChallengeAction < mpCurrentChallenge->GetNumActions(),
                   "miCurrentChallengeAction < mpCurrentChallenge->GetNumActions()");

        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction(miCurrentChallengeAction);
        CGS_ASSERT(lpAction != 0, "lpAction");

        // X360 lfs f0,+0x40(action) compared to 0.0, then re-reads it via GetTimeLimit for the
        // store (both are the action's mfTimeLimit @+0x40).
        if (lpAction->HasTimeLimit())
        {
            mfChallengeTimer        = lpAction->GetTimeLimit();
            mbChallengeTimerRunning = true;
        }
    }

    const bool lbTimerRunning = mbChallengeTimerRunning;

    if (lbTimerRunning && mfChallengeTimer > 0.0f)
    {
        // f0 = timer - dt; fsel(-(timer-dt), 0.0, f0) clamps the countdown at 0.0
        // (X360 fsubs / fneg / fsel f0,f13,f31,f0).
        const f32 lfNewTimer = mfChallengeTimer - lfTimeStep;
        mfChallengeTimer = (lfNewTimer >= 0.0f) ? lfNewTimer : 0.0f;
    }

    // X360 tail (cntlzw/extrwi == "== 0"): expired iff the timer is running AND has reached 0.0.
    if (lbTimerRunning && mfChallengeTimer <= 0.0f)
    {
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// UpdateFreeburnSkillsThisFrame -- X360 0x8233AFF8. Per-frame freeburn-skill driver: always
// clears this frame's skill scratch, then (only while the manager is RUNNING) folds in the
// burnout skill scores and, when the active challenge tracks leapt cars, the leap-car skills.
// (UpdateLeaptCars is the VMX128-blocked geometry callee -- declared in the header, called here.)
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateFreeburnSkillsThisFrame(
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
        f32 lfTimeStep)
{
    ClearFreeburnSkillsThisFrame();

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz +0xE08 == 2
    {
        UpdateBurnouts(lpActiveRaceCarOutputInterface);

        if (mbChallengeRequiresLeapCars)   // X360 lbz +0xE34
        {
            UpdateLeaptCars(lpActiveRaceCarOutputInterface, lfTimeStep);
        }
    }
}

}  // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_07.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_07.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the challenge CANCEL path.
// Store-for-store reconstruction of one X360 ARTIST method over the keystone's NAMED
// members (see BrnChallengeManager.h for the layout evidence):
//
//   CancelFreeburnChallenge @ 0x823506E8  (DWARF :162)
//
// NOTE (group 7, funcs blocked): the other two functions of this group --
//   BeginChallenge          @ 0x823505B8
//   TriggerFreeburnChallenge @ 0x82346DA0
// both post the id-153 (E_ACTION_FREEBURN_CHALLENGE, 0x20-byte) begin/trigger action,
// whose dedicated payload struct was NOT frozen into BrnGameActions.h (only the id-155/158/
// 159 challenge actions were). They cannot be bodied store-for-store without inventing that
// type (out of scope: no header edits, no forked/local action structs). Reported in
// funcs_blocked with the exact missing struct + attested layout. CancelFreeburnChallenge
// only ever posts the empty id-154 end-not-active action, so it IS bodiable here.
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// CancelFreeburnChallenge -- X360 0x823506E8. Abort the in-flight freeburn challenge (local
// or, when nothing is running yet, notify the network that the currently-selected challenge
// is no longer active).
//
//   * If the manager is in any non-NONE state, tear the challenge down via EndChallenge:
//       - RUNNING or RESULTS -> E_CHALLENGE_STATUS_ABORTED
//       - otherwise (PENDING) -> E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING
//     (both with lbIsOnline == false, matching the X360 `li r6,0`).
//   * If the manager is NONE but a challenge is still selected (mpCurrentChallenge != NULL),
//     post the empty id-154 end-not-active action and clear the per-player success scratch.
//     The X360 posts an uninitialised 1-byte stack buffer (the action carries no payload) --
//     reproduced with the empty GameAction<E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE> tag.
// ----------------------------------------------------------------------------
void ChallengeManager::CancelFreeburnChallenge(TGameActionQueue* lpActionQueue)
{
    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)   // X360 lwz +0xE08; cmpwi 0; bne
    {
        // X360 computes (status == 2 || status == 3) into a bool, then branches on == 1.
        if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING ||   // X360 cmpwi 2
            meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RESULTS)     // X360 cmpwi 3
        {
            EndChallenge(E_CHALLENGE_STATUS_ABORTED, lpActionQueue, false);              // X360 li r4,2
        }
        else
        {
            EndChallenge(E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING, lpActionQueue, false); // X360 li r4,4
        }
    }
    else if (mpCurrentChallenge != 0)   // X360 loc_82350758: lwz +0xE0C; cmplwi 0; beq -> return
    {
        CGS_ASSERT(lpActionQueue != 0, "lpOutput");   // X360 cmplwi r30,0; assert @ ...2213

        // Empty end-not-active action (no payload): X360 posts &stack, size 1, id 154.
        GameStateModuleIO::GameAction<GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE>
            lEndNotActiveAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEndNotActiveAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE, 1);  // X360 li r5,0x9A; li r6,1

        ClearPlayerSuccessData();   // X360 bl ClearPlayerSuccessData
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_08.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_08.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the challenge-END spine.
// Store-for-store reconstructions of the X360 ARTIST methods over the keystone's NAMED
// members (see BrnChallengeManager.h for the layout evidence):
//
//   RemoteEndChallenge @ 0x82347090  (DWARF :185)  -- remote flavour of the end tail
//   Disconnected       @ 0x8234E548  (DWARF :265)  -- local-player-disconnect teardown
//
// NOTE (group 8, func blocked):
//   EndChallenge @ 0x8234DE30 (DWARF :180) -- the local challenge-completion spine -- posts
//   the id-153 (E_ACTION_FREEBURN_CHALLENGE, 0x20-byte) challenge action (X360 li r5,0x99;
//   li r6,0x20; AddEvent). That action's dedicated payload struct was NOT frozen into
//   BrnGameActions.h (only the id-155 update / id-158 success-update / id-159 success actions
//   were). It cannot be bodied store-for-store without inventing that type (out of scope: no
//   header edits, no forked/local action structs -- the same call the sibling group-7 partfile
//   made for BeginChallenge/TriggerFreeburnChallenge). Reported in funcs_blocked with the exact
//   missing struct + attested layout.
//
//   Disconnected only CALLS EndChallenge (declared in the frozen header) and posts nothing
//   itself; RemoteEndChallenge only ever posts the empty id-154 end-not-active action. Both are
//   therefore bodiable here.
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// RemoteEndChallenge -- X360 0x82347090. Apply a remote player's challenge-end notification.
//
//   * The status must not be E_CHALLENGE_STATUS_ONGOING (non-fatal assert @2494).
//   * If a local challenge is still live (manager not NONE) the remote end cannot be applied
//     yet: latch the incoming status + set mbRemoteEndPending so UpdateRemoteRequests replays
//     it once the local challenge terminates, then return.
//   * Otherwise (manager NONE): on a SUCCESS end, mark the just-completed challenge complete
//     for every claimed remote slot (SetRemotePlayersChallengeCompleted by list index), then --
//     regardless of the end status -- post the empty id-154 end-not-active action and clear the
//     per-player success scratch.
// ----------------------------------------------------------------------------
void ChallengeManager::RemoteEndChallenge(TGameActionQueue* lpActionQueue, EChallengeStatus leChallengeStatus)
{
    CGS_ASSERT(leChallengeStatus != E_CHALLENGE_STATUS_ONGOING,
               "leChallengeStatus != E_CHALLENGE_STATUS_ONGOING");   // X360 cmpwi r30,0; bne skip; @2494

    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)   // X360 lwz +0xE08; cmpwi 0; bne
    {
        meLocalChallengeStatus = leChallengeStatus;   // X360 stw r30,+0xE30
        mbRemoteEndPending      = true;               // X360 stb 1,+0xE37
        return;                                       // X360 b restore
    }

    if (leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)   // X360 loc_823470F4: cmpwi r30,1; bne
    {
        CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");   // X360 lwz +0xE0C; @2509

        // X360 ld +0xC0 (mpCurrentChallenge->GetChallengeID()); bl GetChallengeIndex.
        const s32 liChallengeIndex = GetChallengeIndex(mpCurrentChallenge->GetChallengeID());
        if (liChallengeIndex >= 0)   // X360 cmpwi r4,0; bge
        {
            SetRemotePlayersChallengeCompleted(liChallengeIndex);   // X360 bl SetRemotePlayersChallengeCompleted
        }
        else
        {
            CGS_ASSERT(liChallengeIndex >= 0, "liChallengeIndex >= 0");   // X360 @2511
        }
    }

    // Empty end-not-active action (no payload): X360 posts &stack byte, size 1, id 154.
    GameStateModuleIO::GameAction<GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE>
        lEndNotActiveAction;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEndNotActiveAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE, 1);  // X360 li r5,0x9A; li r6,1

    ClearPlayerSuccessData();   // X360 bl ClearPlayerSuccessData
}

// ----------------------------------------------------------------------------
// Disconnected -- X360 0x8234E548. The local player has dropped out of the online session:
// wipe every remote-player completion record, then tear down any in-flight challenge.
//
//   * Per remote slot (X360 loop base +0x5C8, stride 0x108: 32 std zeroing the completion bit
//     store, stw -1 @+0x100, stb 0 @+0x104): clear the completed-challenges bits, free the slot
//     (-1), clear the finalised flag.
//   * If the manager is not NONE, tear the challenge down via EndChallenge (X360 tail-call):
//       - RUNNING or RESULTS -> E_CHALLENGE_STATUS_ABORTED (X360 li r4,2)
//       - otherwise (PENDING) -> E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING (X360 li r4,4)
//     both with lbIsOnline == false (X360 li r6,0). A NONE manager returns after the wipe
//     (X360 beqlr).
// ----------------------------------------------------------------------------
void ChallengeManager::Disconnected(TGameActionQueue* lpActionQueue)
{
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        maChallengeCompletionData[liSlot].mCompletedChallenges.UnSetAll();   // X360 32x stdx 0, base +0x5C8
        maChallengeCompletionData[liSlot].mNetworkPlayerID = -1;             // X360 stw -1, +0x100
        maChallengeCompletionData[liSlot].mbFinalised      = false;          // X360 stb 0, +0x104
    }

    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)   // X360 lwz +0xE08; cmpwi 0; beqlr
    {
        // X360 folds (status == 2 || status == 3) into a bool, then selects 2 vs 4.
        if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING ||   // X360 cmpwi 2
            meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RESULTS)     // X360 cmpwi 3
        {
            EndChallenge(E_CHALLENGE_STATUS_ABORTED, lpActionQueue, false);              // X360 li r4,2
        }
        else
        {
            EndChallenge(E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING, lpActionQueue, false); // X360 li r4,4
        }
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_09.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_09.cpp
// ============================================================================
// Wave-B partfile (group 9) for BrnGameState::ChallengeManager -- the network-player
// completion-slot book-keeping trio. Store-for-store reconstruction over the keystone's
// NAMED members (see BrnChallengeManager.h for the layout evidence):
//
//   NetworkPlayerAdded     @ 0x823240B8  (DWARF :241)  -- claim a maChallengeCompletionData slot
//
// The maChallengeCompletionData slot layout (stride 0x108: FastBitArray<2000> bit store @+0x00,
// NetworkPlayerID @+0x100 where -1 == free, mbFinalised @+0x104) is the same free-slot/claim
// pattern the committed Construct/Destruct foundation uses.
//
// GROUP FUNCS BLOCKED (reported in funcs_blocked; NOT bodied here):
//   * NetworkPlayerFinalised @ 0x82347E88 -- unconditionally builds + posts the id-161
//     E_ACTION_ACTIVE_FREEBURN_CHALLENGE action (size 0x30). Its payload struct
//     {CgsID mChallengeID @+0x00, EActiveRaceCarIndex maPlayers[7] @+0x08 (packed indices of
//     the players who started the challenge), BrnNetwork::NetworkPlayerID mPlayerID @+0x24,
//     s32 miNumPlayersInChallenge @+0x28} was NOT frozen into BrnGameActions.h (only the
//     id-155/158/159 challenge actions were). It cannot be bodied store-for-store without
//     inventing that action type (out of scope -- same precedent as the id-153 gap that
//     blocked BeginChallenge/TriggerFreeburnChallenge in BrnChallengeManager_wB_07.cpp). The
//     id-156 half maps cleanly onto the header's ChallengeCompletionData, but the id-161 half
//     is a hard gap, so the whole function is blocked.
//   * NetworkPlayerRemoved @ 0x8234E420 -- unconditionally calls
//     mpGameStateModule->GetActiveRaceCarIndex(lPlayerID), but
//     BrnGameState::GameStateModule::GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID) is NOT
//     declared on GameStateModule in BrnGameStateModule.h (the method lives only on the
//     Network/Skillz classes). Cannot call it without editing that header (out of scope).
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// NetworkPlayerAdded -- X360 0x823240B8. Register a newly-joined network player in the
// completion-tracking pool. Posts no actions (lpActionQueue / lbIsOnline are unused by the
// X360 body). Two passes over maChallengeCompletionData (stride 0x108, scanning the
// mNetworkPlayerID field at slot+0x100):
//   1. Duplicate guard: if any slot already holds this player id, fire the "same player"
//      assert (the X360 builds the id into the message via StrStream -- the assert front-end,
//      represented by the failure condition per the committed foundation convention).
//   2. Claim the first free slot (mNetworkPlayerID == -1). If none is free, fire the
//      "added more than KI_MAX_REMOTE_PLAYERS players" assert. On success: store the player id,
//      clear the finalised flag, and wipe the completion bit store (X360 stw +0x100, stb 0
//      +0x104, then 32 std zeroes over the 256-byte FastBitArray == UnSetAll()).
// ----------------------------------------------------------------------------
void ChallengeManager::NetworkPlayerAdded(BrnNetwork::NetworkPlayerID lPlayerID,
                                          TGameActionQueue*, bool)
{
    // Pass 1 -- duplicate detection. The X360 fires only when a matching slot is found AND its
    // element pointer is non-null; a member-array element address is never null, so a match
    // alone triggers the assert.
    ChallengeCompletionData* lpExistingEntry = 0;   // X360 r24 seed 0
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID == lPlayerID)   // X360 lwz slot+0x100; cmpw
        {
            lpExistingEntry = &maChallengeCompletionData[liSlot];
            break;
        }
    }
    CGS_ASSERT(lpExistingEntry == 0, "Trying to add the same player to the challenge manager");

    // Pass 2 -- claim the first free slot (mNetworkPlayerID == -1).
    ChallengeCompletionData* lpFreeEntry = 0;   // X360 r24 re-seed 0
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID == -1)   // X360 lwz slot+0x100; cmpwi -1
        {
            lpFreeEntry = &maChallengeCompletionData[liSlot];
            break;
        }
    }
    CGS_ASSERT(lpFreeEntry != 0, "Added more than KI_MAX_REMOTE_PLAYERS players to the challenge manager");

    lpFreeEntry->mNetworkPlayerID = lPlayerID;         // X360 stw r23,+0x100
    lpFreeEntry->mbFinalised      = false;             // X360 stb 0,+0x104
    lpFreeEntry->mCompletedChallenges.UnSetAll();      // X360 32 std 0 over the 256-byte bit store
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_10.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_10.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the remote completion-status
// intake plumbing. Store-for-store reconstruction over the keystone's NAMED members
// (see BrnChallengeManager.h for the layout evidence):
//
//   UpdateRemotePlayerSuccessStatus @ 0x8234E210 (DWARF :492) -- drain the incoming
//     CompletedFburnChallengesData queue into the per-player completion records, then
//     re-broadcast the every-player status event.
//
// GROUP NOTE: two sibling methods of this group hit genuine FROZEN-header gaps and are
// left for a follow-up (reported in funcs_blocked, exact gaps below); they are NOT bodied
// here per the "skip + report, do not hack around a missing accessor" rule:
//
//   * OutputFreeburnChallengeEveryPlayerStatusEvent @ 0x82348080 -- its pre-loop
//     `memcpy(event+0x738, &mLocalChallengeCompletionData, 0x100)` writes the trailing
//     PRIVATE member GameStateModuleIO::FburnChallengeEveryPlayerStatusData::
//     mLocalChallengeCompletionData (BrnGameStateSharedIO.h), which exposes only
//     Construct()/AddCompletionStatus() and no accessor/setter for that field. Reaching it
//     by offset is an offset_hack lint failure and the header must not be edited.
//   * SetRemotePlayersChallengeCompleted @ 0x82323DF8 -- calls
//     BrnGameState::GameStateModule::GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID),
//     which is NOT declared on GameStateModule anywhere in the committed tree
//     (BrnGameStateModule.h); bodying it needs an additive declare-only grow to that
//     (non-frozen) header, which this partfile is forbidden to make.
//
// (Both are called by name from UpdateRemotePlayerSuccessStatus below / declared in the
// frozen header, so this file compiles against the declarations alone.)
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// UpdateRemotePlayerSuccessStatus -- X360 0x8234E210. Called by PreWorldUpdate when online.
// For every event queued on the incoming completed-challenge-status queue, copy that remote
// player's completion bit store into the matching maChallengeCompletionData slot (matched by
// mNetworkPlayerID), then -- if anything was queued -- re-emit the aggregated every-player
// status event onto the action queue.
//
// The queue element is GameStateModuleIO::CompletedFburnChallengesData (264-byte stride:
// s32 mNetworkPlayerID @0 + 4 pad + CompletedFburnChallenges bit store @8). The X360 copies
// the whole element to a stack temp (memcpy 0x108), matches by id, then blits the 256-byte
// bit store into the slot's FastBitArray<2000> mCompletedChallenges. When no slot owns the
// id it fires the "Failed to find ... who sent us data" assert (reduced from the X360's
// StrStream-formatted message per the plain-string CGS_ASSERT convention) and -- faithfully
// to the asm -- still falls through to the (now null-destination) copy.
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateRemotePlayerSuccessStatus(
    const CgsModule::BaseEventQueue<GameStateModuleIO::CompletedFburnChallengesData>* lpCompletedChallengeStatusQueue,
    TGameActionQueue* lpActionQueue)
{
    // X360 cmplwi r20,0; bne -> skip. Non-gating tripwire, then the queue is used regardless.
    CGS_ASSERT(lpCompletedChallengeStatusQueue != 0, "lpCompletedChallengeStatusQueue");

    if (lpCompletedChallengeStatusQueue->GetLength() > 0)   // X360 lwz r11,8(r20); cmpwi 0; ble skip
    {
        s32 liEvent = 0;
        do
        {
            // X360 bl BaseEventQueue<>::GetEvent(v6); memcpy(v30, event, 0x108) -- take a
            // local copy of the whole 264-byte element.
            GameStateModuleIO::CompletedFburnChallengesData lEvent =
                lpCompletedChallengeStatusQueue->GetEvent(liEvent);

            // Find the completion record owned by this player id (X360 walks
            // maChallengeCompletionData[i].mNetworkPlayerID from slot 0, stride 0x108).
            CgsContainers::FastBitArray<2000>* lpDestCompletionData = 0;   // X360 r29 = 0
            s32 liSlot = 0;
            while (maChallengeCompletionData[liSlot].mNetworkPlayerID != lEvent.mNetworkPlayerID)
            {
                ++liSlot;
                if (liSlot >= KI_MAX_REMOTE_PLAYERS)   // X360 cmpwi r11,7; blt loop / else fall to assert
                {
                    break;
                }
            }

            if (liSlot < KI_MAX_REMOTE_PLAYERS)
            {
                // Found: X360 computes r29 = &slot.mCompletedChallenges (the r29 != 0 test is
                // always true for this member address, so the assert below is never taken here).
                lpDestCompletionData = &maChallengeCompletionData[liSlot].mCompletedChallenges;
            }
            else
            {
                CGS_ASSERT(false,
                           "Failed to find challenge status data entry for player () who sent us data\n");
            }

            // X360 LABEL_15 (reached from both the found path and the assert fall-through):
            // memcpy(r29, &event.mCompletedFreeburnChallenges, 0x100). Bridges the slot's
            // FastBitArray<2000> store and the event's CompletedFburnChallenges field (identical
            // 256-byte layouts).
            std::memcpy(lpDestCompletionData, &lEvent.mCompletedFreeburnChallenges, 256);

            ++liEvent;
        }
        while (liEvent < lpCompletedChallengeStatusQueue->GetLength());   // X360 re-reads lwz 8(r20)
    }

    if (lpCompletedChallengeStatusQueue->GetLength() > 0)   // X360 second lwz r11,8(r20); cmpwi 0; ble skip
    {
        CGS_ASSERT(lpActionQueue != 0, "lpActionQueue");   // X360 cmplwi r31,0; bne -> skip
        OutputFreeburnChallengeEveryPlayerStatusEvent(lpActionQueue);
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_11.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_11.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the three network/road success
// event handlers. Store-for-store reconstructions of the X360 ARTIST methods over the
// keystone's NAMED members (see BrnChallengeManager.h for the layout evidence):
//
//   HandleSuccessUpdateEvent   @ 0x8233CDE0  (DWARF :212) -- remote last-second success mask
//   HandleChallengeSuccessEvent@ 0x82316AE0  (DWARF :218) -- per-action success/accumulation grid
//   HandleRoadRuleScore        @ 0x82334D48  (DWARF :224) -- road-rule time/crash score feed
//
// Every handler gates on the manager being RUNNING (meChallengeManagerStatus == 2). The
// two remote handlers additionally verify the event's active-race-car index; the two frame
// stamped events (Success/SuccessUpdate) drop nothing on frame here (HandleSuccessUpdateEvent
// has no frame gate), while HandleChallengeSuccessEvent drops events whose update frame is
// not newer than miLastChallengeResetFrame (X360-only latch, spec section A).
//
// The FastBitArray<60>/<120> bit scan + set in HandleSuccessUpdateEvent is the X360 inlined
// container iteration (GetFirstBitSet/GetNextBitSet/SetBit, complete with its CgsDev::StrStream
// range-assert scaffolding); per spec pitfall 5 the committed container methods are called
// directly and the assert/StrStream machinery is intentionally not reproduced (benign).
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// HandleSuccessUpdateEvent -- X360 0x8233CDE0. Fold a remote player's per-frame "last second
// success" bit mask into that player's success ring, then latch the player's arbitration slot.
//
//   * Only while RUNNING (X360 lwz +0xE08; cmpwi 2). No frame gate on this event.
//   * mabReceivedSuccessUpdates[arci] = true (X360 stb 1, +0x590 + arci).
//   * For every set bit of the event mask (FastBitArray<60> mChallengeSuccessUpdate): the
//     network frame (event->miChallengeUpdateFrame + bit) is folded modulo the sim ring size
//     (100 @ 50Hz, 120 @ 60Hz) and its slot is set in this player's FastBitArray<120> ring
//     (X360 inlines GetFirstBitSet/GetNextBitSet/SetBit + the range asserts).
//   * Finally the mask's per-frequency last-second bit (bit 49 @ 50Hz, bit 59 @ 60Hz) selects
//     the arbitration success: DONE if set, NONE otherwise.
// ----------------------------------------------------------------------------
void ChallengeManager::HandleSuccessUpdateEvent(
    const CgsSystem::TimerStatusInterface* lpTimerStatusInterface,
    const GameStateModuleIO::FburnChallengeSuccessUpdateEvent* lpEvent)
{
    CGS_ASSERT(lpTimerStatusInterface, "lpTimerStatusInterface");   // X360 line 4698
    CGS_ASSERT(lpEvent, "lpEvent");                                 // X360 line 4699

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz +0xE08; cmpwi 2
    {
        const EActiveRaceCarIndex leActiveRaceCarIndex = lpEvent->meActiveRaceCarIndex;   // X360 lwz +8
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");     // X360 line 4712
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");  // X360 line 4713

        mabReceivedSuccessUpdates[leActiveRaceCarIndex] = true;             // X360 stb 1, +0x590 + arci

        // Walk every set bit of the incoming last-second success mask; each maps a network
        // frame into this player's per-frame success ring.
        for (s32 liBit = lpEvent->mChallengeSuccessUpdate.GetFirstBitSet();
             liBit != GameStateModuleIO::LastSecondChallengeSuccess::KI_INVALID_BIT_INDEX;
             liBit = lpEvent->mChallengeSuccessUpdate.GetNextBitSet(liBit))
        {
            // X360 divides by 100 (50Hz) or 120 (60Hz) and keeps the remainder.
            const s32 liRingSize = lpTimerStatusInterface->IsSimTimerFrequency50Hz() ? 100 : 120;
            const s32 liFrameSlot = (lpEvent->miChallengeUpdateFrame + liBit) % liRingSize;
            maPlayerSuccessUpdateArray[leActiveRaceCarIndex].SetBit(static_cast<u32>(liFrameSlot));
        }

        // X360 tail: reads the sim time step directly (== IsSimTimerFrequency50Hz) and tests the
        // matching last-second bit of the mask (1<<49 @ 50Hz, 1<<59 @ 60Hz).
        bool lbLastSecondSuccess;
        if (lpTimerStatusInterface->IsSimTimerFrequency50Hz())
        {
            lbLastSecondSuccess = lpEvent->mChallengeSuccessUpdate.IsBitSet(49);
        }
        else
        {
            lbLastSecondSuccess = lpEvent->mChallengeSuccessUpdate.IsBitSet(59);
        }

        maaePlayersSuccessStatus[leActiveRaceCarIndex][miCurrentArbitrationIndex] =
            lbLastSecondSuccess ? GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE
                                : GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
    }
}

// ----------------------------------------------------------------------------
// HandleChallengeSuccessEvent -- X360 0x82316AE0. Apply a remote player's per-action challenge
// score/success event to that player's row of the success grids.
//
//   * Only while RUNNING and only when the event's update frame is newer than the last
//     challenge-reset frame (X360 lwz +0xC > lwz +0x1000).
//   * arci + mpCurrentChallenge validated (non-fatal asserts).
//   * For each action slot: if the event flags it successful, copy the score, (re)arm the
//     convoy timer on the arbitration action, then fold the score into the grids per coop
//     type. If not successful but the event carries an individual-accumulation contribution,
//     record it as a CONTRIBUTING partial score.
// ----------------------------------------------------------------------------
void ChallengeManager::HandleChallengeSuccessEvent(
    const GameStateModuleIO::FburnChallengeSuccessEvent* lpEvent)
{
    CGS_ASSERT(lpEvent, "lpEvent");   // X360 line 4778

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING &&      // X360 lwz +0xE08; cmpwi 2
        lpEvent->miChallengeUpdateFrame > miLastChallengeResetFrame)           // X360 lwz +0xC > lwz +0x1000
    {
        const EActiveRaceCarIndex leActiveRaceCarIndex = lpEvent->meActiveRaceCarIndex;   // X360 lwz +0x10
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");        // X360 line 4792
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");     // X360 line 4793
        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");                  // X360 line 4794

        for (s32 liActionIndex = 0; liActionIndex < KI_MAX_CHALLENGE_ACTIONS; ++liActionIndex)
        {
            const BrnResource::ChallengeListEntryAction* lpAction =
                mpCurrentChallenge->GetAction(liActionIndex);
            CGS_ASSERT(lpAction, "lpAction");   // X360 line 4800

            if (lpEvent->mabSuccessfulActions[liActionIndex])   // X360 lbzx +8 + action
            {
                maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] =
                    lpEvent->mafActionScores[liActionIndex];    // X360 stfsx +0x468 grid

                if (liActionIndex == miCurrentArbitrationIndex &&   // X360 cmpw action, +0xE14
                    lpAction->HasConvoyTime() &&                    // X360 lfs +0x44 > 0.0
                    !mbConvoyTimerRunning)                          // X360 lbz +0xE24 == 0
                {
                    mfConvoyTimer        = lpAction->GetConvoyTime();   // X360 stfs +0xE20
                    mbConvoyTimerRunning = true;                        // X360 stb 1, +0xE24
                }

                switch (static_cast<s32>(lpAction->GetCoopType()))   // X360 lbz +1 (muCoopType)
                {
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE:   // 4
                    {
                        s32 liRemainingTarget = maiRemainingTarget[liActionIndex] -
                            static_cast<s32>(maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex]);
                        if (liRemainingTarget < 0)
                        {
                            liRemainingTarget = 0;
                        }
                        maiRemainingTarget[liActionIndex] = liRemainingTarget;

                        const GameStateModuleIO::EFreeburnChallengeSuccess lePreviousStatus =
                            maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex];
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] +=
                            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        if (lePreviousStatus != GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
                        {
                            maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                                GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;
                        }
                        break;
                    }
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:              // 1
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION: // 2
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        break;
                    case 6:   // X360 coop-type value 6 (no committed enumerator; scored as CONTRIBUTING)
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        break;
                    default:
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
                        break;
                }
            }
            else if (lpEvent->mabAccumulationThisFrame[liActionIndex] &&   // X360 lbz +0xA + action
                     lpAction->GetCoopType() ==
                         BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION)
            {
                maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] =
                    lpEvent->mafActionScores[liActionIndex];
                maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;
                maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                    lpEvent->mafActionScores[liActionIndex];
            }
        }
    }
}

// ----------------------------------------------------------------------------
// HandleRoadRuleScore -- X360 0x82334D48. Feed a freshly recorded road-rule score into the
// current challenge's road-rule action.
//
//   * Only while RUNNING; the current challenge + action index are validated.
//   * The current action must be the matching road-rule action for the score type
//     (ROAD_RULE_TIME + E_SCORE_TYPE_TIME, or ROAD_RULE_CRASH + E_SCORE_TYPE_CRASH) and its
//     target road id (GetCgsIDTarget(1)) must match the scored road.
//   * When the score record actually carries a score for that type, it is pushed under the
//     matching freeburn skill: the time score is scaled by the 0.001 rodata epsilon
//     (flt_82013F90), the crash score is pushed raw. Neither banks immediately.
// ----------------------------------------------------------------------------
void ChallengeManager::HandleRoadRuleScore(BrnStreetData::ChallengePlayerScoreEntry lChallengeScore,
                                           BrnStreetData::ScoreType leScoreType, CgsID lRoadID)
{
    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz +0xE08; cmpwi 2
    {
        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");             // X360 line 5849
        CGS_ASSERT(miCurrentChallengeAction < mpCurrentChallenge->GetNumActions(),
                   "miCurrentChallengeAction < mpCurrentChallenge->GetNumActions()");   // X360 line 5851

        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction(miCurrentChallengeAction);

        if (lpAction->GetActionType() ==
                BrnResource::ChallengeListEntryAction::E_CHALLENGE_ACTION_ROAD_RULE_TIME &&
            leScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
        {
            if (lpAction->GetCgsIDTarget(1) == lRoadID)   // X360 (u64)action+0x38 == lRoadID
            {
                if (lChallengeScore.ContainsData(BrnStreetData::E_SCORE_TYPE_TIME))
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_ROAD_RULE_TIME,
                        static_cast<f32>(lChallengeScore.GetScore(BrnStreetData::E_SCORE_TYPE_TIME)) * 0.001f,
                        false);
                }
            }
        }
        else if (lpAction->GetActionType() ==
                     BrnResource::ChallengeListEntryAction::E_CHALLENGE_ACTION_ROAD_RULE_CRASH &&
                 leScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
        {
            if (lpAction->GetCgsIDTarget(1) == lRoadID)
            {
                if (lChallengeScore.ContainsData(BrnStreetData::E_SCORE_TYPE_CRASH))
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_ROAD_RULE_CRASH,
                        static_cast<f32>(lChallengeScore.GetScore(BrnStreetData::E_SCORE_TYPE_CRASH)),
                        false);
                }
            }
        }
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_12.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_12.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-B partfile (group 12): the two output-writer
// helpers that pack the per-frame FreeburnChallengeUpdateAction plus the driver that dispatches
// the deferred remote start/trigger/end requests.
//
//   WriteDataToOutput            (X360 0x82346918)
//   WriteDataToOutputForTarget   (X360 0x823236D0)
//   UpdateRemoteRequests         (X360 0x82351990)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store/branch/early-out;
// raw offsets are mapped onto the keystone-frozen named members/accessors. The output action's
// [action][player] grids are the TRANSPOSED counterpart of the manager's [player][action] grids
// (spec pitfall 4). sub_8231D800 is OutputBuffer::GetGameStateToNetworkInterface (asm assert
// string), NOT the game-action queue; the game-action queue is OutputBuffer::GetGameActionQueue
// (null-checked) / GetGuiOutputQueue (concrete-typed twin actually posted through, matching the
// committed DeveloperChallengeManager pattern). The active-race-car loops are reconstructed as
// plain counted loops over the raw X360 register counter (the inlined EActiveRaceCarIndex
// operator++ range guard is a non-behavioural debug assert).


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// WriteDataToOutput  (X360 0x82346918)
// ----------------------------------------------------------------------------
// Builds the per-frame FreeburnChallengeUpdateAction on the stack (time-left, current action,
// per-target rows) and posts it to the output buffer's game-action queue, then mirrors the
// per-player "started challenge" flags into the GameState->Network interface.
void ChallengeManager::WriteDataToOutput(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer, "lpOutput");   // BrnChallengeManager.cpp:1705

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING ||
        meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RESULTS)
    {
        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1718
        CGS_ASSERT(miCurrentArbitrationIndex < mpCurrentChallenge->GetNumActions(),
                   "miCurrentArbitrationIndex < mpCurrentChallenge->GetNumActions()");   // :1719

        const BrnResource::ChallengeListEntryAction* lpArbitrationAction =
            mpCurrentChallenge->GetAction(miCurrentArbitrationIndex);

        // Skip forward over any run of E_COMBINE_ACTION_INDEPENDENT actions starting at the
        // arbitration index; the resulting index is what the event reports as the current action.
        s32 liCurrentActionIndex = miCurrentArbitrationIndex;
        if (lpArbitrationAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT)
        {
            if (liCurrentActionIndex < mpCurrentChallenge->GetNumActions())
            {
                do
                {
                    if (mpCurrentChallenge->GetAction(liCurrentActionIndex)->GetCombineAction() !=
                        BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT)
                    {
                        break;
                    }
                    ++liCurrentActionIndex;
                }
                while (liCurrentActionIndex < mpCurrentChallenge->GetNumActions());
            }
        }

        GameStateModuleIO::FreeburnChallengeUpdateAction loUpdateAction;
        loUpdateAction.mfTimeLeftInChallenge = mbChallengeTimerRunning ? mfChallengeTimer : -1.0f;
        loUpdateAction.miCurrentActionIndex  = liCurrentActionIndex;
        loUpdateAction.miNumTargetsUsed      = 0;

        // Does the challenge contain an action of the (X360-drift) type 22? -- if so every target
        // is written out below regardless of the arbitration action's combine mode.
        bool lbWriteAllTargets = false;
        if (mpCurrentChallenge->GetNumActions() != 0)
        {
            s32 liScanAction = 0;
            while (true)
            {
                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1749
                const BrnResource::ChallengeListEntryAction* lpScanAction =
                    mpCurrentChallenge->GetAction(liScanAction);
                CGS_ASSERT(lpScanAction, "mpCurrentChallenge->GetAction(liIndex)");   // :1750
                if (static_cast<s32>(lpScanAction->GetActionType()) == 22)   // X360-drift MEET-UP-style action id
                {
                    lbWriteAllTargets = true;
                    break;
                }
                ++liScanAction;
                if (liScanAction >= mpCurrentChallenge->GetNumActions())
                {
                    break;
                }
            }
        }

        const BrnResource::ChallengeListEntryAction::ECombineActionType leCombineAction =
            lpArbitrationAction->GetCombineAction();
        if (leCombineAction == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT ||
            leCombineAction == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT ||
            lbWriteAllTargets)
        {
            if (mpCurrentChallenge->GetNumActions() != 0)
            {
                s32 liTargetIndex = 0;
                do
                {
                    WriteDataToOutputForTarget(&loUpdateAction, liTargetIndex);
                    ++liTargetIndex;
                }
                while (liTargetIndex < mpCurrentChallenge->GetNumActions());
            }
        }
        else if (lpArbitrationAction->GetNumTargets() != 0)
        {
            WriteDataToOutputForTarget(&loUpdateAction, miCurrentArbitrationIndex);
        }

        CGS_ASSERT(lpOutputBuffer->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");   // :1778
        TGameActionQueue* lpActionQueue = lpOutputBuffer->GetGuiOutputQueue();
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&loUpdateAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_UPDATE,
                                sizeof(GameStateModuleIO::FreeburnChallengeUpdateAction));
    }

    // Publish the per-player "player started the challenge" flags to the network interface.
    for (s32 liActiveRaceCarIndex = 0; liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liActiveRaceCarIndex)
    {
        CGS_ASSERT(lpOutputBuffer->GetGameStateToNetworkInterface(),
                   "lpOutput->GetGameStateToNetworkInterface()");   // :1787
        lpOutputBuffer->GetGameStateToNetworkInterface()->SetPlayerInFreeburnChallenge(
            static_cast< ::EActiveRaceCarIndex>(liActiveRaceCarIndex),
            mabPlayerStartedChallenge[liActiveRaceCarIndex]);
    }
}

// ----------------------------------------------------------------------------
// WriteDataToOutputForTarget  (X360 0x823236D0)
// ----------------------------------------------------------------------------
// Fills one target column (row [miNumTargetsUsed]) of the update action: per-player contribution
// + completion status + overall remaining, driven by the action's coop type, then bumps
// miNumTargetsUsed. The output grids are [target][player], transposed vs the manager's grids.
void ChallengeManager::WriteDataToOutputForTarget(GameStateModuleIO::FreeburnChallengeUpdateAction* lpUpdateAction,
                                                  s32 liActionIndex)
{
    const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liActionIndex);

    s32 liOverallTargetRemaining;
    if (lpAction->GetNumTargets() != 0 && lpAction->GetTargetValue(0) > 1)
    {
        switch (lpAction->GetCoopType())
        {
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE:   // 4
            for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
            {
                CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :1821
                f32 lfContribution = maafCumulativeContributions[liCar][liActionIndex];
                if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liCar)
                {
                    const s32 liSkill =
                        KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill != KI_FREEBURN_SKILL_COUNT_X360 && !mabBankedSkillThisFrame[liSkill])
                    {
                        lfContribution = mafActiveSkillValue[liSkill] +
                                         maafCumulativeContributions[liCar][liActionIndex];
                    }
                }
                lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                    lfContribution;
            }
            break;

        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:   // 1
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT:        // 6
            for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
            {
                CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :1859
                if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liCar &&
                    maaePlayersSuccessStatus[liCar][liActionIndex] !=
                        GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
                {
                    const s32 liSkill =
                        KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] = 0.0f;
                    }
                    else
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                            mafActiveSkillValue[liSkill];
                    }
                }
                else
                {
                    lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                        maafCumulativeContributions[liCar][liActionIndex];
                }
            }
            break;

        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION:   // 2
            for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
            {
                CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :1894
                if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liCar &&
                    maaePlayersSuccessStatus[liCar][liActionIndex] !=
                        GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
                {
                    const s32 liSkill =
                        KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] = 0.0f;
                    }
                    else
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                            mafActiveSkillValue[liSkill] + mafBankedActionScores[liSkill];
                    }
                }
                else
                {
                    lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                        maafCumulativeContributions[liCar][liActionIndex];
                }
            }
            break;

        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_SIMULTANEOUS:   // 3
            for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
            {
                CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :1929
                if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liCar)
                {
                    const s32 liSkill =
                        KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] = 0.0f;
                    }
                    else
                    {
                        lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] =
                            mafActiveSkillValue[liSkill];
                    }
                }
                else
                {
                    lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][liCar] = 0.0f;
                }
            }
            break;

        default:   // E_CHALLENGE_COOP_TYPE_ONCE (0) / E_CHALLENGE_COOP_TYPE_AVERAGE (5)
            std::memset(&lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][0], 0,
                        sizeof(lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed]));
            break;
        }

        CGS_ASSERT(liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                   "liTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");   // :1956
        CGS_ASSERT(lpUpdateAction->miNumTargetsUsed < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                   "lpOutEvent->miNumTargetsUsed < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");   // :1957
        liOverallTargetRemaining = maiRemainingTarget[liActionIndex];
    }
    else
    {
        std::memset(&lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed][0], 0,
                    sizeof(lpUpdateAction->maafIndividualTargetContributions[lpUpdateAction->miNumTargetsUsed]));
        CGS_ASSERT(lpUpdateAction->miNumTargetsUsed < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                   "lpOutEvent->miNumTargetsUsed < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");   // :1963
        liOverallTargetRemaining = 0;
    }

    lpUpdateAction->maiOverallTargetRemaining[lpUpdateAction->miNumTargetsUsed] = liOverallTargetRemaining;

    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        if (mabPlayerStartedChallenge[liCar])
        {
            if (lpAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_CHAIN)
            {
                // Walk the chain of E_COMBINE_ACTION_CHAIN actions to its end, then report that
                // action's per-player success status.
                s32 liChainActionIndex = liActionIndex;
                if (liActionIndex < mpCurrentChallenge->GetNumActions())
                {
                    while (mpCurrentChallenge->GetAction(liChainActionIndex)->GetCombineAction() ==
                           BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_CHAIN)
                    {
                        ++liChainActionIndex;
                        if (liChainActionIndex >= mpCurrentChallenge->GetNumActions())
                        {
                            break;
                        }
                    }
                }
                lpUpdateAction->maaeCompleted[lpUpdateAction->miNumTargetsUsed][liCar] =
                    maaePlayersSuccessStatus[liCar][liChainActionIndex];
            }
            else
            {
                lpUpdateAction->maaeCompleted[lpUpdateAction->miNumTargetsUsed][liCar] =
                    maaePlayersSuccessStatus[liCar][liActionIndex];
            }
        }
        else
        {
            CGS_ASSERT(mpGameStateModule, "mpGameStateModule");   // :2001
            CGS_ASSERT(mpGameStateModule->GetModeManager(), "mpGameStateModule->GetModeManager()");   // :2002
            ScoringSystem* lpScoringSystem = mpGameStateModule->GetModeManager()->GetScoringSystem();
            CGS_ASSERT(lpScoringSystem, "lpScoringSystem");   // :2004
            if (lpScoringSystem->GetCarData(static_cast<EActiveRaceCarIndex>(liCar)))
            {
                lpUpdateAction->maaeCompleted[lpUpdateAction->miNumTargetsUsed][liCar] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NOT_IN_CHALLENGE;
            }
            else
            {
                lpUpdateAction->maaeCompleted[lpUpdateAction->miNumTargetsUsed][liCar] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
            }
        }
    }

    ++lpUpdateAction->miNumTargetsUsed;
}

// ----------------------------------------------------------------------------
// UpdateRemoteRequests  (X360 0x82351990)
// ----------------------------------------------------------------------------
// Drains the three deferred remote-request flags set while the arbitrator's decisions were
// pending. On the arbitrator (lbIsOnline) each request is an error (the arbitrator drives the
// challenge itself); otherwise the request is replayed locally as a remote begin/trigger/end.
// The asm asserts the request-holder is not the arbitrator via a verbatim "!lbIsArbitrator"
// (the r6 bool the header spells lbIsOnline).
void ChallengeManager::UpdateRemoteRequests(TGameActionQueue* lpActionQueue,
                                            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                                            bool lbIsOnline)
{
    if (mbRemoteStartPending)
    {
        if (lbIsOnline)
        {
            CGS_ASSERT(!lbIsOnline, "!lbIsArbitrator");   // BrnChallengeManager.cpp:818
        }
        else
        {
            BeginChallenge(mpCurrentChallenge->GetChallengeID(), lpActionQueue,
                           lpActiveRaceCarOutputInterface, lbIsOnline, true);
        }
        mbRemoteStartPending = false;
    }

    if (mbRemoteTriggerPending)
    {
        if (lbIsOnline)
        {
            CGS_ASSERT(!lbIsOnline, "!lbIsArbitrator");   // :830
        }
        else
        {
            BeginChallenge(mpCurrentChallenge->GetChallengeID(), lpActionQueue,
                           lpActiveRaceCarOutputInterface, lbIsOnline, false);
            TriggerFreeburnChallenge(mpCurrentChallenge->GetChallengeID(), lpActionQueue, lbIsOnline);
        }
        mbRemoteTriggerPending = false;
    }

    if (mbRemoteEndPending)
    {
        if (lbIsOnline)
        {
            CGS_ASSERT(!lbIsOnline, "!lbIsArbitrator");   // :844
        }
        else if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)
        {
            EndChallenge(meLocalChallengeStatus, lpActionQueue, false);
        }
        mbRemoteEndPending = false;
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_13.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_13.cpp
// ============================================================================
// Wave-B partfile (group 13) for BrnGameState::ChallengeManager. Store-for-store
// reconstruction over the keystone-frozen NAMED members (see BrnChallengeManager.h).
//
// BODIED HERE:
//   UpdateActionSuccess  -- X360 0x823460A0
//
// NOT bodied (reported as blocked): the group's other two members --
//   UpdateArbitration         @ 0x82351AF8
//   UpdateArbitrationSuccess  @ 0x82350340
// Both post the 0x20-byte id-153 E_ACTION_FREEBURN_CHALLENGE action (X360
// AddEvent(queue, &ev, 153, 0x20)), whose dedicated payload struct is NOT declared in
// any frozen header -- only the id-155/158/159 challenge actions exist in BrnGameActions.h
// (this is the SAME blocker the sibling partfiles wB_06 UpdateResults and wB_07 Begin/
// TriggerFreeburnChallenge report). The id-153 payload is integral to both bodies (the
// arbitration-accept spine), so neither can be reconstructed store-for-store without
// inventing that type or editing a header (out of scope). Exact attested layout is in the
// funcs_blocked report so a keystone follow-up can add the struct and unblock them.
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// UpdateActionSuccess -- X360 0x823460A0. Called by UpdateChallenge once a player's
// action score has been (re)evaluated: it folds that player's per-action success state
// into the manager's [player][action] grids according to the action's coop type.
//
// Three coop-type regimes (outer switch on lpAction->GetCoopType()):
//   * SIMULTANEOUS (3) -- latch this player into the last-second success bit sets and post
//     the id-158 FburnChallengeSuccessUpdateAction carrying the current last-second mask.
//   * ONCE/INDIVIDUAL/INDIVIDUAL_ACCUMULATION/CUMULATIVE/AVERAGE/COUNT (0,1,2,4,5,6) --
//     on SUCCESS, bank the contribution against the CURRENT challenge action; otherwise
//     re-derive this player's CONTRIBUTING/NONE status from the running skill value.
//   * anything else -- "Unknown action coop type" assert.
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateActionSuccess(const BrnResource::ChallengeListEntryAction* lpAction,
                                           s32 liActionIndex,
                                           EChallengeStatus leChallengeStatus,
                                           EActiveRaceCarIndex leActiveRaceCarIndex,
                                           s32 liNumPlayers,
                                           TGameActionQueue* lpActionQueue,
                                           bool lbIsOnline)
{
    CGS_ASSERT(lpAction != 0, "lpAction");   // X360 @ BrnChallengeManager.cpp:1130
    CGS_ASSERT(leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                   leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "( lePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )"); // :1131

    switch (lpAction->GetCoopType())   // X360 lbz +0x01(lpAction); jump table (cases 0-6)
    {
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_SIMULTANEOUS:  // 3
        {
            // Two independent "last-second window" bit indices: mLastSecondSuccessStatus is a
            // FastBitArray<60> (frame modulo 50 online / 60 offline), maPlayerSuccessUpdateArray
            // a FastBitArray<120> (frame modulo 100 online / 120 offline). X360 computes each via
            // a signed reciprocal-multiply divide then `frame - D*(frame/D)`.
            s32 liLastSecondBit;
            s32 liPlayerUpdateBit;
            if (lbIsOnline)
            {
                liLastSecondBit   = liNumPlayers - 50 * (liNumPlayers / 50);
                liPlayerUpdateBit = liNumPlayers - 100 * (liNumPlayers / 100);
            }
            else
            {
                liLastSecondBit   = liNumPlayers - 60 * (liNumPlayers / 60);
                liPlayerUpdateBit = liNumPlayers - 120 * (liNumPlayers / 120);
            }

            if (leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)   // X360 cmpwi r26,1
            {
                mLastSecondSuccessStatus.SetBit(static_cast<u32>(liLastSecondBit));
                maPlayerSuccessUpdateArray[leActiveRaceCarIndex].SetBit(static_cast<u32>(liPlayerUpdateBit));
                maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;   // 3
            }
            else
            {
                mLastSecondSuccessStatus.UnSetBit(static_cast<u32>(liLastSecondBit));
                maPlayerSuccessUpdateArray[leActiveRaceCarIndex].UnSetBit(static_cast<u32>(liPlayerUpdateBit));
                maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;   // 0
            }

            // Post the id-158 success-update action carrying the whole last-second mask (X360
            // `ld r11,+0xE00; std r11` == copy the FastBitArray<60>'s single field) + this action.
            GameStateModuleIO::FburnChallengeSuccessUpdateAction lSuccessUpdate;
            lSuccessUpdate.mChallengeSuccessUpdate = mLastSecondSuccessStatus;
            lSuccessUpdate.miActionIndex           = liActionIndex;
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSuccessUpdate),
                                    GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SUCCESS_UPDATE,
                                    sizeof(lSuccessUpdate));   // X360 li r5,0x9E; li r6,0x10
            break;
        }

        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_ONCE:                    // 0
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:              // 1
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION: // 2
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE:              // 4
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_AVERAGE:                 // 5
        case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT:                   // 6
        {
            if (leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)   // X360 loc_823465B4: cmpwi r26,1 (fall-through)
            {
                CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");   // :1197
                const BrnResource::ChallengeListEntryAction* lpCurrentAction =
                    mpCurrentChallenge->GetAction(liActionIndex);

                // Arm the convoy timer from the current action's convoy time on first success.
                if (lpCurrentAction->GetConvoyTime() > 0.0f && !mbConvoyTimerRunning)
                {
                    mfConvoyTimer        = lpCurrentAction->GetConvoyTime();
                    mbConvoyTimerRunning = true;
                }

                switch (lpCurrentAction->GetCoopType())   // X360 lbz +0x01(current action)
                {
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE: // 4
                        // Draw down the shared remaining target by this player's action score
                        // (clamped at 0), accumulate the scaled contribution, mark CONTRIBUTING.
                        maiRemainingTarget[liActionIndex] -=
                            static_cast<s32>(maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex]);
                        if (maiRemainingTarget[liActionIndex] < 0)
                        {
                            maiRemainingTarget[liActionIndex] = 0;
                        }
                        {
                            const f32 lfScale = GetScaleFactor();
                            maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                                lfScale * maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] +
                                maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex];
                        }
                        if (maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] !=
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
                        {
                            maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                                GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;   // 2
                        }
                        break;

                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:              // 1
                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION: // 2
                    {
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;   // 3
                        const f32 lfCumulative = maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex];
                        const f32 lfCurrent    = maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        // X360 fsel: keep the larger of the accumulated / this-frame contribution.
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                            ((lfCumulative - lfCurrent) >= 0.0f) ? lfCumulative : lfCurrent;
                        break;
                    }

                    case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT: // 6
                        maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] =
                            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;   // 2
                        break;

                    default:
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;   // 3
                        break;
                }
            }
            else   // X360 loc_823467BC: leChallengeStatus != SUCCESS
            {
                if (maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] ==
                    GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)   // already DONE -> leave it
                {
                    break;
                }

                const BrnResource::ChallengeListEntryAction::EChallengeCoopType leCoop = lpAction->GetCoopType();
                if (leCoop == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL ||   // 1
                    leCoop == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT)          // 6
                {
                    const s32 liSkill = KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)   // 38 == "no skill" sentinel
                    {
                        break;
                    }
                    maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                        (mafActiveSkillValue[liSkill] > 0.0f)
                            ? GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING   // 2
                            : GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;          // 0
                }
                else if (leCoop == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION) // 2
                {
                    const s32 liSkill = KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                    if (liSkill == KI_FREEBURN_SKILL_COUNT_X360)   // 38
                    {
                        break;
                    }
                    if (mafActiveSkillValue[liSkill] > 0.0f)
                    {
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING;   // 2
                    }
                    else if (mafBankedActionScores[liSkill] != 0.0f)
                    {
                        break;   // banked value present -> leave status untouched
                    }
                    else
                    {
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;   // 0
                    }
                }
                // any other coop type -> no change
            }
            break;
        }

        default:
            CGS_ASSERT(false, "Unknown action coop type to deal with\n");   // X360 @ :1300
            break;
    }
}

}  // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_14.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_14.cpp
// ============================================================================
// Wave-B partfile (group 14) for BrnGameState::ChallengeManager -- the per-frame drivers.
// Store-for-store reconstructions over the keystone-frozen named layout (BrnChallengeManager.h):
//   * PreWorldUpdate  (X360 0x82353640) -- caches miFramesSinceNetworkStart, drains remote
//                                          success/requests, then runs the running/results/skill
//                                          sub-updates and flushes the update action to output.
//   * UpdateRunning   (X360 0x82353008) -- the RUNNING-state driver: honours the "make all
//                                          challenges N-player" debug toggles (inline hack /
//                                          UnHackAllChallenges), then UpdateTimer -> UpdateChallenge
//                                          -> UpdateArbitration.
//
// All manager state is reached through named members / committed accessors; every offset quoted
// in a comment is X360-asm-attested.
//
// BLOCKED in this group (reported in funcs_blocked, NOT bodied here):
//   PostWorldUpdate @ 0x8233AC28 -- the X360 body takes THREE parameters (r4 = crash-event queue,
//   r5 = the external stunt-score snapshot passed straight through to UpdateStuntScores, r6 = the
//   TimerStatusInterface). The keystone-frozen declaration in BrnChallengeManager.h has only TWO
//   (lpRaceCarCrashEventQueue, lpTimerStatusInterface): the middle stunt-score-snapshot parameter
//   is absent, so the function cannot be bodied against the frozen signature without either
//   dropping the UpdateStuntScores(snapshot) call or fabricating its argument. The header may not
//   be edited here (see funcs_blocked).
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// PreWorldUpdate -- X360 0x82353640.
// ----------------------------------------------------------------------------
// The mode manager's pre-world tick. Validates the output buffer + its game-action queue + the
// timer interface, snapshots the network-frame counter, then fans out to the remote-sync,
// running-state, results-state, output-flush and freeburn-skill sub-updates. The frame's fixed
// time step is the SIM timer's current step (mfBaseTimeStep * mfTimeStepMultiplier); the
// 50Hz-fixed-step predicate is IsSimTimerFrequency50Hz() (X360 inlines it as a fresh
// `simStep == 0.02f` compare against flt_82005574).
//
// X360 trailing-bool NOTE: the X360 build passes UpdateRunning's last two GPR args as
// (r8 = is-50Hz, r9 = is-online) -- i.e. the SIM-50Hz predicate lands in UpdateRunning's 5th
// parameter slot and the online flag in its 6th. That order is preserved verbatim below (the two
// values are handed to the slots exactly as the binary does); UpdateRunning consumes them in the
// same slot order (5th -> UpdateChallenge, 6th -> the UpdateArbitration gate).
// ----------------------------------------------------------------------------
void ChallengeManager::PreWorldUpdate(
    const CgsSystem::TimerStatusInterface*                                       lpTimerStatusInterface,
    s32                                                                          liFramesSinceNetworkStart,
    const CgsModule::BaseEventQueue<GameStateModuleIO::CompletedFburnChallengesData>* lpCompletedChallengeStatusQueue,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    bool                                                                         lbIsOnline,
    GameStateModuleIO::OutputBuffer*                                             lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer, "lpOutput");                                       // X360 line 0x2B0
    CGS_ASSERT(lpOutputBuffer->GetGuiOutputQueue(), "lpOutput->GetGameActionQueue()"); // X360 line 0x2B1
    CGS_ASSERT(lpTimerStatusInterface, "lpTimerStatusInterface");                 // X360 line 0x2B4

    // Fixed frame step + the network-frame snapshot (X360 stw,+0xFFC).
    const f32 lfTimeStep = lpTimerStatusInterface->GetSimTimerStatus()->GetCurrentTimeStep(); // lfs +0x20 * lfs +0x1C
    miFramesSinceNetworkStart = liFramesSinceNetworkStart;

    UpdateRemotePlayerSuccessStatus(lpCompletedChallengeStatusQueue, lpOutputBuffer->GetGuiOutputQueue());
    UpdateRemoteRequests(lpOutputBuffer->GetGuiOutputQueue(), lpActiveRaceCarOutputInterface, lbIsOnline);

    const bool lbIsSimTimerFrequency50Hz = lpTimerStatusInterface->IsSimTimerFrequency50Hz(); // fresh simStep == 0.02f

    // X360-attested arg slots: is-50Hz into the 5th slot, is-online into the 6th (see NOTE above).
    UpdateRunning(lfTimeStep, liFramesSinceNetworkStart, lpOutputBuffer->GetGuiOutputQueue(),
                  lpActiveRaceCarOutputInterface, lbIsSimTimerFrequency50Hz, lbIsOnline);

    UpdateResults(lfTimeStep, lpOutputBuffer->GetGuiOutputQueue());
    WriteDataToOutput(lpOutputBuffer);
    UpdateFreeburnSkillsThisFrame(lpActiveRaceCarOutputInterface, lfTimeStep);

    // [PC HARNESS] one bounded "[net] fburn" line per manager status change (LAN / harness runs).
    static s32 siWitnessedStatus = E_CHALLENGE_MANAGER_STATUS_NONE;
    if (static_cast<s32>(meChallengeManagerStatus) != siWitnessedStatus)
    {
        BrnNetHarnessPC::Witness("fburn", "status %d -> %d id=%llu host=%d frame=%d",
                                 siWitnessedStatus, static_cast<s32>(meChallengeManagerStatus),
                                 static_cast<unsigned long long>(GetCurrentFreeburnChallengeID()),
                                 lbIsOnline ? 1 : 0, liFramesSinceNetworkStart);
        siWitnessedStatus = static_cast<s32>(meChallengeManagerStatus);
    }
}

// ----------------------------------------------------------------------------
// UpdateRunning -- X360 0x82353008.
// ----------------------------------------------------------------------------
// First reconciles the two "make every challenge N-player" debug toggles against the last-applied
// state: when a toggle newly turns ON, every challenge's active player count (ChallengeListEntry
// byte +0xD3 low nibble) is forced to 1 (all-one-player) or 2 (all-two-player) via SetNumPlayers;
// when it turns OFF, UnHackAllChallenges restores the backed-up counts. The effective all-two flag
// is gated by the all-one flag (all-one wins).
//
// Then, only while the manager is RUNNING, runs the countdown timer and (when it is still going)
// the per-challenge update, latching the local player's received-success flag; the meet-up /
// convoy arbitration pass runs on the trailing gate.
//
// X360 slot NOTE (mirrors PreWorldUpdate): the 5th parameter (declared lbIsOnline) carries the
// SIM-50Hz predicate the binary routes into UpdateChallenge, and the 6th (declared
// lbIsTimerFrequency50Hz) carries the online flag that gates UpdateArbitration. The parameters are
// used strictly in that slot order below, reproducing the X360 data flow exactly.
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateRunning(
    f32                                                                          lfTimeStep,
    s32                                                                          liFramesSinceNetworkStart,
    TGameActionQueue*                                                            lpActionQueue,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    bool                                                                         lbIsOnline,
    bool                                                                         lbIsTimerFrequency50Hz)
{
    // ---- all-one-player toggle ---- (byte101C vs byte_82FAD9C1 == mbChallengesAreAllOnePlayer)
    if (mbWereAllChallengesOnePlayer != mbChallengesAreAllOnePlayer)
    {
        if (mbChallengesAreAllOnePlayer)
        {
            // Hack every entry's active player count to 1 (X360 rlwimi ,1,28,23 -> low nibble = 1).
            for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
            {
                BrnResource::ChallengeListEntry* lpChallenge =
                    const_cast<BrnResource::ChallengeListEntry*>(mpFreeburnChallengeList->GetChallengeData(liChallenge));
                lpChallenge->SetNumPlayers(1);
            }
        }
        else
        {
            UnHackAllChallenges();
        }
        mbWereAllChallengesOnePlayer = mbChallengesAreAllOnePlayer;
    }

    // Effective all-two-player state: mbChallengesAreAllTwoPlayer AND NOT mbChallengesAreAllOnePlayer
    // (all-one wins). X360: `(!C1 && C0) ? 1 : 0`, C1 == AllOnePlayer, C0 == AllTwoPlayer.
    const bool lbEffectiveAllTwoPlayer =
        (!mbChallengesAreAllOnePlayer) && mbChallengesAreAllTwoPlayer;
    if (mbWereAllChallengesTwoPlayer != lbEffectiveAllTwoPlayer)
    {
        if (lbEffectiveAllTwoPlayer)
        {
            // Hack every entry's active player count to 2 (X360 rlwimi ,1,28,23 with rol 1 -> low nibble = 2).
            for (s32 liChallenge = 0; liChallenge < mpFreeburnChallengeList->GetChallengeCount(); ++liChallenge)
            {
                BrnResource::ChallengeListEntry* lpChallenge =
                    const_cast<BrnResource::ChallengeListEntry*>(mpFreeburnChallengeList->GetChallengeData(liChallenge));
                lpChallenge->SetNumPlayers(2);
            }
        }
        else
        {
            UnHackAllChallenges();
        }
        mbWereAllChallengesTwoPlayer =
            (!mbChallengesAreAllOnePlayer) && mbChallengesAreAllTwoPlayer;
    }

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // dwordE08 == 2
    {
        bool lbTimerNotFinished = false;   // r28; set when the countdown is still running

        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");             // X360 line 0x3E3

        const ::EActiveRaceCarIndex lePlayerActiveRaceCarIndex =
            lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex(); // inlined +10328 read + "index set" assert (h:980)
        CGS_ASSERT(lePlayerActiveRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                   "lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");      // X360 line 0x3E7
        CGS_ASSERT(lePlayerActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // X360 line 0x3E8

        if (UpdateTimer(lfTimeStep))
        {
            // 5th param (X360-routed SIM-50Hz predicate) flows into UpdateChallenge.
            UpdateChallenge(lfTimeStep, liFramesSinceNetworkStart, lpActiveRaceCarOutputInterface,
                            lpActionQueue, lbIsOnline);
            mabReceivedSuccessUpdates[lePlayerActiveRaceCarIndex] = true;  // stb 1,+0x590+idx
        }
        else
        {
            lbTimerNotFinished = true;
        }

        // 6th param (X360-routed online flag) gates the arbitration pass.
        if (lbIsTimerFrequency50Hz)
        {
            UpdateArbitration(lfTimeStep, lpActionQueue, lpActiveRaceCarOutputInterface, lbTimerNotFinished);
        }
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wB_15.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_15.cpp
// ----------------------------------------------------------------------------
// Wave-B partfile (group 15) for BrnGameState::ChallengeManager.
//
// Bodied here: UpdateAction (X360 0x8233B410).
//
// The other two group-15 giants -- UpdateChallenge (0x82347190) and ProcessEvent
// (0x8233D6A8) -- are reported blocked (see funcs_blocked / the final report):
//   * ProcessEvent switches on the raw X360 game-event discriminant and reads the
//     event payload off the untyped `const CgsModule::Event*` for a family of event
//     kinds (completed-stunt +0x68/+0x5C/+0x6C/+0x50/+0x54/+0x80..., in-progress-stunt,
//     near-miss, drift, boost-time-complete, action-success {CgsID@0, actionIndex@8},
//     challenge-reset {CgsID@0, action@8}, active-challenge {arci[]@0, id@0x20, count@0x28}).
//     None of these event-payload structs are defined in the frozen headers
//     (BrnGameEvents.h homes only the Fburn success / success-update, road-rules and
//     network events), so faithful named-member field access is impossible -- the reads
//     would have to be raw-offset casts (offset_hack lint failures).
//   * UpdateChallenge's body is dominated by runtime-value-formatted assert blocks
//     (BeginAssert -> StrStream -> operator<< of the banked score / challenge-id /
//     action-index -> AppendFormat -> FireAssert) that the CGS_ASSERT(cond,"literal")
//     convention cannot express, and it posts a 32-byte id-153 challenge-reset action
//     built on the stack whose 0x20 payload has no committed action struct.
//
// UpdateAction is a per-action state machine: it refreshes location/car/modifier gates,
// then dispatches on the action's EChallengeActionType, feeding the per-skill score into
// UpdateCurrentActionScore / GetCurrentSkillScore / IsSkillScoreCurrentlySuccessful, and
// finally recomputes the per-action remaining-target for the INDIVIDUAL / INDIVIDUAL_
// ACCUMULATION coop types. Returns the resulting EChallengeStatus (ONGOING / SUCCESS /
// RESET_IF_NEEDED).
//
// NOTE on IsSkillScoreCurrentlySuccessful (fixed in wave C): the header now declares the
// TRUE 7-param X360 shape (f1=lfScore, r5=lpAction, r6=lbIsCumulativeArbitration,
// r7=liTargetValueIndex, r8=liActionIndex, r9=leActiveRaceCarIndex,
// r10=lbLargerScoresAreBetter). Every callsite below passes the seven attested register
// values 1:1 (r6 always carries this caller's lbIsOnline; r7 is the GetTargetValue index).
// ============================================================================


namespace BrnGameState
{
    // X360 0x8233B410.
    EChallengeStatus ChallengeManager::UpdateAction(
        s32                                                                            liActionIndex,
        const BrnResource::ChallengeListEntryAction*                                   lpAction,
        f32                                                                            lfTimeStep,
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*   lpActiveRaceCarOutputInterface,
        bool                                                                           lbIsOnline)
    {
        typedef BrnResource::ChallengeListEntryAction Action;

        (void)lfTimeStep;   // consumed by the (blocked) CheckCurrentLocation path only

        CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface");
        CGS_ASSERT(lpAction, "lpAction");
        CGS_ASSERT(liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                   "liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");

        EChallengeStatus leChallengeStatus = E_CHALLENGE_STATUS_ONGOING;

        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
            lpActiveRaceCarOutputInterface->GetPlayerRaceCarState();
        CGS_ASSERT(lpRaceCarState, "lpRaceCarState");

        CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface");

        // Player active-race-car index (baked "Player car index hasn't been set" assert).
        const EActiveRaceCarIndex leActiveRaceCarIndex =
            lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex();

        if (!lbIsOnline)
        {
            maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] = 0.0f;
        }

        const bool lbWasLocationOk = mabIsLocationOk[liActionIndex];
        mabIsLocationOk[liActionIndex] = CheckCurrentLocation(lpAction, lpActiveRaceCarOutputInterface);
        const bool lbIsCarOk        = CheckCurrentCar(mpCurrentChallenge, lpActiveRaceCarOutputInterface);
        const bool lbAreModifiersOk = CheckForModifiers(lpAction, lpActiveRaceCarOutputInterface);
        const bool lbLocationChanged =
            UpdateLocationOKStatusChange(lbWasLocationOk, mabIsLocationOk[liActionIndex]);

        if ((((mabIsLocationOk[liActionIndex] || lbLocationChanged) && lbIsCarOk && lbAreModifiersOk)) || lbIsOnline)
        {
            // The X360 range assert here (`cmplwi 0x29` == 41, the drifted action-type count)
            // is the one INLINED from the committed ChallengeListEntryAction::GetActionType()
            // (ChallengeListEntry.h:318, whose X360-drift note records the 22-vs-41 threshold
            // call) -- do not emit a second explicit copy.
            const Action::EChallengeActionType leActionType = lpAction->GetActionType();

            const Action::EChallengeCoopType leCoopType = lpAction->GetCoopType();
            f32 lfCurrentSkillScore = 0.0f;

            switch (leActionType)
            {
            case Action::E_CHALLENGE_ACTION_MINIMUM_SPEED:               // 0
            {
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate minimum speed\n");

                f32 lfSpeed;
                if (lbIsOnline)
                {
                    CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                               "liActionIndex < mpCurrentChallenge->GetNumActions()");
                    lfSpeed = mafCumulativeActionScores[liActionIndex];
                }
                else
                {
                    lfSpeed = lpRaceCarState->mfMaxSpeedMPH;
                    CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                               "liActionIndex < mpCurrentChallenge->GetNumActions()");
                    maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] = lpRaceCarState->mfMaxSpeedMPH;
                }

                if (lfSpeed >= static_cast<f32>(lpAction->GetTargetValue(0)))
                {
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                else if ((leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
                          leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE) &&
                         lfSpeed > 0.0f && !lbIsOnline)
                {
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                break;
            }

            case Action::E_CHALLENGE_ACTION_IN_AIR:                      // 1 -> AIR
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_AIR, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_AIR, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_AIR_DISTANCE:                // 2 -> AIR_DISTANCE
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_AIR_DISTANCE, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_AIR_DISTANCE, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_LEAP_CARS:                   // 3 -> LEAP_CARS
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_LEAP_CARS, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_LEAP_CARS, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_DRIFT:                       // 4 -> DRIFT
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_DRIFT, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_DRIFT, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_NEAR_MISS:                   // 5 -> NEAR_MISS
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_NEAR_MISS, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_NEAR_MISS, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_BARREL_ROLLS:                // 6 -> BARREL_ROLL
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BARREL_ROLL, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BARREL_ROLL, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_ONCOMING:                    // 7 -> ONCOMING(0)
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ONCOMING, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ONCOMING, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_FLATSPIN:                    // 8 -> FLATSPIN
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_FLATSPIN, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_FLATSPIN, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_LAND_SUCCESSFUL:             // 9 -> SUCCESSFUL_LANDING
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_SUCCESSFUL_LANDING, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_SUCCESSFUL_LANDING, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                else if (mabActiveSkillThisFrame[E_FREEBURN_SKILL_SUCCESSFUL_LANDING] && lfCurrentSkillScore == 0.0f)
                    leChallengeStatus = E_CHALLENGE_STATUS_RESET_IF_NEEDED;
                break;

            case Action::E_CHALLENGE_ACTION_ROAD_RULE_TIME:             // 10 -> ROAD_RULE_TIME
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate road rule times\n");
                if (leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
                    leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE)
                    UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ROAD_RULE_TIME, leActiveRaceCarIndex, 1000.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ROAD_RULE_TIME, leCoopType, false, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, false))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_ROAD_RULE_CRASH:            // 11 -> ROAD_RULE_CRASH
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate road rule times\n");
                if (leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
                    leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE)
                    UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ROAD_RULE_CRASH, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_ROAD_RULE_CRASH, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_PLAYER_POWER_PARKING:       // 12 -> PLAYER_POWER_PARKING
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate power parking\n");
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_PLAYER_POWER_PARKING, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, (lbIsOnline ? 0 : 1), liActionIndex, leActiveRaceCarIndex, true))
                {
                    if (mabActiveSkillThisFrame[E_FREEBURN_SKILL_PLAYER_POWER_PARKING])
                    {
                        CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                                   "liActionIndex < mpCurrentChallenge->GetNumActions()");
                        maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] = 1.0f;
                    }
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                break;

            case Action::E_CHALLENGE_ACTION_TRAFFIC_POWER_PARKING:      // 13 -> TRAFFIC_POWER_PARKING
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate power parking\n");
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_TRAFFIC_POWER_PARKING, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, (lbIsOnline ? 0 : 1), liActionIndex, leActiveRaceCarIndex, true))
                {
                    if (mabActiveSkillThisFrame[E_FREEBURN_SKILL_TRAFFIC_POWER_PARKING])
                    {
                        CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                                   "liActionIndex < mpCurrentChallenge->GetNumActions()");
                        maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] = 1.0f;
                    }
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                break;

            case Action::E_CHALLENGE_ACTION_CRASH_INTO_PLAYER:          // 14
            {
                CGS_ASSERT(leCoopType != Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION,
                           "Can't currently accumulate crashing into player cars\n");

                s32 liCrashValue;
                if (lbIsOnline)
                {
                    CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                               "liActionIndex < mpCurrentChallenge->GetNumActions()");
                    liCrashValue = static_cast<s32>(mafCumulativeActionScores[liActionIndex]);
                }
                else
                {
                    liCrashValue = maiCrashedWithChallengePlayer[leActiveRaceCarIndex];
                    CGS_ASSERT(liActionIndex < mpCurrentChallenge->GetNumActions(),
                               "liActionIndex < mpCurrentChallenge->GetNumActions()");
                    maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex] =
                        static_cast<f32>(maiCrashedWithChallengePlayer[leActiveRaceCarIndex]);
                }

                if (liCrashValue >= lpAction->GetTargetValue(0))
                {
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                else if ((leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
                          leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE) &&
                         liCrashValue > 0 && !lbIsOnline)
                {
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                }
                break;
            }

            case Action::E_CHALLENGE_ACTION_BURNOUTS:                   // 15 -> BURNOUTS
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BURNOUTS, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BURNOUTS, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_MEET_UP:                    // 16 -- always succeeds when gated in
                leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_BILLBOARD:                  // 17 -> BILLBOARDS
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BILLBOARDS, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BILLBOARDS, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_BOOST_TIME:                 // 18 -> BOOST_TIME
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BOOST_TIME, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BOOST_TIME, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_BARREL_ROLLS_REVERSE:       // 19 -> BARREL_ROLL_REVERSE
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BARREL_ROLL_REVERSE, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_BARREL_ROLL_REVERSE, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_FLATSPIN_REVERSE:           // 20 -> FLATSPIN_REVERSE(2)
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_FLATSPIN_REVERSE, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_FLATSPIN_REVERSE, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case Action::E_CHALLENGE_ACTION_LAND_SUCCESSFUL_REVERSE:    // 21 -> SUCCESSFUL_LANDING_REVERSE(6)
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE, leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE, leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                else if (mabActiveSkillThisFrame[E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE] && lfCurrentSkillScore == 0.0f)
                    leChallengeStatus = E_CHALLENGE_STATUS_RESET_IF_NEEDED;
                break;

            // ---- X360-drift stunt-run action types (22..40) -> drifted skills (19..37). ----
            // These skill ids have no recoverable PS3 enumerator names; the X360 asm uses them as
            // plain numeric casts (see BrnChallengeManager.h EFreeburnSkill note), which is
            // reproduced verbatim here.
            case 22:   // -> drift skill 19; RESET_IF_NEEDED when the score was banked this frame at 0.
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(19), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(19), leCoopType, true, 1.0f);
                if (mabBankedSkillThisFrame[19] && lfCurrentSkillScore == 0.0f)
                    leChallengeStatus = E_CHALLENGE_STATUS_RESET_IF_NEEDED;
                else if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 23:   // -> drift skill 20; RESET_IF_NEEDED when the score was banked this frame.
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(20), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(20), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                else if (mabBankedSkillThisFrame[20])
                    leChallengeStatus = E_CHALLENGE_STATUS_RESET_IF_NEEDED;
                break;

            case 24:   // -> drift skill 21
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(21), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(21), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 25:   // -> drift skill 22
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(22), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(22), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 26:   // -> drift skill 23
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(23), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(23), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 27:   // -> drift skill 24
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(24), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(24), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 28:   // -> drift skill 25
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(25), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(25), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 29:   // -> drift skill 26
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(26), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(26), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 30:   // -> drift skill 27
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(27), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(27), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 31:   // -> drift skill 28
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(28), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(28), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 32:   // -> drift skill 29
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(29), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(29), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 33:   // -> drift skill 30
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(30), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(30), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 34:   // -> drift skill 31
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(31), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(31), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 35:   // -> drift skill 32
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(32), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(32), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 36:   // -> drift skill 33
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(33), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(33), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 37:   // -> drift skill 34
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(34), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(34), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 38:   // -> drift skill 35
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(35), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(35), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 39:   // -> drift skill 36
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(36), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(36), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            case 40:   // -> drift skill 37
                UpdateCurrentActionScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(37), leActiveRaceCarIndex, 1.0f);
                lfCurrentSkillScore = GetCurrentSkillScore(liActionIndex, lbIsOnline, static_cast<EFreeburnSkill>(37), leCoopType, true, 1.0f);
                if (IsSkillScoreCurrentlySuccessful(lfCurrentSkillScore, lpAction, lbIsOnline, 0, liActionIndex, leActiveRaceCarIndex, true))
                    leChallengeStatus = E_CHALLENGE_STATUS_SUCCESS;
                break;

            default:
                leChallengeStatus = E_CHALLENGE_STATUS_ONGOING;
                CGS_ASSERT(false, "Unknown challenge type.");
                break;
            }
        }

        // Recompute the per-action remaining target for the two individual coop types.
        const Action::EChallengeCoopType leTailCoopType = lpAction->GetCoopType();
        if (leTailCoopType == Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL)
        {
            s32 liRemaining = lpAction->GetTargetValue(0) -
                              static_cast<s32>(maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex]);
            if (liRemaining < 0)
                liRemaining = 0;
            maiRemainingTarget[liActionIndex] = liRemaining;
        }
        else if (leTailCoopType == Action::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION &&
                 leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)
        {
            s32 liRemaining = maiRemainingTarget[liActionIndex] -
                              static_cast<s32>(maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex]);
            if (liRemaining < 0)
                liRemaining = 0;
            maiRemainingTarget[liActionIndex] = liRemaining;
        }

        return leChallengeStatus;
    }
}

// ============================================================================
// FOLDED FROM BrnChallengeManager_wC_00.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_00.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-C partfile (group G1): the three
// E_ACTION_FREEBURN_CHALLENGE (id 153, 0x20-byte FreeburnChallengeAction) posters that drive
// the freeburn-challenge lifecycle from selection to results teardown.
//
//   BeginChallenge            (X360 0x823505B8)
//   TriggerFreeburnChallenge  (X360 0x82346DA0)
//   UpdateResults             (X360 0x82345FD0)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store, branch, early-out and
// assert; the raw offsets are mapped onto the keystone-frozen NAMED members/accessors of
// BrnChallengeManager.h and onto the frozen GameStateModuleIO::FreeburnChallengeAction payload
// in BrnGameActions.h (sizeof 0x20, offsets static_assert-pinned there). No header was edited.
//
// FreeburnChallengeAction::meEventType DRIFT (keystone pitfall F4): the values 0..3 match the
// committed BrnNetwork::BrnNetworkModuleIO::EChallengeEventType enumerators 1:1 and are written
// by name (BeginChallenge stores SELECTED, TriggerFreeburnChallenge stores TRIGGERED); the
// UpdateResults store is the RAW X360 value 6, for which the X360 enum carries an extra
// (unattested) enumerator ahead of ENDED -- it is deliberately NOT spelled with the PS3 names
// E_CHALLENGE_EVENT_ENDED(4) / E_CHALLENGE_EVENT_RESULTS_FINISHED(5).
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// BeginChallenge  (X360 0x823505B8)
// ----------------------------------------------------------------------------
// Arms a freeburn challenge: aborts whatever challenge is already live, resolves the challenge
// list entry for lChallengeID, optionally broadcasts the "challenge selected" action, then
// resets the whole per-challenge working set and parks the manager in PENDING.
//
// Register map (asm prologue): r3 this, r4 lChallengeID (64-bit CgsID, `mr r28,r4` + the later
// `std`), r5 lpActionQueue, r6 lpActiveRaceCarOutputInterface (NEVER read by this body -- it is
// only threaded through by the caller), r7 lbIsOnline, r8 lbRemote (the post gate).
void ChallengeManager::BeginChallenge(
    CgsID lChallengeID,
    TGameActionQueue* lpActionQueue,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    bool lbIsOnline,
    bool lbRemote)
{
    // X360 folds the two status compares into one bool in r11 (`cmpwi 2` / `cmpwi 3`), then
    // branches on it -- a plain short-circuit || in the source.
    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING ||
        meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RESULTS)
    {
        EndChallenge(E_CHALLENGE_STATUS_ABORTED, lpActionQueue, true);   // li r4,2 / li r6,1
    }

    // Resolve the entry: index first, and only fetch the entry when the id was found
    // (a miss leaves the cached challenge NULL -- `mr r29,r30` with r30 == 0).
    const BrnResource::ChallengeListEntry* lpChallenge = 0;
    const s32 liChallengeIndex = mpFreeburnChallengeList->GetChallengeIndex(lChallengeID);
    if (liChallengeIndex >= 0)
    {
        lpChallenge = mpFreeburnChallengeList->GetChallengeData(liChallengeIndex);
    }

    if (lbRemote)
    {
        // The posted id is the RESOLVED entry's id (`ld 0xC0(r29)`), not the parameter; the X360
        // does not null-check lpChallenge here.
        GameStateModuleIO::FreeburnChallengeAction lAction;
        lAction.mChallengeID                  = lpChallenge->GetChallengeID();
        lAction.meEventType                   = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_SELECTED;  // 0
        lAction.meChallengeStatus             = E_CHALLENGE_STATUS_ONGOING;                                  // 0
        lAction.miActionIndex                 = 0;
        lAction.miNumChallengesComplete       = -1;
        lAction.miTotalNumChallenges          = -1;
        lAction.mbIsHost                      = lbIsOnline;   // stb r26 (r7), +0x1C
        lAction.mbAbortingToStartNewChallenge = false;

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                                sizeof(GameStateModuleIO::FreeburnChallengeAction));
    }

    mpCurrentChallenge         = lpChallenge;                            // stw +0xE0C
    miCurrentChallengeAction   = 0;                                      // stw +0xE10
    miCurrentArbitrationIndex  = 0;                                      // stw +0xE14
    mbChallengeTimerRunning    = false;                                  // stb +0xE1C
    mbConvoyTimerRunning       = false;                                  // stb +0xE24
    meChallengeManagerStatus   = E_CHALLENGE_MANAGER_STATUS_PENDING;     // stw 1, +0xE08
    mbResultsTimerRunning      = false;                                  // stb +0xE2C
    mLastSecondSuccessStatus.UnSetAll();                                 // std 0, +0xE00

    ClearPlayerSuccessData();
    ResetCurrentChallengeData(miCurrentChallengeAction);                 // lwz r4, +0xE10

    // Leap-car tracking reset (flt_820037C8 == -1.0f, the "timer invalid" sentinel).
    mbUpdateLeaptCars     = false;   // stb +0x118
    miNumCarsLeapt        = 0;       // stw +0x114
    mfLeapCarsValidTimer  = -1.0f;   // stfs +0x110
}

// ----------------------------------------------------------------------------
// TriggerFreeburnChallenge  (X360 0x82346DA0)
// ----------------------------------------------------------------------------
// Takes the armed (PENDING) challenge live: flags every finalised remote player and the local
// player as challenge participants, broadcasts the "challenge triggered" action, and caches
// whether any of the challenge's actions needs the leap-car tracker.
//
// Register map: r3 this, r4 lChallengeID (64-bit CgsID), r5 lpActionQueue, r6 lbRemote.
void ChallengeManager::TriggerFreeburnChallenge(CgsID lChallengeID, TGameActionQueue* lpActionQueue,
                                                bool lbRemote)
{
    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_PENDING)   // lwz +0xE08; cmpwi 1
    {
        // Runtime-valued diagnostic (the X360 streams the status into the assert message buffer
        // through the global StrStream at off_82000D08); reproduced with the committed
        // local-StrStream idiom. X360 file/line: BrnChallengeManager.cpp:2098.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Status is " << static_cast<s32>(meChallengeManagerStatus);
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    meChallengeManagerStatus = E_CHALLENGE_MANAGER_STATUS_RUNNING;   // stw 2, +0xE08
    CGS_ASSERT(mpGameStateModule, "mpGameStateModule");              // :2102

    // Every finalised remote slot's player joins the challenge. The X360 walks the slots by their
    // +0x100 id field (this+0x6C8, stride 0x108) and reads the mbFinalised byte right after it.
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        const ChallengeCompletionData& lSlot = maChallengeCompletionData[liSlot];
        if (lSlot.mNetworkPlayerID != -1 && lSlot.mbFinalised)     // -1 == free slot
        {
            const ::EActiveRaceCarIndex leActiveRaceCarIndex =
                mpGameStateModule->GetActiveRaceCarIndex(lSlot.mNetworkPlayerID);

            // The two-sided range test the compiler folded into a single unsigned `cmplwi 7`
            // (E_ACTIVE_RACE_CAR_INDEX_INVALID == -1 fails it as a large unsigned).
            if (leActiveRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0 &&
                leActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT)
            {
                mabPlayerStartedChallenge[leActiveRaceCarIndex] = true;
            }
        }
    }

    // Both bound asserts re-call the accessor (three calls in total in the asm).
    CGS_ASSERT(mpGameStateModule->GetPlayerActiveRaceCarIndex() >= ::E_ACTIVE_RACE_CAR_INDEX_0,
               "mpGameStateModule->GetPlayerActiveRaceCarIndex() >= E_ACTIVE_RACE_CAR_INDEX_0");   // :2120
    CGS_ASSERT(mpGameStateModule->GetPlayerActiveRaceCarIndex() < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mpGameStateModule->GetPlayerActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT");  // :2121

    mabPlayerStartedChallenge[mpGameStateModule->GetPlayerActiveRaceCarIndex()] = true;

    // Posted id is the PARAMETER here (`std r26` == the r4 CgsID), not the cached entry's.
    GameStateModuleIO::FreeburnChallengeAction lAction;
    lAction.mChallengeID                  = lChallengeID;
    lAction.meEventType                   = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_TRIGGERED;  // 1
    lAction.meChallengeStatus             = E_CHALLENGE_STATUS_ONGOING;                                   // 0
    lAction.miActionIndex                 = 0;
    lAction.miNumChallengesComplete       = -1;
    lAction.miTotalNumChallenges          = -1;
    lAction.mbIsHost                      = lbRemote;   // stb r24 (r6), +0x1C
    lAction.mbAbortingToStartNewChallenge = false;

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                            sizeof(GameStateModuleIO::FreeburnChallengeAction));

    CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :2137

    // Leap-car tracking is only run when one of the challenge's actions is LEAP_CARS.
    // GetAction inlines its own index guards (ChallengeListEntry.h:941/942) and GetActionType
    // inlines its type-range guard (:660) -- neither is duplicated here.
    mbChallengeRequiresLeapCars = false;   // stb 0, +0xE34
    for (s32 liActionIndex = 0; liActionIndex < mpCurrentChallenge->GetNumActions(); ++liActionIndex)
    {
        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction(liActionIndex);
        CGS_ASSERT(lpAction, "lpAction");   // :2144

        if (lpAction->GetActionType() ==
            BrnResource::ChallengeListEntryAction::E_CHALLENGE_ACTION_LEAP_CARS)   // == 3
        {
            mbChallengeRequiresLeapCars = true;
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateResults  (X360 0x82345FD0)
// ----------------------------------------------------------------------------
// Runs the post-challenge results hold. While the results timer is still counting the manager
// just idles; when it expires the final "results finished" action goes out and the whole
// challenge working set is torn down back to NONE.
//
// Register map: r3 this, f1 lfTimeStep (its r4 slot is shadowed and unused), r5 lpActionQueue.
void ChallengeManager::UpdateResults(f32 lfTimeStep, TGameActionQueue* lpActionQueue)
{
    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_RESULTS)   // lwz +0xE08; cmpwi 3
    {
        return;
    }

    if (UpdateResultsTimer(lfTimeStep))   // still running -> nothing to do this frame
    {
        return;
    }

    GameStateModuleIO::FreeburnChallengeAction lAction;
    lAction.mChallengeID                  = mpCurrentChallenge->GetChallengeID();   // ld 0xC0
    // RAW X360-drift event value (see the file header note): NOT the PS3 enumerator names.
    lAction.meEventType                   = 6;
    lAction.meChallengeStatus             = meLocalChallengeStatus;                 // lwz +0xE30
    lAction.miActionIndex                 = 0;
    lAction.miNumChallengesComplete       = -1;
    lAction.miTotalNumChallenges          = -1;
    lAction.mbIsHost                      = false;
    lAction.mbAbortingToStartNewChallenge = false;

    CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :884
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                            sizeof(GameStateModuleIO::FreeburnChallengeAction));

    meChallengeManagerStatus  = E_CHALLENGE_MANAGER_STATUS_NONE;   // stw 0, +0xE08
    mpCurrentChallenge        = 0;                                 // stw 0, +0xE0C
    miCurrentChallengeAction  = 0;                                 // stw 0, +0xE10
    miCurrentArbitrationIndex = 0;                                 // stw 0, +0xE14
    // The COUNT sentinel doubles as "no local status" (BurnoutConstants.h note).
    meLocalChallengeStatus    = E_CHALLENGE_STATUS_COUNT;          // stw 7, +0xE30
    mLastSecondSuccessStatus.UnSetAll();                           // std 0, +0xE00

    ClearPlayerSuccessData();
}

}

// ============================================================================
// FOLDED FROM BrnChallengeManager_wC_01.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_01.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-C partfile (group G2): the challenge COMPLETION +
// ARBITRATION spine. The three bodies call each other (both arbitration paths terminate in
// EndChallenge), so they live in one partfile.
//
//   EndChallenge             (X360 0x8234DE30, keystone row 4)
//   UpdateArbitration        (X360 0x82351AF8, keystone row 5)
//   UpdateArbitrationSuccess (X360 0x82350340, keystone row 6)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store, branch, early-out and
// assert; the raw offsets are mapped onto the keystone-frozen NAMED members/accessors of
// BrnChallengeManager.h and onto the frozen GameStateModuleIO action payloads in
// BrnGameActions.h (FreeburnChallengeAction sizeof 0x20 / FburnChallengeShowSelectorAction
// sizeof 8, offsets static_assert-pinned there). NO header was edited.
//
// Conventions carried from the keystone spec:
//  * FreeburnChallengeAction::meEventType -- values 0..3 match the committed
//    BrnNetwork::BrnNetworkModuleIO::EChallengeEventType enumerators 1:1 and are written by
//    name (UpdateArbitration stores RESET, UpdateArbitrationSuccess stores ACTION_SUCCESS);
//    EndChallenge's store is the RAW X360-drift value 5 -- the X360 enum carries one extra
//    (unattested) enumerator ahead of ENDED, so it is deliberately NOT spelled
//    E_CHALLENGE_EVENT_ENDED(4).
//  * The "Index N is out of range (max bits: 2000)" (CgsFastBitArray.h:431) and
//    "Index: N, Number of bits: 8" (CgsBitArray.h:222) StrStream blobs in the asm are the
//    INLINED container range asserts -- the committed container methods are called instead of
//    reproducing them.
//  * The per-index `leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT` /
//    `leEnumIndex <= E_FREEBURN_SKILL_COUNT` asserts are the inlined enum operator++ guards
//    (BurnoutConstants.h:39 / BrnChallengeManager.h:113) -- the committed operators are used.
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// EndChallenge  (X360 0x8234DE30)
// ----------------------------------------------------------------------------
// Tears the live freeburn challenge down with the supplied outcome. On SUCCESS it also banks
// the completion in the player profile (first time only -- which additionally feeds the
// achievement counter and the online-unlock re-scan), records the challenge in the local
// completion bit store, mirrors that into the remote players' slots, broadcasts the
// every-player status event, posts the unnamed id-55 notification and marks every
// participating player DONE on both action slots. In ALL cases it posts the id-153 lifecycle
// action and parks the manager in RESULTS with the leap-car tracking state cleared.
//
// Register map (asm prologue): r3 this, r4 leChallengeStatus, r5 lpActionQueue (the assert
// string still spells the parameter "lpOutput"), r6 lbIsOnline.
void ChallengeManager::EndChallenge(EChallengeStatus leChallengeStatus,
                                    TGameActionQueue* lpActionQueue,
                                    bool lbIsOnline)
{
    // X360 inlines IsChallengeActive() as `meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE`
    // (lwz +0xE08; cmpwi 0) -- the assert text is the source expression.
    CGS_ASSERT(IsChallengeActive(), "IsChallengeActive()");   // :2239
    CGS_ASSERT(lpActionQueue, "lpOutput");                    // :2240

    const CgsID lActiveChallengeID = mpCurrentChallenge->GetChallengeID();   // lwz +0xE0C; ld +0xC0
    CGS_ASSERT(lActiveChallengeID != 0, "lActiveChallengeID != 0");          // :2246

    meLocalChallengeStatus = leChallengeStatus;   // stw +0xE30

    if (leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)   // X360 folds `status - 1 == 0` into a bool
    {
        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :2258
        CGS_ASSERT(mpProgression, "mpProgression");             // :2259

        // First completion only: bank it in the profile, tell the achievement manager the new
        // completed-challenge count and re-scan the online unlocks.
        if (!mpProgression->GetProfile()->HasPlayerCompletedFreeburnChallenge(lActiveChallengeID))
        {
            const u32 luCompletedChallengeCount =
                mpProgression->GetProfile()->CompleteFreeburnChallenge(lActiveChallengeID);
            mpProgression->GetAchievementManager()->OnFreeburnChallengeComplete(luCompletedChallengeCount);
            CheckForOnlineChallengeUnlocks();
        }

        const s32 liChallengeIndex = GetChallengeIndex(lActiveChallengeID);
        if (liChallengeIndex >= 0)
        {
            // Inlined FastBitArray<2000>::SetBit (its "Index N is out of range (max bits: 2000)"
            // StrStream blob at CgsFastBitArray.h:431 is that container's own range assert).
            mLocalChallengeCompletionData.SetBit(static_cast<u32>(liChallengeIndex));
            SetRemotePlayersChallengeCompleted(liChallengeIndex);
            OutputFreeburnChallengeEveryPlayerStatusEvent(lpActionQueue);
        }
        else
        {
            CGS_ASSERT(liChallengeIndex >= 0, "liChallengeIndex >= 0");   // :2268
        }

        // X360 posts a single ZEROED byte under the RAW action id 55. No committed
        // EGameActionType enumerator carries that value, so the raw s32 id is kept here
        // (AddEvent's type parameter is a plain s32).
        const u8 luChallengeCompleteActionPayload = 0;   // stb 0, sp+var_B0
        lpActionQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&luChallengeCompleteActionPayload), 55, 1);

        // Every player that took part finishes DONE on both action slots (stw 3, +0x00/+0x04
        // over the 8-byte per-player stride of maaePlayersSuccessStatus).
        for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
             static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
             lePlayer++)
        {
            if (mabPlayerStartedChallenge[lePlayer])
            {
                maaePlayersSuccessStatus[lePlayer][0] = GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
                maaePlayersSuccessStatus[lePlayer][1] = GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
            }
        }
    }

    GameStateModuleIO::FreeburnChallengeAction lChallengeAction;
    lChallengeAction.mChallengeID = lActiveChallengeID;                       // std sp+var_90
    // RAW X360-drift event value (see the file header note): NOT E_CHALLENGE_EVENT_ENDED(4).
    lChallengeAction.meEventType                   = 5;                      // stw sp+var_88
    lChallengeAction.meChallengeStatus             = leChallengeStatus;      // stw sp+var_84
    lChallengeAction.miActionIndex                 = 0;                      // stw sp+var_80
    lChallengeAction.miNumChallengesComplete       = CountCompletedChallenges();               // stw sp+var_7C
    lChallengeAction.miTotalNumChallenges          = mpFreeburnChallengeList->GetChallengeCount(); // lwz +0x32E0; stw sp+var_78
    lChallengeAction.mbIsHost                      = false;                  // stb 0, sp+var_74
    lChallengeAction.mbAbortingToStartNewChallenge = lbIsOnline;             // stb r20, sp+var_73 (attested store)
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChallengeAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                            sizeof(GameStateModuleIO::FreeburnChallengeAction));   // li r5,0x99; li r6,0x20

    mbChallengeTimerRunning     = false;                                   // stb 0, +0xE1C
    meChallengeManagerStatus    = E_CHALLENGE_MANAGER_STATUS_RESULTS;      // stw 3, +0xE08
    mbChallengeRequiresLeapCars = false;                                   // stb 0, +0xE34
    mbUpdateLeaptCars           = false;                                   // stb 0, +0x118
    miNumCarsLeapt              = 0;                                       // stw 0, +0x114
    mfLeapCarsValidTimer        = -1.0f;                                   // stfs flt_820037C8, +0x110

    // Drop every cached location-enter skill value (stfs 0.0 over the 38-entry +0xF34 run).
    for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START;
         static_cast<s32>(leSkill) < KI_FREEBURN_SKILL_COUNT_X360;
         leSkill++)
    {
        mafCachedActiveSkillValueOnLocationEnter[leSkill] = 0.0f;
    }
}

// ----------------------------------------------------------------------------
// UpdateArbitration  (X360 0x82351AF8)
// ----------------------------------------------------------------------------
// The per-frame arbitration pass UpdateRunning runs after the challenge update. It first
// handles the two global exits (the challenge action ran out of time; the convoy/meet-up
// window closed), then walks the challenge's actions from miCurrentArbitrationIndex and, per
// action co-op type, decides whether the action's success condition is now met -- handing the
// accepted action to UpdateArbitrationSuccess, which may end the whole challenge.
//
// Register map (asm prologue): r3 this, f1 lfTimeStep, r5 lpActionQueue, r6
// lpActiveRaceCarOutputInterface (spilled; only threaded into UpdateAction), r7 the declared
// lbIsOnline slot. X360 SLOT NOTE (same class of drift as UpdateRunning/PostWorldUpdate): the
// committed caller UpdateRunning passes its "the challenge action timer expired" flag in this
// slot, and this body uses it as that gate; the declared parameter name is kept.
void ChallengeManager::UpdateArbitration(
    f32                                                                          lfTimeStep,
    TGameActionQueue*                                                            lpActionQueue,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    bool                                                                         lbIsOnline)
{
    if (lbIsOnline)
    {
        // Time is up: bounce the player back to the challenge selector and fail the challenge.
        GameStateModuleIO::FburnChallengeShowSelectorAction lShowSelectorAction;
        lShowSelectorAction.mChallengeID = mpCurrentChallenge->GetChallengeID();   // lwz +0xE0C; ld +0xC0
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowSelectorAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR,
                                sizeof(GameStateModuleIO::FburnChallengeShowSelectorAction));  // li r5,0xA0; li r6,8
        EndChallenge(E_CHALLENGE_STATUS_FAILURE, lpActionQueue, false);            // li r4,6; li r6,0
        return;
    }

    // Inlined convoy countdown (the UpdateConvoyTimer shape: no arming -- the convoy timer is
    // armed by UpdateActionSuccess -- just the fsel clamp-at-zero decrement and the expiry test
    // against the SAME once-loaded running flag).
    if (mbConvoyTimerRunning && mfConvoyTimer > 0.0f)   // lbz +0xE24; lfs +0xE20
    {
        // f0 = timer - dt; fsel(-(timer-dt), 0.0, f0) clamps the countdown at 0.0.
        const f32 lfNewConvoyTimer = mfConvoyTimer - lfTimeStep;
        mfConvoyTimer = (lfNewConvoyTimer >= 0.0f) ? lfNewConvoyTimer : 0.0f;   // stfs +0xE20
    }

    if (mbConvoyTimerRunning && mfConvoyTimer <= 0.0f)
    {
        // The convoy/meet-up window closed without the group forming: rewind the challenge to
        // the arbitration action and tell everyone it was reset.
        GameStateModuleIO::FreeburnChallengeAction lResetAction;
        lResetAction.mChallengeID                  = mpCurrentChallenge->GetChallengeID();  // std sp+var_D0
        lResetAction.meEventType                   = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_RESET;  // stw 3, sp+var_C8
        lResetAction.meChallengeStatus             = E_CHALLENGE_STATUS_ONGOING;            // stw 0, sp+var_C8+4
        lResetAction.miActionIndex                 = miCurrentArbitrationIndex;             // lwz +0xE14; stw sp+var_C0
        lResetAction.miNumChallengesComplete       = -1;                                    // stw -1, sp+var_BC
        lResetAction.miTotalNumChallenges          = -1;                                    // stw -1, sp+var_B8
        lResetAction.mbIsHost                      = true;                                  // stb 1, sp+var_B4
        lResetAction.mbAbortingToStartNewChallenge = false;                                 // stb 0, sp+var_B3

        miCurrentChallengeAction = miCurrentArbitrationIndex;   // stw +0xE10 (before the post)

        CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :1349
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                                sizeof(GameStateModuleIO::FreeburnChallengeAction));   // li r5,0x99; li r6,0x20

        mbConvoyTimerRunning = false;                          // stb 0, +0xE24
        ResetCurrentChallengeData(miCurrentChallengeAction);   // lwz +0xE10
        return;
    }

    CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1363

    for (s32 liActionIndex = miCurrentArbitrationIndex;                 // lwz +0xE14
         liActionIndex < mpCurrentChallenge->GetNumActions();           // lbz +0xD4
         ++liActionIndex)
    {
        const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liActionIndex);
        const BrnResource::ChallengeListEntryAction::EChallengeCoopType leCoopType = lpAction->GetCoopType();  // lbz +0x01

        switch (leCoopType)   // X360 jump table, cases 0..6 (case 5 falls to the default)
        {
            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_ONCE:   // 0
            {
                // Anyone succeeding is enough.
                if (GetNumPlayerSucceeding(liActionIndex) > 0)
                {
                    if (UpdateArbitrationSuccess(liActionIndex, lpActionQueue))
                    {
                        return;
                    }
                }
                break;
            }

            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:               // 1
            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION:  // 2
            {
                // Everyone in the challenge has to be succeeding, and the per-action success
                // update must not already have gone out.
                const s32 liNumPlayerSucceeding = GetNumPlayerSucceeding(liActionIndex);
                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1508
                if (liNumPlayerSucceeding == mpCurrentChallenge->GetNumPlayers() &&   // lbz +0xD3 & 0xF
                    !mabIndividualActionsSuccessUpdateSent[liActionIndex])            // lbz +0xFDA
                {
                    if (UpdateArbitrationSuccess(liActionIndex, lpActionQueue))
                    {
                        return;
                    }
                }
                break;
            }

            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_SIMULTANEOUS:   // 3
            {
                // Everyone has reported in this frame: intersect the per-player success windows
                // and accept the action if there is any frame where every participant succeeded
                // at the same time.
                if (!ReceivedSuccessUpdatesFromAllPlayers())
                {
                    break;
                }

                // 0x82351D9C `mr r3, r24` re-seeds the accepted flag with r24 == 0 once the
                // gate has passed, so an empty intersection keeps scanning the later actions
                // this frame rather than returning.
                bool lbArbitrationAccepted = false;

                // X360 keeps the intersection in two stack qwords: cleared, then bits 0..119 set
                // (the FastBitArray<120> SetAll), then ANDed with each participant's window.
                CgsContainers::FastBitArray<120> lCommonSuccessWindow;
                lCommonSuccessWindow.UnSetAll();
                lCommonSuccessWindow.SetAll();
                for (s32 liPlayer = 0; liPlayer < KI_MAX_CHALLENGE_PLAYERS; ++liPlayer)
                {
                    if (mabPlayerStartedChallenge[liPlayer])   // lbz +0x598
                    {
                        // Per-field AND of maPlayerSuccessUpdateArray[liPlayer] into the
                        // intersection, expressed through the container's public bit API.
                        for (u32 luBit = 0; luBit < lCommonSuccessWindow.GetCapacity(); ++luBit)
                        {
                            if (!maPlayerSuccessUpdateArray[liPlayer].IsBitSet(luBit))
                            {
                                lCommonSuccessWindow.UnSetBit(luBit);
                            }
                        }
                    }
                }

                // X360 tests the two fields for "all zero"; a surviving bit accepts the action.
                if (lCommonSuccessWindow.GetFirstBitSet() !=
                    CgsContainers::FastBitArray<120>::KI_INVALID_BIT_INDEX)
                {
                    lbArbitrationAccepted = UpdateArbitrationSuccess(liActionIndex, lpActionQueue);
                }

                // Start the next reporting round regardless (stb 0 over the 8-byte run @+0x590).
                for (s32 liPlayer = 0; liPlayer < KI_MAX_CHALLENGE_PLAYERS; ++liPlayer)
                {
                    mabReceivedSuccessUpdates[liPlayer] = false;
                }

                if (lbArbitrationAccepted)
                {
                    return;
                }
                break;
            }

            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE:   // 4
            {
                CGS_ASSERT(liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                           "liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");   // :1449

                // Fold this frame's per-player scores into the cumulative total and clear them.
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
                     lePlayer++)
                {
                    mafCumulativeActionScores[liActionIndex] += maafCurrentActionsScores[lePlayer][liActionIndex];
                    maafCurrentActionsScores[lePlayer][liActionIndex] = 0.0f;
                }

                const s32 liNumPlayersContributing = GetNumPlayersContributing(liActionIndex);
                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1464
                if (liNumPlayersContributing != mpCurrentChallenge->GetNumPlayers())   // lbz +0xD3 & 0xF
                {
                    break;
                }

                // Re-run the action against the pooled total; only a SUCCESS accepts it.
                if (UpdateAction(liActionIndex, mpCurrentChallenge->GetAction(liActionIndex), lfTimeStep,
                                 lpActiveRaceCarOutputInterface, true) != E_CHALLENGE_STATUS_SUCCESS)
                {
                    break;
                }

                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
                     lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer])
                    {
                        maaePlayersSuccessStatus[lePlayer][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;   // stw 3
                    }
                }

                if (UpdateArbitrationSuccess(liActionIndex, lpActionQueue))
                {
                    return;
                }
                break;
            }

            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT:   // 6 (a real X360 co-op type)
            {
                // Ranked co-op: every participant's cumulative contribution is a 1-based rank.
                // The action is accepted once the used ranks form a gap-free run covering all
                // participants (the find-first-CLEAR-bit scan over the rank scratch).
                const s32 liNumPlayersContributing = GetNumPlayersContributing(liActionIndex);
                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1529
                if (liNumPlayersContributing != mpCurrentChallenge->GetNumPlayers())   // lbz +0xD3 & 0xF
                {
                    break;
                }

                CgsContainers::BitArray<8> lRanksUsed;
                lRanksUsed.UnSetAll();   // std 0, sp+var_100
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
                     lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer])
                    {
                        // fctiwz/stfiwx == the float->s32 truncation of the banked contribution.
                        const s32 liScore =
                            static_cast<s32>(maafCumulativeContributions[lePlayer][liActionIndex]) - 1;
                        CGS_ASSERT(liScore >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                                   "liScore >= E_ACTIVE_RACE_CAR_INDEX_0");     // :1545
                        CGS_ASSERT(liScore < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                                   "liScore < E_ACTIVE_RACE_CAR_INDEX_COUNT");  // :1546
                        // Inlined BitArray<8>::SetBit ("Index: N, Number of bits: 8" blob
                        // @CgsBitArray.h:222 is that container's own range assert).
                        lRanksUsed.SetBit(static_cast<u32>(liScore));
                    }
                }

                // X360 inlines the find-first-clear-bit scan TWICE (once per side of the &&).
                if (lRanksUsed.GetFirstClearBit() < mpCurrentChallenge->GetNumPlayers() &&
                    lRanksUsed.GetFirstClearBit() != CgsContainers::BitArray<8>::KI_INVALID_BITINDEX)
                {
                    break;
                }

                if (mabIndividualActionsSuccessUpdateSent[liActionIndex])   // lbz +0xFDA
                {
                    break;
                }

                if (UpdateArbitrationSuccess(liActionIndex, lpActionQueue))
                {
                    return;
                }
                break;
            }

            default:
            {
                // X360 streams the co-op type into the assert message buffer (:1568).
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Can't deal with challenges coop type: "
                           << static_cast<s32>(leCoopType)
                           << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
                break;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateArbitrationSuccess  (X360 0x82350340)
// ----------------------------------------------------------------------------
// Accepts one challenge action: latches its "success update sent" flag, then decides whether
// the whole challenge is now finished. For a CHAIN-terminating action (the X360 co-op/combine
// value 5) that means every participant is DONE on every action; for anything else it means
// this was the last action. If the challenge is finished it shows the selector again and ends
// the challenge with SUCCESS (returning true). Otherwise -- unless the action is INDEPENDENT --
// it broadcasts the id-153 "action success" and advances the arbitration/current action index.
//
// Register map (asm prologue): r3 this, r4 liActionIndex, r5 lpActionQueue.
bool ChallengeManager::UpdateArbitrationSuccess(s32 liActionIndex, TGameActionQueue* lpActionQueue)
{
    CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1595

    const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liActionIndex);
    CGS_ASSERT(lpAction, "lpAction");   // :1597

    mabIndividualActionsSuccessUpdateSent[liActionIndex] = true;   // stb 1, +0xFDA

    bool lbChallengeComplete;
    if (lpAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT)  // lbz +0x03 == 5
    {
        lbChallengeComplete = true;
        if (mpCurrentChallenge->GetNumActions() != 0)   // lbz +0xD4
        {
            s32 liAction = 0;
            do
            {
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
                     lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer] &&                     // lbz +0x598
                        maaePlayersSuccessStatus[lePlayer][liAction] !=            // lwz +0x428
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)  // cmpwi 3
                    {
                        lbChallengeComplete = false;
                        break;
                    }
                }
                if (!lbChallengeComplete)
                {
                    break;
                }
                ++liAction;
            }
            while (liAction < mpCurrentChallenge->GetNumActions());
        }
    }
    else
    {
        lbChallengeComplete = (mpCurrentChallenge->GetNumActions() - 1 == liActionIndex);
    }

    if (lbChallengeComplete)
    {
        GameStateModuleIO::FburnChallengeShowSelectorAction lShowSelectorAction;
        lShowSelectorAction.mChallengeID = mpCurrentChallenge->GetChallengeID();   // ld +0xC0
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowSelectorAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR,
                                sizeof(GameStateModuleIO::FburnChallengeShowSelectorAction));  // li r5,0xA0; li r6,8
        EndChallenge(E_CHALLENGE_STATUS_SUCCESS, lpActionQueue, false);            // li r4,1; li r6,0
        return true;
    }

    if (lpAction->GetCombineAction() != BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT)  // lbz +0x03 == 4
    {
        GameStateModuleIO::FreeburnChallengeAction lSuccessAction;
        lSuccessAction.mChallengeID                  = mpCurrentChallenge->GetChallengeID();  // std sp+var_A0
        lSuccessAction.meEventType                   = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_ACTION_SUCCESS;  // stw 2, sp+var_98
        lSuccessAction.meChallengeStatus             = E_CHALLENGE_STATUS_ONGOING;   // stw 0, sp+var_94
        lSuccessAction.miActionIndex                 = liActionIndex;                // stw sp+var_90
        lSuccessAction.miNumChallengesComplete       = -1;                           // stw -1, sp+var_8C
        lSuccessAction.miTotalNumChallenges          = -1;                           // stw -1, sp+var_88
        lSuccessAction.mbIsHost                      = true;                         // stb 1, sp+var_84
        lSuccessAction.mbAbortingToStartNewChallenge = false;                        // stb 0, sp+var_83

        CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :1670
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSuccessAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                                sizeof(GameStateModuleIO::FreeburnChallengeAction));   // li r5,0x99; li r6,0x20

        if (lpAction->GetCombineAction() != BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT)  // lbz +0x03 == 5
        {
            // Advance the arbitration index, clamped to the last action; drag the current
            // challenge action along with it when it has fallen behind (X360 stores the raw
            // index to +0xE14 first, then re-stores the clamped one).
            s32 liNextActionIndex = liActionIndex + 1;
            miCurrentArbitrationIndex = liNextActionIndex;                            // stw +0xE14 (raw)
            const s32 liLastActionIndex = mpCurrentChallenge->GetNumActions() - 1;    // lbz +0xD4; -1
            if (liNextActionIndex >= liLastActionIndex)
            {
                liNextActionIndex = liLastActionIndex;
            }
            miCurrentArbitrationIndex = liNextActionIndex;                            // stw +0xE14 (clamped)

            if (liNextActionIndex > miCurrentChallengeAction)                         // lwz +0xE10; cmpw
            {
                if (liNextActionIndex >= liLastActionIndex)
                {
                    liNextActionIndex = liLastActionIndex;
                }
                miCurrentChallengeAction = liNextActionIndex;                         // stw +0xE10
            }
        }
    }

    return false;
}

}   // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wC_02.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_02.cpp
// ============================================================================
// Wave-C partfile (group G3) for BrnGameState::ChallengeManager -- the network-player
// book-keeping trio that the wave-B pass had to leave blocked (the id-156/160/161 action
// payload structs and GameStateModule::GetActiveRaceCarIndex are now declared in the frozen
// headers). Store-for-store reconstruction over the NAMED members declared in
// BrnChallengeManager.h / BrnGameActions.h / BrnGameStateModule.h:
//
//   NetworkPlayerFinalised            @ 0x82347E88  (DWARF :248)
//   NetworkPlayerRemoved              @ 0x8234E420  (DWARF :255)
//   SetRemotePlayersChallengeCompleted@ 0x82323DF8  (DWARF :7xx private helper)
//
// The maChallengeCompletionData slot layout (stride 0x108: FastBitArray<2000> bit store
// @+0x00, NetworkPlayerID @+0x100 where -1 == free, mbFinalised @+0x104) is the committed
// Construct/NetworkPlayerAdded shape (BrnChallengeManager.cpp / _wB_09.cpp).
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// NetworkPlayerFinalised -- X360 0x82347E88. A remote player's join has completed: mark that
// player's completion slot finalised, send that player OUR completion bit store (id-156), and
// -- when online and a challenge is actually up -- tell them which challenge is active and who
// is in it (id-161).
//
// The slot scan is the same stride-0x108 walk over maChallengeCompletionData the sibling
// NetworkPlayerAdded uses; a miss leaves lpEntry NULL and fires the NON-GATING "lpEntry"
// assert (X360 :4958) -- the body then proceeds to the store exactly as the asm does (the
// element-address test `r30 != 0` after a hit is never false for a member array element).
// ----------------------------------------------------------------------------
void ChallengeManager::NetworkPlayerFinalised(BrnNetwork::NetworkPlayerID lPlayerID,
                                              TGameActionQueue* lpActionQueue,
                                              bool lbIsOnline)
{
    ChallengeCompletionData* lpEntry = 0;   // X360 li r30, 0
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID == lPlayerID)   // X360 lwz slot+0x100; cmpw
        {
            lpEntry = &maChallengeCompletionData[liSlot];
            break;
        }
    }
    CGS_ASSERT(lpEntry != 0, "lpEntry");   // X360 :4958 -- non-gating

    lpEntry->mbFinalised = true;           // X360 stb 1, 0x104(r30)

    // id-156: hand this player our own completion bit store. The X360 emits a plain
    // memcpy(&action, &mLocalChallengeCompletionData, 0x100) -- the action's
    // GameStateModuleIO::CompletedFburnChallenges field and the manager's
    // CgsContainers::FastBitArray<2000> member are the identical 256-byte bit store
    // (same bridge the committed UpdateRemotePlayerSuccessStatus uses).
    GameStateModuleIO::FburnChallengeStatusAction lStatusAction;
    std::memcpy(&lStatusAction.mCompletedChallenges, &mLocalChallengeCompletionData, 256);
    lStatusAction.mPlayerID = lPlayerID;    // X360 stw r29, sp+0x180 (action+0x100)

    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue");   // X360 :4964
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStatusAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_COMPLETION_STATUS,
                            sizeof(GameStateModuleIO::FburnChallengeStatusAction));   // X360 li r5,0x9C; li r6,0x108

    if (lbIsOnline)
    {
        // X360 materialises (status == 2 || status == 3) into a byte, then branches on it.
        if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING ||   // X360 cmpwi 2
            meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RESULTS)     // X360 cmpwi 3
        {
            CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");   // X360 :4974

            GameStateModuleIO::ActiveFburnChallengeAction lActiveChallengeAction;
            lActiveChallengeAction.mChallengeID            = mpCurrentChallenge->GetChallengeID(); // X360 ld +0xC0
            lActiveChallengeAction.mPlayerToSendToID       = lPlayerID;                            // X360 stw sp+0x74
            lActiveChallengeAction.miNumPlayersInChallenge = 0;                                    // X360 stw sp+0x78

            for (EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
                 (s32)leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
                 leActiveRaceCarIndex++)   // X360 op++ carries the BurnoutConstants.h:39 range assert
            {
                if (mabPlayerStartedChallenge[leActiveRaceCarIndex])   // X360 lbzx r11, r26(this+0x598), r30
                {
                    CGS_ASSERT(lActiveChallengeAction.miNumPlayersInChallenge <
                                   GameStateModuleIO::ActiveFburnChallengeAction::KI_MAX_NETWORK_PLAYERS,
                               "lActiveChallengeAction.miNumPlayersInChallenge < KI_MAX_NETWORK_PLAYERS");   // X360 :4983 -- non-gating
                    lActiveChallengeAction.maePlayersInChallengeARCI[
                        lActiveChallengeAction.miNumPlayersInChallenge] = leActiveRaceCarIndex;   // X360 stwx r30, count*4, sp+0x58
                    ++lActiveChallengeAction.miNumPlayersInChallenge;   // X360 lwz sp+0x78; addi 1; stw back
                }
            }

            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lActiveChallengeAction),
                                    GameStateModuleIO::E_ACTION_ACTIVE_FREEBURN_CHALLENGE,
                                    sizeof(GameStateModuleIO::ActiveFburnChallengeAction));   // X360 li r5,0xA1; li r6,0x30
        }
    }
}

// ----------------------------------------------------------------------------
// NetworkPlayerRemoved -- X360 0x8234E420. A network player left: release their completion
// slot, and if they were taking part in the live challenge, abort it (bouncing everyone back
// to the challenge selector when online).
//
// The slot release mirrors the Construct/NetworkPlayerAdded init pattern in reverse
// (stb 0 +0x104 / stw -1 +0x100 / 32 std zeroes over the 256-byte bit store == UnSetAll()).
// ----------------------------------------------------------------------------
void ChallengeManager::NetworkPlayerRemoved(BrnNetwork::NetworkPlayerID lPlayerID,
                                            TGameActionQueue* lpActionQueue,
                                            bool lbIsOnline)
{
    ChallengeCompletionData* lpEntry = 0;   // X360 r31 seed 0
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID == lPlayerID)   // X360 lwz slot+0x100; cmpw
        {
            lpEntry = &maChallengeCompletionData[liSlot];
            break;
        }
    }

    if (lpEntry != 0)   // X360 cmplwi r31,0; beq -> skip the release
    {
        lpEntry->mbFinalised      = false;          // X360 stb r28(0), 0x104(r31)
        lpEntry->mNetworkPlayerID = -1;             // X360 stw -1, 0x100(r31)
        lpEntry->mCompletedChallenges.UnSetAll();   // X360 32 std 0 over the 256-byte bit store
    }

    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE)   // X360 lwz +0xE08; cmpwi 0; beq -> return
    {
        CGS_ASSERT(mpGameStateModule != 0, "mpGameStateModule");   // X360 :5026

        const EActiveRaceCarIndex leActiveRaceCarIndex = mpGameStateModule->GetActiveRaceCarIndex(lPlayerID);
        if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&   // X360 cmpwi r3,-1; beq -> return
            mabPlayerStartedChallenge[leActiveRaceCarIndex])              // X360 lbz 0x598(r3+this)
        {
            if (lbIsOnline && lpEntry != 0)   // X360 clrlwi r26; beq / cmplwi r31,0; beq
            {
                // The X360 posts the action ZEROED (std r28 == 0 into the single CgsID field):
                // "no challenge" -> the GUI falls back to the selector.
                GameStateModuleIO::FburnChallengeShowSelectorAction lShowSelectorAction;
                lShowSelectorAction.mChallengeID = 0;
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowSelectorAction),
                                        GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR,
                                        sizeof(GameStateModuleIO::FburnChallengeShowSelectorAction));   // X360 li r5,0xA0; li r6,8
            }

            EndChallenge(E_CHALLENGE_STATUS_ABORTED_DUE_TO_PLAYER_LEAVE, lpActionQueue, false);   // X360 li r4,3; li r6,0
        }
    }
}

// ----------------------------------------------------------------------------
// SetRemotePlayersChallengeCompleted -- X360 0x82323DF8. Called from EndChallenge /
// RemoteEndChallenge when the local player completes challenge index liChallengeIndex: every
// remote player who STARTED this challenge gets the same challenge marked complete in their
// own completion bit store (they finished it alongside us).
//
// The X360 inlines FastBitArray<2000>::SetBit (the "Index N is out of range (max bits: 2000)"
// StrStream blob at CgsFastBitArray.h:431 is that container's own range assert, and the
// shift/or triple is its bit math) -- call the committed container method.
// ----------------------------------------------------------------------------
void ChallengeManager::SetRemotePlayersChallengeCompleted(s32 liChallengeIndex)
{
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)   // X360 counts 7 down, slot stride 0x108
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID != -1)   // X360 lwz +0x100; cmpwi -1; beq next
        {
            CGS_ASSERT(mpGameStateModule != 0, "mpGameStateModule");   // X360 :2436

            const EActiveRaceCarIndex leActiveRaceCarIndex =
                mpGameStateModule->GetActiveRaceCarIndex(maChallengeCompletionData[liSlot].mNetworkPlayerID);
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");        // X360 :2438 -- non-gating
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");     // X360 :2439 -- non-gating

            if (mabPlayerStartedChallenge[leActiveRaceCarIndex])   // X360 lbz 0x598(r31+this)
            {
                maChallengeCompletionData[liSlot].mCompletedChallenges.SetBit(liChallengeIndex);
            }
        }
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wC_03.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_03.cpp
// ============================================================================
// Wave-C partfile for BrnGameState::ChallengeManager -- group G4 (keystone rows 10/12/13).
// Store-for-store reconstruction of three X360 ARTIST methods over the keystone-frozen NAMED
// members (layout evidence lives in BrnChallengeManager.h):
//
//   OutputFreeburnChallengeEveryPlayerStatusEvent @ 0x82348080 (DWARF :260)
//   CheckCurrentCar                               @ 0x823336E8 (DWARF :480)
//   IsSkillScoreCurrentlySuccessful               @ 0x823167A0 (DWARF :671, X360 7-param shape)
//
// SOURCE-OF-TRUTH: the X360 asm is authoritative for every store/branch/early-out. No header
// is edited by this file; every foreign field is reached through its committed accessor.
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// OutputFreeburnChallengeEveryPlayerStatusEvent -- X360 0x82348080.
// ----------------------------------------------------------------------------
// Build the aggregated "every player" freeburn-challenge completion block on the stack and
// post it to the game-action queue:
//   * Construct() clears all seven remote slots + the local block.
//   * The X360 `memcpy(event+0x738, this+0xD00, 0x100)` IS the frozen inline member copy
//     FburnChallengeEveryPlayerStatusData::AddLocalPlayerCompletionStatus (the trailing
//     mLocalChallengeCompletionData field).
//   * Every occupied maChallengeCompletionData slot (mNetworkPlayerID != -1) is added.
// The manager's FastBitArray<2000> stores and the SharedIO CompletedFburnChallenges block are
// the identical 256-byte layout; the single documented reinterpret_cast bridges them
// (keystone pitfall F3 / wave-B pitfall 8).
void ChallengeManager::OutputFreeburnChallengeEveryPlayerStatusEvent(TGameActionQueue* lpActionQueue)
{
    GameStateModuleIO::FburnChallengeEveryPlayerStatusData lStatusData;
    lStatusData.Construct();

    lStatusData.AddLocalPlayerCompletionStatus(
        reinterpret_cast<const GameStateModuleIO::CompletedFburnChallenges*>(&mLocalChallengeCompletionData));

    // X360: r31 = this + 0x6C8 (== &maChallengeCompletionData[0].mNetworkPlayerID), 7 iterations,
    // stride 0x108; the status block address passed is r31 - 0x100 (the slot's bit store).
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID != -1)
        {
            lStatusData.AddCompletionStatus(
                reinterpret_cast<const GameStateModuleIO::CompletedFburnChallenges*>(
                    &maChallengeCompletionData[liSlot].mCompletedChallenges),
                maChallengeCompletionData[liSlot].mNetworkPlayerID);
        }
    }

    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue");   // BrnChallengeManager.cpp:5113

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStatusData),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_EVERY_PLAYER_COMPLETION_STATUS,
                            sizeof(lStatusData));   // X360 li r5,0x9D; li r6,0x838 (2104)
}

// ----------------------------------------------------------------------------
// CheckCurrentCar -- X360 0x823336E8.
// ----------------------------------------------------------------------------
// Does the player's current car satisfy the challenge's car restriction?
//   * A challenge that names an exact car id (GetCarID() != 0) only passes when the player's
//     active car model id matches it (full 64-bit CgsID compare -- X360 `cmpld`).
//   * Otherwise an unrestricted challenge (E_CAR_TYPE_NONE) always passes.
//   * Otherwise the player's car is resolved through the vehicle list and its boost-class byte
//     is mapped onto the challenge's ECarRestrictionType.
// The "Player car index hasn't been set" assert (BrnRaceCarEntityModuleOutputInterface.h:980)
// the X360 emits in the first branch is the interface's own inlined GetPlayerActiveRaceCarIndex
// guard -- it comes from the committed accessor, so it is not duplicated here.
bool ChallengeManager::CheckCurrentCar(
    const BrnResource::ChallengeListEntry* lpChallenge,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface)
{
    if (lpChallenge->GetCarID() != 0)   // X360 ld 0xC8(challenge); cmpldi 0
    {
        return lpActiveRaceCarOutputInterface->GetCarModelId(
                   lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex()) == lpChallenge->GetCarID();
    }

    if (lpChallenge->GetCarType() == BrnResource::ChallengeListEntry::E_CAR_TYPE_NONE)   // X360 lbz 0xD0; cmplwi 0
    {
        return true;
    }

    CGS_ASSERT(mpVehicleList != 0, "mpVehicleList != NULL");   // BrnChallengeManager.cpp:2678

    const s32 liModelIndex = mpVehicleList->GetVehicleIndex(
        lpActiveRaceCarOutputInterface->GetCarModelId(
            lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex()));
    CGS_ASSERT(liModelIndex >= 0, "liModelIndex >= 0");   // :2681

    const BrnResource::VehicleListEntry* lpListEntry = mpVehicleList->GetVehicleData(liModelIndex);
    CGS_ASSERT(lpListEntry != 0, "lpListEntry != NULL");   // :2685

    // VehicleListEntry::GetCarType() is the raw +0xE8 boost-class byte
    // (0 == DANGER, 1 == AGGRESSION, 2 == STUNTS); the challenge restriction enum is 1-based.
    const u8 luVehicleCarType = lpListEntry->GetCarType();

    if (luVehicleCarType == 0 &&
        lpChallenge->GetCarType() == BrnResource::ChallengeListEntry::E_CAR_TYPE_DANGER)
    {
        return true;
    }
    if (luVehicleCarType == 1 &&
        lpChallenge->GetCarType() == BrnResource::ChallengeListEntry::E_CAR_TYPE_AGGRESSION)
    {
        return true;
    }
    if (luVehicleCarType == 2 &&
        lpChallenge->GetCarType() == BrnResource::ChallengeListEntry::E_CAR_TYPE_STUNT)
    {
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// IsSkillScoreCurrentlySuccessful -- X360 0x823167A0.
// ----------------------------------------------------------------------------
// Has lfScore reached the action's target for this frame? The X360 shape carries the two extra
// middle params (liActionIndex/leActiveRaceCarIndex) used only by the SEQUENCE branch below --
// see the register map documented at the declaration.
//
//   lbLargerScoresAreBetter:
//     score >= target                                                            -> success
//     CUMULATIVE/AVERAGE coop, score > 0, not arbitrating the cumulative total    -> success
//     coop type 6, score > 0 and the score CHANGED since this player's recorded
//       cumulative contribution                                                   -> success (logged)
//     otherwise                                                                   -> failure
//   !lbLargerScoresAreBetter (lower is better -- e.g. timed actions):
//     score <= target                                                            -> success
//     CUMULATIVE/AVERAGE coop, score below the FLT_MAX "unset" marker, not
//       arbitrating the cumulative total                                          -> success
//     otherwise                                                                   -> failure
bool ChallengeManager::IsSkillScoreCurrentlySuccessful(f32 lfScore,
                                                       const BrnResource::ChallengeListEntryAction* lpAction,
                                                       bool lbIsCumulativeArbitration,
                                                       s32 liTargetValueIndex,
                                                       s32 liActionIndex,
                                                       EActiveRaceCarIndex leActiveRaceCarIndex,
                                                       bool lbLargerScoresAreBetter)
{
    typedef BrnResource::ChallengeListEntryAction Action;

    if (lbLargerScoresAreBetter)
    {
        // X360 fcfid/frsp -- the target is an s32 converted to f32.
        const f32 lfTargetValue = static_cast<f32>(lpAction->GetTargetValue(liTargetValueIndex));
        if (lfScore >= lfTargetValue)
        {
            return true;
        }

        const Action::EChallengeCoopType leCoopType = lpAction->GetCoopType();   // X360 lbz +0x01

        if ((leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
             leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE) &&
            lfScore > 0.0f && !lbIsCumulativeArbitration)
        {
            return true;
        }

        // Coop type 6 == the X360 sequence regime (the committed enum's COUNT slot; same raw
        // value the wB_13 UpdateActionSuccess switch already handles).
        if (leCoopType == Action::E_CHALLENGE_COOP_TYPE_COUNT && lfScore > 0.0f &&
            maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] != lfScore)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "OUTPUTTING SEQUENCE ACTION SUCCESS. Frame: "
                                           << miFramesSinceNetworkStart << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Active score: " << lfScore << " Score last frame: "
                                           << maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex]
                                           << "\n";
            }
            return true;
        }

        return false;
    }

    const f32 lfTargetValue = static_cast<f32>(lpAction->GetTargetValue(liTargetValueIndex));
    if (lfScore <= lfTargetValue)
    {
        return true;
    }

    const Action::EChallengeCoopType leCoopType = lpAction->GetCoopType();   // X360 lbz +0x01
    if (leCoopType != Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE &&
        leCoopType != Action::E_CHALLENGE_COOP_TYPE_AVERAGE)
    {
        return false;
    }
    if (lfScore >= FLT_MAX)   // X360 flt_82020AFC
    {
        return false;
    }
    if (lbIsCumulativeArbitration)
    {
        return false;
    }

    return true;
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnChallengeManager_wC_04.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_04.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-C partfile (group G5): the three world-space
// gates of the freeburn-challenge scorer. All three were previously parked as
// "VMX128 blocked"; every VMX sequence in the X360 asm decodes to plain scalar math over
// the NAMED Vector3 lanes of the committed types, so they are reconstructed store-for-store
// here (no raw offset access, no header edits).
//
//   IsPointInTriggerRegion  (X360 0x82333368)
//   CheckCurrentLocation    (X360 0x82333878)
//   UpdateLeaptCars         (X360 0x82333BB8)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store, branch, early-out,
// assert and call. Two places where the wave-C fix spec's prose disagrees with the asm are
// resolved in FAVOUR OF THE ASM and flagged inline:
//   (1) CheckCurrentLocation's world-map sample goes through the *Vector2* overload
//       (xref target 0x82907FF8 == CgsWorld::WorldMap2D::GetValue(Vector2)); the `vperm`
//       against unk_82CDA450 immediately before the call is a BARE inline Vector3->Vector2
//       XZ pack that REUSES BrnMath::Flatten's permute mask -- it is NOT a call to Flatten
//       (@0x822CB8E8): the site emits no `bl` and 0x82333878 is absent from Flatten's
//       xrefs_to, so Flatten's own `RwMath::IsValid( lVector )` assert does not fire here.
//   (2) UpdateLeaptCars' two PHASE-1 SetCurrentSkillScore calls pass lbBankImmediately ==
//       TRUE (`li r6, 1` @0x82333D00 and @0x82333D98); only the tail call passes false
//       (`li r6, 0` @0x82334574).
//
// VMX decode notes (all confirmed against the operand order IDA prints for `vmaddfp`, which
// is the raw field order vD, vA, vB, vC -- i.e. the result is vA*vC + vB; the only reading
// under which both fused-multiply-adds in this TU are meaningful):
//   * `vspltw vX, vY, N` == reading lane N of a Vector3: 0 -> .x, 1 -> .y, 2 -> .z.
//   * `vsubfp` / `vmulfp128` by a splat scalar == the committed rw::math::vpu operator- /
//     operator*(Vector3, float).
//   * `vmsum3fp128 v0, v0, v0` == rw::math::vpu::MagnitudeSquared (xyz lanes).
//   * the `fsel` pairs are the two-step max() chain over the box dimensions.
// ============================================================================


namespace BrnGameState
{

// X360 `cmpwi r29, 7` / `cmpwi r21, 7` / `li r5, 7` -- the leaping-data pool capacity, which is
// also the number of bits the local keep-mask BitArray carries (the inlined range asserts print
// "Number of bits: 7"). Named rather than repeated as a literal; the value is attested.
static const s32 KI_LEAPT_CAR_POOL_CAPACITY = 7;

// ----------------------------------------------------------------------------
// IsPointInTriggerRegion  (X360 0x82333368)
// ----------------------------------------------------------------------------
// Answer "is lpPoint inside the trigger region whose id is lTriggerID", for the subset of
// regions the TriggerQueryManager currently has armed. The region must be a generic region of
// sub-type 13; a cheap bounding-sphere pre-cull runs before the exact oriented-box test.
//
// Register map (asm prologue): r3 this, r4 lpPoint (r19), r5 lTriggerID (r20, 64-bit CgsID).
bool ChallengeManager::IsPointInTriggerRegion(const Vector3* lpPoint, CgsID lTriggerID)
{
    CGS_ASSERT(lpPoint, "lpPosition");                                   // X360 line 2594
    CGS_ASSERT(mpTriggerQueryManager, "mpTriggerQueryManager");          // X360 line 2595

    // Linear scan of the armed trigger set. The X360 re-reads the live count through the
    // accessor on every iteration (the CgsArray "Array used before Construct/Clear was called"
    // guard lives inside it) and once more after the loop -- reproduced by calling the committed
    // accessor at both points rather than caching the count.
    const BrnTrigger::TriggerRegion* lpTriggerRegion = NULL;
    s32 liIndex = 0;
    for (; liIndex < static_cast<s32>(mpTriggerQueryManager->GetActiveTriggerCount()); ++liIndex)
    {
        const u16 luRegionIndex = mpTriggerQueryManager->GetActiveTrigger(static_cast<u32>(liIndex));

        // X360 `TriggerData::GetMemor(this+1568)` == the committed ResourcePtr-backed accessor;
        // GetRegion carries its own "liRegionIndex < miRegionCount" guard (BrnTriggerData.h:624).
        lpTriggerRegion = mpTriggerQueryManager->GetTriggerData()->GetRegion(luRegionIndex);
        CGS_ASSERT(lpTriggerRegion != NULL, "lpTriggerRegion != NULL");   // X360 line 2603

        if (lpTriggerRegion->GetId() == lTriggerID)                      // lwz +0x24, extsw, cmpld
        {
            break;
        }
    }

    if (liIndex >= static_cast<s32>(mpTriggerQueryManager->GetActiveTriggerCount()))
    {
        return false;                                                    // scan exhausted: no such armed trigger
    }

    if (lpTriggerRegion->GetType() != BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)  // lbz +0x2A; cmplwi 2
    {
        // Runtime-valued diagnostic: the X360 streams the trigger id into the assert message
        // buffer through the global StrStream (the `sub_82203EE8` call is
        // StrStreamBase::operator<<(u64)). Reproduced with the committed local-StrStream idiom.
        // X360 file/line: BrnChallengeManager.cpp:2642.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Trigger " << lTriggerID << " is not a generic region";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
        return false;
    }

    const BrnTrigger::GenericRegion* lpGenericRegion =
        static_cast<const BrnTrigger::GenericRegion*>(lpTriggerRegion);

    // X360 `lbz r11, 0x36(r31); cmplwi r11, 0xD` -- only generic sub-type 13 (== the committed
    // BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_STRENGTH, which the header pins at that very
    // offset) qualifies.
    if (lpGenericRegion->GetType() != BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_STRENGTH)
    {
        return false;
    }

    const BrnTrigger::BoxRegion* lpBoxRegion = lpTriggerRegion->GetBoxRegion();

    // Bounding-sphere pre-cull: the sphere radius is half the LARGEST box dimension. The two
    // `fsel`s are the max chain (Y-vs-Z first, then X against that).
    const f32 lfMaxDimensionYZ = (lpBoxRegion->GetDimensionY() >= lpBoxRegion->GetDimensionZ())
                                     ? lpBoxRegion->GetDimensionY()
                                     : lpBoxRegion->GetDimensionZ();
    const f32 lfMaxDimension   = (lpBoxRegion->GetDimensionX() >= lfMaxDimensionYZ)
                                     ? lpBoxRegion->GetDimensionX()
                                     : lfMaxDimensionYZ;
    const f32 lfRadius         = lfMaxDimension * 0.5f;                  // flt_82001DA0 == 0.5f

    // vsubfp (boxPosition - point) + vmsum3fp128 == MagnitudeSquared of the xyz lanes.
    const f32 lfDistanceSquared = MagnitudeSquared(lpBoxRegion->GetPosition() - *lpPoint);

    if (lfDistanceSquared < lfRadius * lfRadius)                         // vcmpgtfp. radius^2 > distance^2
    {
        const Matrix44Affine lTransform = lpBoxRegion->ComputeTransform();
        if (BrnMath::IsPointInsideBox(lTransform, *lpPoint, lpBoxRegion->GetDimensions() * 0.5f))
        {
            return true;
        }
    }

    return false;
}

// ----------------------------------------------------------------------------
// CheckCurrentLocation  (X360 0x82333878)
// ----------------------------------------------------------------------------
// Is the player currently somewhere this challenge action allows? An action with no location
// restrictions is allowed anywhere; otherwise ANY one of its (up to four) locations matching
// the player's current district / trigger region / road passes the gate.
//
// Register map (asm prologue): r3 this (r28), r4 lpAction (r26), r5 lpActiveRaceCarOutputInterface (r31).
bool ChallengeManager::CheckCurrentLocation(
    const BrnResource::ChallengeListEntryAction*                                 lpAction,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface)
{
    CGS_ASSERT(lpAction, "lpAction");                                             // X360 line 2723
    CGS_ASSERT(mpRoadRulesManager, "mpRoadRulesManager");                         // X360 line 2724
    CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface"); // X360 line 2725

    // sub_82310240 == the committed GetPlayerRaceCarState() (it carries its own player-index asserts).
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
        lpActiveRaceCarOutputInterface->GetPlayerRaceCarState();
    CGS_ASSERT(lpRaceCarState, "lpRaceCarState");                                 // X360 line 2729

    const CgsWorld::WorldMap2D* lpWorldMap = lpActiveRaceCarOutputInterface->GetWorldMap2D();
    CGS_ASSERT(lpWorldMap, "lpWorldMap");                                         // X360 line 2733

    // `lvx v0, r0, (state+544)` + `stvx v0` into the stack local == the by-value copy of the
    // transform's translation row. The local is address-taken by the TRIGGER case below.
    const Vector3 lCarPosition = lpRaceCarState->mTransform.Pos();

    // The `lvx128 v7,&unk_82CDA450; vperm v1,v0,v0,v7` here is a BARE permute -- the inline
    // Vector3 -> Vector2 XZ pack that shares BrnMath::Flatten's lane mask. It is NOT a call
    // to Flatten: Flatten @0x822CB8E8 is out-of-line, this site emits no `bl`, and 0x82333878
    // is absent from Flatten's xrefs -- so the IsValid assert Flatten's body fires does NOT
    // execute here. The call target is the Vector2 overload of GetValue (0x82907FF8).
    // The permute mask is dumped from the image: bytes 00 01 02 03 18 19 1A 1B 00 01 02 03
    // 00 01 02 03, i.e. lane 0 = x and lane 1 = byte 8 of the second (same) operand = z. The
    // pack is (x, z): XZ, confirmed.
    const Vector2 l2DCarPosition = Vector2{ lCarPosition.x, lCarPosition.z };
    const s32 liDistrict = static_cast<s32>(lpWorldMap->GetValue(l2DCarPosition));

    const CgsID lCurrentRoadID = mpRoadRulesManager->GetCurrentRoadID();

    if (lpAction->GetNumLocations() == 0)                                         // lbz +4
    {
        return true;                                                              // unrestricted action
    }

    for (u8 lu8LocationIndex = 0; lu8LocationIndex < lpAction->GetNumLocations(); ++lu8LocationIndex)
    {
        // GetLocationType inlines the "luLocationIndex < KU_MAX_LOCATIONS_PER_ACTION" guard
        // (ChallengeListEntry.h:753) -- call the accessor, never duplicate the assert.
        switch (lpAction->GetLocationType(lu8LocationIndex))
        {
        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_ANYWHERE:
            return true;

        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_DISTRICT:
            if (lpAction->GetDistrict(lu8LocationIndex) == liDistrict)
            {
                return true;
            }
            break;

        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_TRIGGER:
            if (IsPointInTriggerRegion(&lCarPosition, lpAction->GetTriggerID(lu8LocationIndex)))
            {
                return true;
            }
            break;

        // The two road flavours share one branch in the X360 jump table.
        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_ROAD:
        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_ROAD_NO_MARKER:
            if (lCurrentRoadID != 0 && lpAction->GetRoadID(lu8LocationIndex) == lCurrentRoadID)
            {
                return true;
            }
            break;

        default:
        {
            // Runtime-valued diagnostic (the X360 re-reads the type byte through the same inlined
            // accessor to stream it). Committed local-StrStream idiom; the default case does NOT
            // return -- it falls through to the next location. X360 file/line:
            // BrnChallengeManager.cpp:2791.
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Unknown location type "
                       << static_cast<s32>(lpAction->GetLocationType(lu8LocationIndex));
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            break;
        }
        }
    }

    return false;
}

// ----------------------------------------------------------------------------
// UpdateLeaptCars  (X360 0x82333BB8)
// ----------------------------------------------------------------------------
// Per-frame tracker for the LEAP_CARS freeburn skill. Two phases:
//
//   PHASE 1 -- the player is off the gas: wind down the "leap still counts" grace timer, banking
//     the running leap count into the skill each frame (or, for the two accumulating co-op types,
//     only once the timer expires), then reset the tracker.
//
//   PHASE 2 -- rebuild the candidate set: every networked car the player is currently flying OVER
//     (a clear height gap, within a 10m horizontal radius) becomes a candidate. Each candidate is
//     matched against the persistent mPotentiallyLeaptCars pool by active-race-car index; the sign
//     of the 2D (XZ) dot of the stored entry vector against the current player->car vector says
//     whether the player has crossed over that car -- a sign flip is a leap. Slots that were not
//     re-confirmed this frame are freed.
//
// Register map (asm prologue): r3 this (r16), r4 lpActiveRaceCarOutputInterface (r19),
// f1 lfTimeStep (f31).
void ChallengeManager::UpdateLeaptCars(
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    f32 lfTimeStep)
{
    CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface");   // X360 line 3900

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpPlayerRaceCarState =
        lpActiveRaceCarOutputInterface->GetPlayerRaceCarState();
    CGS_ASSERT(lpPlayerRaceCarState, "lpPlayerRaceCarState");                       // X360 line 3903

    // ---------------- phase 1: off-gas wind-down ----------------
    if (lpPlayerRaceCarState->mfGas <= 0.0f)                                        // lfs +0x404
    {
        if (mbUpdateLeaptCars && miNumCarsLeapt > 0)
        {
            if (mfLeapCarsValidTimer == -1.0f)                                      // flt_820037C8
            {
                mfLeapCarsValidTimer = 0.25f;                                       // flt_82021138
            }

            CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");                   // X360 line 3921

            // Everything EXCEPT the two accumulating co-op types banks the running count every
            // frame while the grace timer runs down. (lbBankImmediately == true: `li r6, 1`.)
            if (mpCurrentChallenge->GetAction(miCurrentChallengeAction)->GetCoopType() !=
                    BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION &&
                mpCurrentChallenge->GetAction(miCurrentChallengeAction)->GetCoopType() !=
                    BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_LEAP_CARS,
                                     static_cast<f32>(miNumCarsLeapt), true);
            }

            mfLeapCarsValidTimer -= lfTimeStep;
            if (mfLeapCarsValidTimer < 0.0f)
            {
                mfLeapCarsValidTimer = -1.0f;
                mbUpdateLeaptCars = false;

                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");               // X360 line 3942

                // ...and the two accumulating co-op types bank exactly once, on expiry.
                if (mpCurrentChallenge->GetAction(miCurrentChallengeAction)->GetCoopType() ==
                        BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION ||
                    mpCurrentChallenge->GetAction(miCurrentChallengeAction)->GetCoopType() ==
                        BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_LEAP_CARS,
                                         static_cast<f32>(miNumCarsLeapt), true);
                }

                mPotentiallyLeaptCars.Clear();   // occupancy=0, free queue refilled {6..0}, numFree=7
                miNumCarsLeapt = 0;
            }
        }
    }
    else
    {
        mbUpdateLeaptCars = true;
    }

    // ---------------- phase 2: rebuild + match the candidate set ----------------
    if (mbUpdateLeaptCars)
    {
        // Stack-local candidate pool (X360 sp+0xC0, same 0x108-byte ObjectPool shape as the member).
        CgsContainers::ObjectPool<CarLeapingData, 7, s32> lLeapCandidates;
        lLeapCandidates.Clear();

        const Vector3 lPlayerPosition = lpPlayerRaceCarState->mTransform.Pos();

        for (::EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
             static_cast<s32>(leActiveRaceCarIndex) < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             leActiveRaceCarIndex++)
        {
            // The interface's own range asserts (BrnRaceCarEntityModuleOutputInterface.h:898/899)
            // are inlined into this accessor -- call it, do not duplicate them.
            if (!lpActiveRaceCarOutputInterface->IsRaceCarNetwork(leActiveRaceCarIndex))
            {
                continue;
            }

            CGS_ASSERT(lpActiveRaceCarOutputInterface,
                       "lpActiveRaceCarOutputInterface");                           // X360 line 3970 (re-asserted)

            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
                lpActiveRaceCarOutputInterface->GetRaceCarState(leActiveRaceCarIndex);
            CGS_ASSERT(lpRaceCarState, "lpRaceCarState");                           // X360 line 3972

            // Element +0x44A -- mbCrashing. A car that is mid-crash cannot be leapt.
            if (lpRaceCarState->mbCrashing)
            {
                continue;
            }

            const Vector3 lCarPosition = lpRaceCarState->mTransform.Pos();

            // Height gate: the player's underside must clear the car's roof by 0.25m.
            const f32 lfHeightAboveCar = (lPlayerPosition.y - lCarPosition.y)
                                       - lpPlayerRaceCarState->mHalfExtent.y
                                       - lpRaceCarState->mHalfExtent.y;
            if (lfHeightAboveCar < 0.25f)                                           // flt_82021138
            {
                continue;
            }

            // Horizontal gate: within 10m on the XZ plane (100.0 == flt_820049E0, squared radius).
            const Vector3 lPlayerToCar = lCarPosition - lPlayerPosition;
            if (lPlayerToCar.x * lPlayerToCar.x + lPlayerToCar.z * lPlayerToCar.z > 100.0f)
            {
                continue;
            }

            const s32 liAllocatedIndex = lLeapCandidates.AllocateObject();
            CGS_ASSERT(liAllocatedIndex >= 0, "liAllocatedIndex >= 0");             // X360 line 4001
            if (liAllocatedIndex >= 0)
            {
                lLeapCandidates[liAllocatedIndex].mActiveRaceCarIndex = leActiveRaceCarIndex;
                lLeapCandidates[liAllocatedIndex].mPlayerToCar        = lPlayerToCar;
            }
        }

        // Which persistent slots survive this frame. The X360 keeps this in one stack 64-bit word
        // (zeroed by the single `std r15, sp+0x90`); the "Index: N, Number of bits: 7" StrStream
        // blobs around the sets are the container's own inlined range asserts.
        CgsContainers::BitArray<KI_LEAPT_CAR_POOL_CAPACITY> lCarsStillPotentiallyLeapt;
        lCarsStillPotentiallyLeapt.UnSetAll();

        for (s32 liCandidate = 0; liCandidate < KI_LEAPT_CAR_POOL_CAPACITY; ++liCandidate)
        {
            if (!lLeapCandidates.IsObjectAllocated(liCandidate))
            {
                continue;
            }

            const CarLeapingData& lrCandidate = lLeapCandidates[liCandidate];

            s32 liStored = 0;
            for (; liStored < KI_LEAPT_CAR_POOL_CAPACITY; ++liStored)
            {
                if (mPotentiallyLeaptCars.IsObjectAllocated(liStored) &&
                    mPotentiallyLeaptCars[liStored].mActiveRaceCarIndex == lrCandidate.mActiveRaceCarIndex)
                {
                    break;
                }
            }

            if (liStored < KI_LEAPT_CAR_POOL_CAPACITY)
            {
                // Already tracked: compare the entry vector against the current one on the XZ plane.
                const StoredLeapingData& lrStored = mPotentiallyLeaptCars[liStored];
                const f32 lfEntryDot = lrStored.mLeapRadiusEntryVector.x * lrCandidate.mPlayerToCar.x
                                     + lrStored.mLeapRadiusEntryVector.z * lrCandidate.mPlayerToCar.z;
                if (lfEntryDot >= 0.0f)
                {
                    // Same side of the car as on entry: still only a POTENTIAL leap; keep the slot
                    // (and its ORIGINAL entry vector -- the X360 never rewrites it here).
                    lCarsStillPotentiallyLeapt.SetBit(static_cast<u32>(liStored));
                }
                else
                {
                    // Sign flip: the player crossed over the car. Count the leap and let the slot
                    // fall out of the keep-mask so the sweep below frees it.
                    ++miNumCarsLeapt;
                }
            }
            else
            {
                // Newly inside the leap radius: remember where the player entered from.
                const s32 liAllocatedIndex = mPotentiallyLeaptCars.AllocateObject();
                CGS_ASSERT(liAllocatedIndex >= 0, "liAllocatedIndex >= 0");         // X360 line 4064
                if (liAllocatedIndex >= 0)
                {
                    mPotentiallyLeaptCars[liAllocatedIndex].mActiveRaceCarIndex =
                        lrCandidate.mActiveRaceCarIndex;
                    mPotentiallyLeaptCars[liAllocatedIndex].mLeapRadiusEntryVector =
                        lrCandidate.mPlayerToCar;
                    lCarsStillPotentiallyLeapt.SetBit(static_cast<u32>(liAllocatedIndex));
                }
            }
        }

        // Sweep: anything not re-confirmed this frame is out of range (or has been leapt) -- free it.
        for (s32 liStored = 0; liStored < KI_LEAPT_CAR_POOL_CAPACITY; ++liStored)
        {
            if (mPotentiallyLeaptCars.IsObjectAllocated(liStored) &&
                !lCarsStillPotentiallyLeapt.IsBitSet(static_cast<u32>(liStored)))
            {
                mPotentiallyLeaptCars.FreeObject(liStored);
            }
        }

        SetCurrentSkillScore(E_FREEBURN_SKILL_LEAP_CARS,
                             static_cast<f32>(miNumCarsLeapt), false);              // `li r6, 0`
    }
}

}

// ============================================================================
// FOLDED FROM BrnChallengeManager_wC_05.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_05.cpp
// ============================================================================
// Wave-C partfile (group G6) for BrnGameState::ChallengeManager. One function,
// reconstructed store-for-store from the X360 ARTIST asm over the keystone's named
// members and committed accessors:
//   PostWorldUpdate  @ 0x8233AC28
//
// The X360 body is the challenge manager's END-of-frame pass while a challenge is
// RUNNING: it tallies the frame's race-car crashes between challenge participants,
// clears this network frame's success bit for every REMOTE player's success ring
// (only the local player's bit is left for the success-update path to set), and then
// forwards the stunt-score snapshot to UpdateStuntScores.
//
// SIGNATURE (asm-derived, matches the frozen header): three params -- r4 the crash
// event queue, r5 the stunt-score snapshot (spilled to its home slot at the prologue
// `stw r5, arg_24(r1)` and reloaded at the tail `lwz r4, arg_24(r1)` immediately
// before the UpdateStuntScores tail call, i.e. passed straight through), r6 the timer
// status interface.
// ============================================================================


namespace BrnGameState
{

// ----------------------------------------------------------------------------
// ChallengeManager::PostWorldUpdate -- X360 0x8233AC28
// ----------------------------------------------------------------------------
// Whole body is gated on meChallengeManagerStatus == RUNNING (lwz 0xE08 / cmpwi 2 /
// bne -> epilogue); nothing at all happens in any other state.
//
// (1) CRASH WALK. The event count is read ONCE before the loop (`lwz r27,8(r28)`) and
//     the loop runs i in [0, count). Each event is fetched through the queue's element
//     accessor. Only crashes whose CRASHER entity word carries owner byte 1 (the race-car
//     entity type -- `srwi r10,r11,24 ; cmplwi r10,1`) are considered; for those the two
//     participants' active-race-car slots are decoded out of the packed handles, range
//     asserted (cpp:751 / cpp:752), and -- when BOTH cars are in the challenge -- each
//     car's "crashed with a challenge player" tally is bumped.
//
//     DECODE: both slots are the 14-bit entity-index field at bit 10 of a packed entity
//     word (owner:[31..24] | entityIndex:[23..10] | partIndex:[9..0]).
//       * crashED comes from mRaceCarVolumeInstanceID, whose entity word is the HIGH dword
//         (`ld 0(event) ; srdi 32 ; extrwi 14,8`) -- exactly the committed
//         VolumeInstanceId::GetEntityIDEntityIndex(), which documents that same inline.
//       * crashER comes from mCrasherEntityID (`lwz 8(event)`), the bare 32-bit entity word.
//         The minimal BrnCommonTypes.h `EntityId` is only the storage word; the committed
//         decoder for that word is CgsSceneManager::EntityId (GetOwner / GetEntityIndex,
//         the same 8/14/10 geometry), so the word is bridged into it here rather than
//         re-spelling the bit math (the file-local-helper alternative used by
//         BrnScoringSystem_UpdateA.cpp predates that accessor).
//
// (2) NETWORK-FRAME BIT. liBit = (miFramesSinceNetworkStart + 1) modulo the ring length
//     of the success array: 100 slots at 50Hz, 120 otherwise. The X360 emits the modulo as
//     the usual magic-multiply/shift/multiply-subtract pair (0x51EB851F>>5 for 100,
//     0x88888889>>6 for 120) -- written back as `%` per the de-optimisation rule.
//
// (3) REMOTE-PLAYER CLEAR. Walk every active-race-car slot with the range-guarded
//     operator++ (its "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" assert is
//     BurnoutConstants.h:39, seen inlined in the loop tail). mpGameStateModule is asserted
//     EVERY iteration (cpp:779 -- the X360 re-loads and re-tests +0xE48 inside the loop),
//     and every slot that is NOT the local player has this frame's bit cleared. The giant
//     "Index N is out of range (max bits: 120)" StrStream block at 0x8233AE68 is
//     FastBitArray's own inlined range assert (CgsFastBitArray.h:452), so the container
//     method is called instead of reproducing it.
//
// (4) TAIL. UpdateStuntScores(lpStuntScoreInfo) -- the r5 param, untouched.
// ----------------------------------------------------------------------------
void ChallengeManager::PostWorldUpdate(
    const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpRaceCarCrashEventQueue,
    const void*                                                  lpStuntScoreInfo,
    const CgsSystem::TimerStatusInterface*                       lpTimerStatusInterface)
{
    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz 0xE08, cmpwi 2
    {
        return;
    }

    const s32 liNumCrashEvents = lpRaceCarCrashEventQueue->GetLength();   // X360 lwz 8(queue), hoisted

    for (s32 liCrashEvent = 0; liCrashEvent < liNumCrashEvents; ++liCrashEvent)
    {
        const BrnPhysics::Vehicle::RaceCarCrashEvent& lCrashEvent =
            lpRaceCarCrashEventQueue->GetEvent(liCrashEvent);

        // Packed crasher entity word (event +8) decoded through the committed EntityId.
        const CgsSceneManager::EntityId lCrasherEntityID(lCrashEvent.mCrasherEntityID.muValue);

        // Owner byte 1 == the race-car entity type; every other owner is skipped outright
        // (the X360 branches straight to the loop increment). Named per the committed
        // BrnCrashModeScoring.cpp:528 precedent so the un-homed enum stays greppable.
        const u32 K_OWNER_RACE_CAR = 1;   // BrnWorld::E_ENTITYTYPE_RACE_CAR (FLAG: enum un-homed)
        if (lCrasherEntityID.GetOwner() == K_OWNER_RACE_CAR)
        {
            const ::EActiveRaceCarIndex leCrasherActiveRaceCarIndex =
                static_cast< ::EActiveRaceCarIndex>(lCrasherEntityID.GetEntityIndex());
            const ::EActiveRaceCarIndex leCrashedActiveRaceCarIndex =
                static_cast< ::EActiveRaceCarIndex>(
                    lCrashEvent.mRaceCarVolumeInstanceID.GetEntityIDEntityIndex());

            CGS_ASSERT((leCrasherActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                       (leCrasherActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT),
                       "( leCrasherActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && "
                       "( leCrasherActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");  // X360 line 751
            CGS_ASSERT((leCrashedActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                       (leCrashedActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT),
                       "( leCrashedActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && "
                       "( leCrashedActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");  // X360 line 752

            // Both cars must be taking part in the current challenge (X360 tests the CRASHED
            // car's flag first, then the CRASHER's, both off the +0x598 byte array).
            if (mabPlayerStartedChallenge[leCrashedActiveRaceCarIndex] &&
                mabPlayerStartedChallenge[leCrasherActiveRaceCarIndex])
            {
                ++maiCrashedWithChallengePlayer[leCrashedActiveRaceCarIndex];
                ++maiCrashedWithChallengePlayer[leCrasherActiveRaceCarIndex];
            }
        }
    }

    // Success-ring slot for this network frame: 100 slots when the sim timer runs at 50Hz,
    // 120 otherwise (X360: the 0.02f time-step product, then the magic-multiply modulo).
    const s32 liNetworkFrame = miFramesSinceNetworkStart + 1;
    const s32 liSuccessUpdateBit =
        liNetworkFrame % (lpTimerStatusInterface->IsSimTimerFrequency50Hz() ? 100 : 120);

    for (::EActiveRaceCarIndex leActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
         static_cast<s32>(leActiveRaceCarIndex) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActiveRaceCarIndex++)
    {
        CGS_ASSERT(mpGameStateModule, "mpGameStateModule");    // X360 line 779 (re-tested each pass)

        if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) !=
            static_cast<s32>(leActiveRaceCarIndex))
        {
            maPlayerSuccessUpdateArray[leActiveRaceCarIndex].UnSetBit(
                static_cast<u32>(liSuccessUpdateBit));
        }
    }

    UpdateStuntScores(lpStuntScoreInfo);
}

}

// ============================================================================
// FOLDED FROM BrnChallengeManager_wC_06.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_06.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-C partfile (group G7): the two orchestration
// giants of the freeburn-challenge runtime.
//
//   ProcessEvent     (X360 0x8233D6A8)
//   UpdateChallenge  (X360 0x82347190)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store, branch, early-out,
// assert and call. Raw offsets are mapped onto the keystone-frozen NAMED members/accessors of
// BrnChallengeManager.h, the frozen event payloads in BrnGameEvents.h and the frozen action
// payloads in BrnGameActions.h. No header was edited by this partfile.
//
// ProcessEvent DISCRIMINANTS: the X360 jump table is RELATIVE (r11 = leEventType - 54, 120
// slots), so the raw table cases 0/1/11/12/13/16/17/65/66/111/112/113/119 are the ABSOLUTE
// event types 54/55/65/66/67/70/71/119/120/165/166/167/173. The frozen E_EVENT_* tags carry
// exactly those values (each case is annotated `== raw N`). E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE
// and E_EVENT_FREEBURN_CHALLENGE_SUCCESS (171/172) are not among them: they take the default arm.
//
// SetCurrentSkillScore's third argument (r6 at every callsite) is the "bank immediately"
// flag: false for the in-progress event flavours (POWER_PARK/NEAR_MISS/DRIFT/ONCOMING/
// InProgressStunt) and true for the completed flavours (BOOST_TIME_COMPLETE/
// NEAR_MISS_CHAIN_COMPLETED/ONCOMING_COMPLETED/CompletedStunt) -- taken verbatim from the asm.
//
// The X360 skill ids 19/20/37 are the drifted (X360-only, unnamed) stunt-run skills; they are
// written as attested numeric casts exactly like the committed UpdateStuntScores body does.
//
// Formatted diagnostics follow the keystone convention: pure-literal messages fold to
// CGS_ASSERT; the ones that stream runtime values use the committed local-StrStream idiom
// (X360 file/line noted in a comment, __FILE__/__LINE__ used by the committed macro machinery).
// ============================================================================


namespace BrnGameState
{

// ============================================================================
// ProcessEvent  (X360 0x8233D6A8)
// ============================================================================
// Jump-table dispatch of the world/stunt/network game events the challenge system scores off.
// Every arm either feeds SetCurrentSkillScore or mutates the challenge bookkeeping; the
// default arm does nothing.
//
// Register map: r3 this, r4 leEventType (the switch selector, `addi r11, r4, -0x36`),
// r5 lpEvent (kept in r24 throughout).
void ChallengeManager::ProcessEvent(GameStateModuleIO::EGameEventType leEventType,
                                    const CgsModule::Event* lpEvent)
{
    switch (leEventType)
    {
        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_POWER_PARK_RESULT:            // == raw 54
        {
            const GameStateModuleIO::PowerParkResultEvent* lpPowerParkResultEvent =
                reinterpret_cast<const GameStateModuleIO::PowerParkResultEvent*>(lpEvent);

            if (lpPowerParkResultEvent->meOutcome == 1)               // E_PPO_SUCCESS
            {
                if (lpPowerParkResultEvent->miOtherPlayersInvolved >= 2)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_PLAYER_POWER_PARKING,
                                         static_cast<f32>(lpPowerParkResultEvent->miOverallRating), false);
                }
                else
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_TRAFFIC_POWER_PARKING,
                                         static_cast<f32>(lpPowerParkResultEvent->miOverallRating), false);
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_BOOST_TIME_COMPLETE:          // == raw 55
        {
            const GameStateModuleIO::BoostTimeCompleteEvent* lpBoostTimeComplete =
                reinterpret_cast<const GameStateModuleIO::BoostTimeCompleteEvent*>(lpEvent);

            CGS_ASSERT(lpBoostTimeComplete, "lpBoostTimeComplete");   // BrnChallengeManager.cpp:5816

            SetCurrentSkillScore(E_FREEBURN_SKILL_BOOST_TIME, lpBoostTimeComplete->mfTimeSpentBoosting, true);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_NEAR_MISS:                    // == raw 65
        {
            const GameStateModuleIO::NearMissEvent* lpNearMissEvent =
                reinterpret_cast<const GameStateModuleIO::NearMissEvent*>(lpEvent);

            CGS_ASSERT(lpNearMissEvent, "lpNearMissEvent");           // :5443

            SetCurrentSkillScore(E_FREEBURN_SKILL_NEAR_MISS,
                                 static_cast<f32>(lpNearMissEvent->miCount), false);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_NEAR_MISS_CHAIN_COMPLETED:    // == raw 66
        {
            const GameStateModuleIO::NearMissChainCompleteEvent* lpNearMissCompleteEvent =
                reinterpret_cast<const GameStateModuleIO::NearMissChainCompleteEvent*>(lpEvent);

            CGS_ASSERT(lpNearMissCompleteEvent, "lpNearMissCompleteEvent");   // :5454

            SetCurrentSkillScore(E_FREEBURN_SKILL_NEAR_MISS,
                                 static_cast<f32>(lpNearMissCompleteEvent->miCount), true);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_DRIFTING:                     // == raw 67
        {
            const GameStateModuleIO::DriftingEvent* lpDriftEvent =
                reinterpret_cast<const GameStateModuleIO::DriftingEvent*>(lpEvent);

            CGS_ASSERT(lpDriftEvent, "lpDriftEvent");                 // :5432

            SetCurrentSkillScore(E_FREEBURN_SKILL_DRIFT, lpDriftEvent->mfDistance, false);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_ONCOMING:                     // == raw 70
        {
            const GameStateModuleIO::OncomingEvent* lpOncomingEvent =
                reinterpret_cast<const GameStateModuleIO::OncomingEvent*>(lpEvent);

            SetCurrentSkillScore(E_FREEBURN_SKILL_ONCOMING, lpOncomingEvent->mfDistance, false);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_ONCOMING_COMPLETED:           // == raw 71
        {
            const GameStateModuleIO::OncomingCompletedEvent* lpOncomingCompletedEvent =
                reinterpret_cast<const GameStateModuleIO::OncomingCompletedEvent*>(lpEvent);

            SetCurrentSkillScore(E_FREEBURN_SKILL_ONCOMING, lpOncomingCompletedEvent->mfDistance, true);
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_COMPLETED_STUNT:              // == raw 119
        {
            const GameStateModuleIO::CompletedStuntEvent* lpCompletedStuntEvent =
                reinterpret_cast<const GameStateModuleIO::CompletedStuntEvent*>(lpEvent);

            CGS_ASSERT(lpCompletedStuntEvent, "lpCompletedStuntEvent");   // :5635

            // Drift: either of the two drift bits scores the completed drift distance.
            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x40) == 0x40 ||
                (lpCompletedStuntEvent->muStuntActionComplete & 0x80) == 0x80)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_DRIFT,
                                     lpCompletedStuntEvent->mfCompletedDriftDistance, true);
            }

            // Landing: a 1.0f/0.0f score, mirrored onto the reverse twin when in reverse.
            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x10) == 0x10)
            {
                const f32 lfLandingScore = lpCompletedStuntEvent->mbSuccessfulLanding ? 1.0f : 0.0f;
                SetCurrentSkillScore(E_FREEBURN_SKILL_SUCCESSFUL_LANDING, lfLandingScore, true);
                if (lpCompletedStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE, lfLandingScore, true);
                }
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 1) == 1)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_BARREL_ROLL,
                                     static_cast<f32>(lpCompletedStuntEvent->miCompletedBarrelRolls), true);
                if (lpCompletedStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_BARREL_ROLL_REVERSE,
                                         static_cast<f32>(lpCompletedStuntEvent->miCompletedBarrelRolls), true);
                }
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 2) == 2)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_FLATSPIN,
                                     lpCompletedStuntEvent->mfCompletedAirSpinAngle * 57.29578f, true);
                if (lpCompletedStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_FLATSPIN_REVERSE,
                                         lpCompletedStuntEvent->mfCompletedAirSpinAngle * 57.29578f, true);
                }
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x800) == 0x800)
            {
                // X360-drift skill 37 (unnamed enumerator; attested numeric id).
                SetCurrentSkillScore(static_cast<EFreeburnSkill>(37),
                                     static_cast<f32>(lpCompletedStuntEvent->miCompletedSkill37Count), true);
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x100) == 0x100)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_AIR, lpCompletedStuntEvent->mfCompletedAirTime, true);
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x200) == 0x200)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_AIR_DISTANCE,
                                     lpCompletedStuntEvent->mfCompletedAirDistance, true);
            }

            if ((lpCompletedStuntEvent->muStuntActionComplete & 0x400) == 0x400)
            {
                // Stunt-run slot: find the challenge's (X360-drift) type-23 action; its
                // GetTargetValue(1) is the 1-BASED stunt-run slot K, so the event arrays read [K-1].
                if (mpCurrentChallenge != NULL)
                {
                    s32 liStuntRunAction = -1;
                    for (s32 liActionIndex = 0;
                         liActionIndex < mpCurrentChallenge->GetNumActions();
                         ++liActionIndex)
                    {
                        if (static_cast<s32>(mpCurrentChallenge->GetAction(liActionIndex)->GetActionType()) == 23)
                        {
                            liStuntRunAction = liActionIndex;
                        }
                    }

                    if (liStuntRunAction >= 0)
                    {
                        const s32 liStuntRunSlot =
                            mpCurrentChallenge->GetAction(liStuntRunAction)->GetTargetValue(1);
                        if (lpCompletedStuntEvent->mabStuntRunScored[liStuntRunSlot - 1])
                        {
                            // X360-drift skill 20 (unnamed enumerator; attested numeric id).
                            SetCurrentSkillScore(static_cast<EFreeburnSkill>(20),
                                                 lpCompletedStuntEvent->mafStuntRunScores[liStuntRunSlot - 1], true);
                        }
                    }
                }

                if (lpCompletedStuntEvent->mbStuntRunEnded)
                {
                    // X360-drift skill 19 (unnamed enumerator; attested numeric id).
                    SetCurrentSkillScore(static_cast<EFreeburnSkill>(19), 0.0f, true);
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_INPROGRESS_STUNT:             // == raw 120
        {
            const GameStateModuleIO::InProgressStuntEvent* lpInProgressStuntEvent =
                reinterpret_cast<const GameStateModuleIO::InProgressStuntEvent*>(lpEvent);

            CGS_ASSERT(lpInProgressStuntEvent, "lpInProgressStuntEvent");   // :5466

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 1) == 1)
            {
                // Radians -> degrees -> whole rolls (the X360 inlines floor() as the
                // magic-constant fsel/round-and-adjust sequence).
                const f32 lfWholeBarrelRolls = static_cast<f32>(std::floor(
                    lpInProgressStuntEvent->mfInProgressBarrelRollAngle * 57.29578f * 0.0027777778f + 0.5f));

                SetCurrentSkillScore(E_FREEBURN_SKILL_BARREL_ROLL, lfWholeBarrelRolls, false);
                if (lpInProgressStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_BARREL_ROLL_REVERSE, lfWholeBarrelRolls, false);
                }
            }

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 2) == 2)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_FLATSPIN,
                                     lpInProgressStuntEvent->mfInProgressAirSpinAngle * 57.29578f, false);
                if (lpInProgressStuntEvent->mbInReverse)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_FLATSPIN_REVERSE,
                                         lpInProgressStuntEvent->mfInProgressAirSpinAngle * 57.29578f, false);
                }
            }

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 0x20) == 0x20)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_AIR, lpInProgressStuntEvent->mfTimeInAir, false);
            }

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 0x40) == 0x40)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_AIR_DISTANCE,
                                     lpInProgressStuntEvent->mfDistanceInAir, false);
            }

            if ((lpInProgressStuntEvent->muStuntActionInProgress & 0x80) != 0x80)
            {
                break;
            }
            if (mpCurrentChallenge == NULL)
            {
                break;
            }
            if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_RUNNING)
            {
                break;
            }

            // Locate the challenge's convoy (X360-drift type 22) and stunt-run (type 23) actions.
            s32 liConvoyAction   = -1;
            s32 liStuntRunAction = -1;
            for (s32 liActionIndex = 0;
                 liActionIndex < mpCurrentChallenge->GetNumActions();
                 ++liActionIndex)
            {
                if (static_cast<s32>(mpCurrentChallenge->GetAction(liActionIndex)->GetActionType()) == 22)
                {
                    liConvoyAction = liActionIndex;
                }
                if (static_cast<s32>(mpCurrentChallenge->GetAction(liActionIndex)->GetActionType()) == 23)
                {
                    liStuntRunAction = liActionIndex;
                }
            }

            if (liConvoyAction >= 0)
            {
                // Which challenge player is currently banking the longest convoy?
                s32                 liLongestConvoy       = 0;
                ::EActiveRaceCarIndex leLongestConvoyPlayer = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     lePlayer < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer])
                    {
                        const f32 lfContribution = maafCumulativeContributions[lePlayer][liConvoyAction];
                        if (lfContribution > static_cast<f32>(liLongestConvoy))
                        {
                            leLongestConvoyPlayer = lePlayer;
                            liLongestConvoy       = static_cast<s32>(lfContribution);
                        }
                    }
                }

                bool lbLocalConvoyIsLongest =
                    lpInProgressStuntEvent->miConvoyMemberCount > liLongestConvoy;
                if (lpInProgressStuntEvent->miConvoyMemberCount == liLongestConvoy)
                {
                    CGS_ASSERT(lpInProgressStuntEvent->miConvoyMemberCount > 0,
                               "Trying to score a zero length convoy\n");                        // :5571
                    CGS_ASSERT(leLongestConvoyPlayer != ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                               "The longest convoy doesn't have any cars in it!\n");             // :5572

                    // Tie-break: the convoy's tail car decides who owns the run.
                    lbLocalConvoyIsLongest =
                        lpInProgressStuntEvent->maConvoyMemberARCIs[lpInProgressStuntEvent->miConvoyMemberCount - 1]
                            == static_cast<s32>(leLongestConvoyPlayer);
                }

                if (lbLocalConvoyIsLongest)
                {
                    // Walk the convoy front-to-back to the local player, skipping (by counting
                    // down) any member that is not in the challenge.
                    s32 liPlayerPosition = 0;
                    for (s32 liMember = 0; liMember < lpInProgressStuntEvent->miConvoyMemberCount; ++liMember)
                    {
                        const s32 liMemberARCI = lpInProgressStuntEvent->maConvoyMemberARCIs[liMember];
                        if (!mabPlayerStartedChallenge[liMemberARCI])
                        {
                            --liPlayerPosition;
                        }
                        if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) == liMemberARCI)
                        {
                            break;
                        }
                        ++liPlayerPosition;
                    }

                    if (liPlayerPosition < 0)
                    {
                        // Runtime-valued diagnostic; X360 file/line BrnChallengeManager.cpp:5607.
                        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                        lStrStream << "Player in longest convoy but has an invalid position"
                                   << liPlayerPosition << "\n";
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                        CgsDev::Assert::EndAssert();
                    }

                    // X360-drift skill 19 (unnamed enumerator; attested numeric id).
                    SetCurrentSkillScore(static_cast<EFreeburnSkill>(19),
                                         static_cast<f32>(liPlayerPosition + 1), false);
                }
            }

            if (liStuntRunAction < 0)
            {
                break;
            }

            {
                const s32 liStuntRunSlot = mpCurrentChallenge->GetAction(liStuntRunAction)->GetTargetValue(1);
                const f32 lfStuntRunScore = lpInProgressStuntEvent->mafStuntRunScores[liStuntRunSlot - 1];
                if (lfStuntRunScore > 0.0f)
                {
                    // X360-drift skill 20 (unnamed enumerator; attested numeric id).
                    SetCurrentSkillScore(static_cast<EFreeburnSkill>(20), lfStuntRunScore, false);
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS:   // == raw 165
        {
            const GameStateModuleIO::FreeburnChallengeActionSuccessEvent* lpActionSuccessEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeActionSuccessEvent*>(lpEvent);

            CGS_ASSERT(lpActionSuccessEvent, "lpActionSuccessEvent");   // :5299

            if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_RUNNING)
            {
                break;
            }
            // 64-bit challenge-id gate (`ld 0(event); ld 0xC0(challenge); cmpld`).
            if (lpActionSuccessEvent->mChallengeID != mpCurrentChallenge->GetChallengeID())
            {
                break;
            }

            CGS_ASSERT(lpActionSuccessEvent->miActionIndex < mpCurrentChallenge->GetNumActions(),
                       "lpActionSuccessEvent->miActionIndex < mpCurrentChallenge->GetNumActions()");   // :5309

            const BrnResource::ChallengeListEntryAction* lpAction =
                mpCurrentChallenge->GetAction(lpActionSuccessEvent->miActionIndex);
            CGS_ASSERT(lpAction, "lpAction");   // :5311

            if (lpAction->GetCombineAction() != BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT)
            {
                // Advance the arbitration cursor past the action that just succeeded, clamping
                // both cursors to the last action slot.
                miCurrentArbitrationIndex = lpActionSuccessEvent->miActionIndex + 1;
                if (miCurrentArbitrationIndex >= mpCurrentChallenge->GetNumActions() - 1)
                {
                    miCurrentArbitrationIndex = mpCurrentChallenge->GetNumActions() - 1;
                }
                if (miCurrentArbitrationIndex >= miCurrentChallengeAction)
                {
                    miCurrentChallengeAction = miCurrentArbitrationIndex;
                    if (miCurrentChallengeAction >= mpCurrentChallenge->GetNumActions() - 1)
                    {
                        miCurrentChallengeAction = mpCurrentChallenge->GetNumActions() - 1;
                    }
                }
            }

            if (lpAction->GetCoopType() == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE)
            {
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     lePlayer < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer])
                    {
                        maaePlayersSuccessStatus[lePlayer][lpActionSuccessEvent->miActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
                    }
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET:            // == raw 166
        {
            const GameStateModuleIO::FreeburnChallengeResetEvent* lpResetEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeResetEvent*>(lpEvent);

            CGS_ASSERT(lpResetEvent, "lpResetEvent");   // :5351

            if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING &&
                lpResetEvent->mChallengeID == mpCurrentChallenge->GetChallengeID())
            {
                miCurrentChallengeAction  = lpResetEvent->miActionIndex;
                miCurrentArbitrationIndex = lpResetEvent->miActionIndex;
                ResetCurrentChallengeData(miCurrentChallengeAction);

                if (miLastChallengeResetFrame < miFramesSinceNetworkStart)
                {
                    miLastChallengeResetFrame = miFramesSinceNetworkStart;
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS:   // == raw 167
        {
            const GameStateModuleIO::FreeburnChallengeResetEvent* lpResetEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeResetEvent*>(lpEvent);

            CGS_ASSERT(lpResetEvent, "lpResetEvent");   // :5381

            if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING &&
                lpResetEvent->mChallengeID == mpCurrentChallenge->GetChallengeID())
            {
                miCurrentChallengeAction  = 0;
                miCurrentArbitrationIndex = 0;

                for (s32 liActionIndex = 0; liActionIndex < lpResetEvent->miActionIndex + 1; ++liActionIndex)
                {
                    ResetActionData(liActionIndex);
                }

                if (miLastChallengeResetFrame < miFramesSinceNetworkStart)
                {
                    miLastChallengeResetFrame = miFramesSinceNetworkStart;
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        case GameStateModuleIO::E_EVENT_ACTIVE_FREEBURN_CHALLENGE:            // == raw 173
        {
            const GameStateModuleIO::ActiveFburnChallengeEvent* lpActiveChallengeEvent =
                reinterpret_cast<const GameStateModuleIO::ActiveFburnChallengeEvent*>(lpEvent);

            CGS_ASSERT(lpActiveChallengeEvent, "lpActiveChallengeEvent");   // :5788

            if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_NONE)
            {
                for (s32 liIndex = 0; liIndex < lpActiveChallengeEvent->miNumPlayersInChallenge; ++liIndex)
                {
                    CGS_ASSERT(lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                               "lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] >= E_ACTIVE_RACE_CAR_INDEX_0");   // :5798
                    CGS_ASSERT(lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                               "lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // :5799

                    mabPlayerStartedChallenge[lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex]] = true;
                }

                mpCurrentChallenge =
                    mpFreeburnChallengeList->GetChallengeData(lpActiveChallengeEvent->mChallengeID);
            }
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// UpdateChallenge  (X360 0x82347190)
// ============================================================================
// The RUNNING master loop. Drives every action slot from the current action forward:
// UpdateAction scores it, UpdateActionSuccess broadcasts the transition, the failure/reset
// flavours rewind the challenge (posting the id-153 FreeburnChallengeAction reset messages),
// and the per-player banked/current score pair is finally published as the id-159
// FburnChallengeSuccessAction.
//
// Register map: r3 this, f1 lfTimeStep (kept in f30), r5 liFramesSinceNetworkStart,
// r6 lpActiveRaceCarOutputInterface, r7 lpActionQueue, r8 lbIsOnline.
void ChallengeManager::UpdateChallenge(f32 lfTimeStep, s32 liFramesSinceNetworkStart,
                                       const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                                       TGameActionQueue* lpActionQueue, bool lbIsOnline)
{
    CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");                       // :4255
    CGS_ASSERT(miCurrentArbitrationIndex <= miCurrentChallengeAction,
               "miCurrentArbitrationIndex <= miCurrentChallengeAction");        // :4256

    // The per-frame success broadcast is built up across the action loop and posted at the
    // tail; only the flags are cleared here (the scores are memcpy'd in just before the post).
    GameStateModuleIO::FburnChallengeSuccessAction lSuccessAction;
    lSuccessAction.mabSuccessfulActions[0] = false;
    lSuccessAction.mabSuccessfulActions[1] = false;

    // The accessor carries its own "Player car index hasn't been set" assert
    // (BrnRaceCarEntityModuleOutputInterface.h:980) -- do not duplicate it here.
    const ::EActiveRaceCarIndex leActiveRaceCarIndex =
        lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex();

    bool lbPostSuccessAction = false;

    for (s32 liActionIndex = miCurrentChallengeAction;
         liActionIndex < mpCurrentChallenge->GetNumActions();
         ++liActionIndex)
    {
        // GetAction inlines its own "liActionIndex >= 0" / "liActionIndex <
        // KI_MAX_ACTIONS_PER_CHALLENGE" range asserts (ChallengeListEntry.h:941/942).
        const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liActionIndex);
        CGS_ASSERT(lpAction, "lpAction");   // :4271

        // NOTE: the X360 passes a literal false for UpdateAction's lbIsOnline (`li r8, 0`),
        // NOT this function's lbIsOnline parameter.
        const EChallengeStatus leActionStatus =
            UpdateAction(liActionIndex, lpAction, lfTimeStep, lpActiveRaceCarOutputInterface, false);

        lSuccessAction.mabSuccessfulActions[liActionIndex] =
            (leActionStatus == E_CHALLENGE_STATUS_SUCCESS &&
             lpAction->GetCoopType() != BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_SIMULTANEOUS);
        lSuccessAction.mabAccumulationThisFrame[liActionIndex] = false;

        if (maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] ==
            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
        {
            if (lSuccessAction.mabSuccessfulActions[liActionIndex])
            {
                // GetActionType inlines the X360 (drifted, 41-entry) range assert.
                const s32 liFreeburnSkill =
                    KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
                if (liFreeburnSkill != KI_FREEBURN_SKILL_COUNT_X360 &&
                    mabBankedSkillThisFrame[liFreeburnSkill])
                {
                    const f32 lfCurrentActionScore = maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                    if (lpAction->GetCoopType() ==
                        BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL)
                    {
                        if (lfCurrentActionScore >= maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex])
                        {
                            if (lfCurrentActionScore < 0.0f)
                            {
                                // X360 file/line BrnChallengeManager.cpp:4337.
                                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                                lStrStream << "We have banked an action score but have not set a valid current score: "
                                           << lfCurrentActionScore
                                           << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                                           << " Action Index: " << liActionIndex
                                           << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                                CgsDev::Assert::BeginAssert();
                                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                                CgsDev::Assert::EndAssert();
                            }
                            lbPostSuccessAction = true;
                        }
                    }
                    else
                    {
                        if (lfCurrentActionScore < 0.0f)
                        {
                            // X360 file/line BrnChallengeManager.cpp:4348.
                            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                            lStrStream << "We have banked an action score but have not set a valid current score: "
                                       << lfCurrentActionScore
                                       << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                                       << " Action Index: " << liActionIndex
                                       << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                            CgsDev::Assert::BeginAssert();
                            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                            CgsDev::Assert::EndAssert();
                        }
                        lbPostSuccessAction = true;
                    }
                }
            }
        }
        else if (lSuccessAction.mabSuccessfulActions[liActionIndex])
        {
            const f32 lfCurrentActionScore = maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
            if (lfCurrentActionScore < 0.0f)
            {
                // X360 file/line BrnChallengeManager.cpp:4290.
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "We have banked an action score but have not set a valid current score: "
                           << lfCurrentActionScore
                           << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                           << " Action Index: " << liActionIndex
                           << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
            lbPostSuccessAction = true;
        }
        else if (lpAction->GetCoopType() ==
                 BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION)
        {
            const s32 liFreeburnSkill =
                KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[lpAction->GetActionType()];
            if (liFreeburnSkill != KI_FREEBURN_SKILL_COUNT_X360 &&
                mabBankedSkillThisFrame[liFreeburnSkill])
            {
                const f32 lfCurrentActionScore = maafCurrentActionsScores[leActiveRaceCarIndex][liActionIndex];
                if (lfCurrentActionScore < 0.0f)
                {
                    // X360 file/line BrnChallengeManager.cpp:4308.
                    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "We have banked an action score but have not set a valid current score: "
                               << lfCurrentActionScore
                               << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                               << " Action Index: " << liActionIndex
                               << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                lbPostSuccessAction = true;
                lSuccessAction.mabAccumulationThisFrame[liActionIndex] = true;
            }
        }

        // NOTE: the 5th argument (the header spells it liNumPlayers) is this frame counter --
        // the X360 threads its own r5 parameter straight through.
        UpdateActionSuccess(lpAction, liActionIndex, leActionStatus, leActiveRaceCarIndex,
                            liFramesSinceNetworkStart, lpActionQueue, lbIsOnline);

        if (lpAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_CHAIN)
        {
            if (leActionStatus == E_CHALLENGE_STATUS_SUCCESS)
            {
                ++miCurrentChallengeAction;
                continue;
            }
        }
        else if (leActionStatus == E_CHALLENGE_STATUS_SUCCESS)
        {
            continue;
        }

        if (leActionStatus == E_CHALLENGE_STATUS_RESET_IF_NEEDED)
        {
            if (maaePlayersSuccessStatus[leActiveRaceCarIndex][liActionIndex] !=
                GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)
            {
                if (lpAction->GetCombineAction() ==
                    BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_FAILURE_RESETS_CHAIN)
                {
                    // Rewind the whole chain locally: restock the remaining targets and clear
                    // this player's per-action state.
                    miCurrentChallengeAction  = 0;
                    miCurrentArbitrationIndex = 0;

                    for (s32 liResetIndex = 0;
                         liResetIndex < mpCurrentChallenge->GetNumActions();
                         ++liResetIndex)
                    {
                        maiRemainingTarget[liResetIndex] =
                            mpCurrentChallenge->GetAction(liResetIndex)->GetTargetValue(0);
                    }

                    for (s32 liResetIndex = 0;
                         liResetIndex < mpCurrentChallenge->GetNumActions();
                         ++liResetIndex)
                    {
                        maafCumulativeContributions[leActiveRaceCarIndex][liResetIndex] = 0.0f;
                        maaePlayersSuccessStatus[leActiveRaceCarIndex][liResetIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
                        mabIndividualActionsSuccessUpdateSent[liResetIndex] = false;

                        for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START;
                             static_cast<s32>(leSkill) < KI_FREEBURN_SKILL_COUNT_X360; leSkill++)
                        {
                            mafBankedActionScores[leSkill] = 0.0f;
                        }
                    }
                }
                else if (lpAction->GetCombineAction() ==
                         BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_FAILURE_RESETS_EVERYONE)
                {
                    // Rewind everyone: reset the local data and broadcast the reset.
                    miCurrentChallengeAction  = 0;
                    miCurrentArbitrationIndex = 0;

                    GameStateModuleIO::FreeburnChallengeAction lResetAction;
                    lResetAction.mChallengeID   = mpCurrentChallenge->GetChallengeID();
                    lResetAction.meEventType    = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_RESET;   // 3
                    lResetAction.meChallengeStatus       = E_CHALLENGE_STATUS_ONGOING;
                    lResetAction.miActionIndex           = 0;
                    lResetAction.miNumChallengesComplete = -1;
                    lResetAction.miTotalNumChallenges    = -1;
                    lResetAction.mbIsHost                = false;
                    lResetAction.mbAbortingToStartNewChallenge = false;

                    ResetCurrentChallengeData(0);

                    CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :4431
                    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetAction),
                                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE, 0x20);

                    if (miLastChallengeResetFrame < miFramesSinceNetworkStart)
                    {
                        miLastChallengeResetFrame = miFramesSinceNetworkStart;
                    }
                }
            }

            if (lpAction->GetCoopType() == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT ||
                lpAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_SIMULTANEOUS)
            {
                // Reset every action slot up to (and including) the one before the cursor and
                // tell the other machines about it.
                s32 liLastResetAction = miCurrentChallengeAction - 1;
                if (liLastResetAction <= 0)
                {
                    liLastResetAction = 0;
                }
                miCurrentChallengeAction  = 0;
                miCurrentArbitrationIndex = 0;

                GameStateModuleIO::FreeburnChallengeAction lResetAction;
                lResetAction.mChallengeID = mpCurrentChallenge->GetChallengeID();
                // X360 DRIFT: raw enumerator 4 -- the X360 EChallengeEventType carries one extra
                // (unattested) enumerator ahead of ENDED, so this is NOT E_CHALLENGE_EVENT_ENDED.
                lResetAction.meEventType             = 4;
                lResetAction.meChallengeStatus       = E_CHALLENGE_STATUS_ONGOING;
                lResetAction.miActionIndex           = liLastResetAction;
                lResetAction.miNumChallengesComplete = -1;
                lResetAction.miTotalNumChallenges    = -1;
                lResetAction.mbIsHost                = false;
                lResetAction.mbAbortingToStartNewChallenge = false;

                for (s32 liResetIndex = 0; liResetIndex < liLastResetAction + 1; ++liResetIndex)
                {
                    ResetActionData(liResetIndex);
                }

                CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :4469
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetAction),
                                        GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE, 0x20);

                if (miLastChallengeResetFrame < miFramesSinceNetworkStart)
                {
                    miLastChallengeResetFrame = miFramesSinceNetworkStart;
                }
            }
        }

        if (liActionIndex > 0)
        {
            // An INDEPENDENT predecessor means the earlier slots don't feed this broadcast.
            if (mpCurrentChallenge->GetAction(liActionIndex - 1)->GetCombineAction() ==
                BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT)
            {
                lbPostSuccessAction = false;
                for (s32 liPreviousIndex = 0; liPreviousIndex < liActionIndex; ++liPreviousIndex)
                {
                    maaePlayersSuccessStatus[leActiveRaceCarIndex][liPreviousIndex] =
                        GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
                }
            }
        }

        if (lpAction->GetCombineAction() != BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT)
        {
            break;
        }
    }

    if (lbPostSuccessAction)
    {
        std::memcpy(lSuccessAction.mafActionScores, maafCurrentActionsScores[leActiveRaceCarIndex],
                    sizeof(lSuccessAction.mafActionScores));   // X360 XMemCpy(dest, src, 8)

        for (s32 liScoreIndex = 0; liScoreIndex < KI_MAX_CHALLENGE_ACTIONS; ++liScoreIndex)
        {
            if (lSuccessAction.mafActionScores[liScoreIndex] < 0.0f)
            {
                // X360 file/line BrnChallengeManager.cpp:4519.
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Negative successful action score reported: "
                           << lSuccessAction.mafActionScores[liScoreIndex]
                           << " Challenge ID: " << mpCurrentChallenge->GetChallengeID()
                           << " Action Index: " << liScoreIndex
                           << "/" << mpCurrentChallenge->GetNumActions() << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
        }

        CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :4523
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSuccessAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SUCCESS, 0xC);
    }
}

}   // namespace BrnGameState
