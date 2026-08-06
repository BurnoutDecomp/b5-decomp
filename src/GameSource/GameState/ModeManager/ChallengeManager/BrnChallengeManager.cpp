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

#include <cstddef>   // offsetof (layout assert)

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
