#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

// OnModeStart dereferences GameModeParams (the keystone only forward-declares it): the
// road-rage / marked-man case reads the per-mode crash target through GetAStarDistanceFunctionRaw()
// (the +0x858 word) and the online block reads the network-player count (miNumNetworkPlayers).
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// BrnScoringSystem_Lifecycle.cpp
// ============================================================================
// Bodies for the "Lifecycle" group of BrnGameState::ScoringSystem (keystone header
// BrnScoringSystem.h lines 280-303): construct / prepare / release / mode hooks and
// the player roster (AddPlayer / SetPlayerRaceCarIndex / RemovePlayer).
//
// Each body is reconstructed from its X360 pseudocode (addresses in the per-method
// comments) and the DecFIGS dwarfdump variable hints, accessing ScoringSystem members
// BY NAME. The X360 search/scan loops walk the per-car records (maCarData[8]); the
// optimizer's pointer-stride form (++ptr by 86 dwords == 344 bytes == sizeof(CarData)
// on X360) is reconstructed back to clean indexed loops over E_PLAYER_SCORING_INDEX_COUNT.
//
// CgsNetwork::K_INVALID_PLAYER_ID has no committed home in the tree; the file-local -1
// (the BrnScoringSystem_Lookup.cpp / FlybyManager precedent) stands in for it.
//
// SCOPE NOTE -- the keystone is a SEMANTIC slice, not a byte-exact X360 layout. The X360
// ScoringSystem also embeds members the keystone deliberately omits (a CgsDev::DebugComponent
// base, a second StuntModeScoring instance, an OnlineGameResults aggregate and the
// OnlineStuntRunModeScoring sub-scorer). Construct/Prepare touch some of those; the bodies
// here reconstruct the operations that map to keystone-named members and delegate the rest to
// the embedded sub-scorers' own lifecycle calls. OnModeStart and WriteDataToOutput are now
// RECONSTRUCTED (2026-06-18) -- see their own headers for the precise residual flags. Only
// SetRivalEliminated stays declare-only (no standalone X360 export -- fully inlined into callers).
// ============================================================================

namespace BrnGameState
{
namespace
{
    // CgsNetwork::K_INVALID_PLAYER_ID stand-in (no committed home; -1 per existing precedent).
    const BrnNetwork::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

// ----------------------------------------------------------------------------
// construction
// ----------------------------------------------------------------------------

// X360 0x827E0998. Default constructor.
//
// The X360 ctor body decomposes into four kinds of store, three of which are AUTOMATIC in clean
// C++ and so are NOT replayed here:
//   (1) Embedded sub-object vtable pointers -- ss+0x20 (mCrashModeScoring), ss+0x350
//       (mStuntModeScoring), ss+0x2620 (mOnlineStuntModeScoring), ss+0x4B74 (mOnlineRaceModeScoring),
//       ss+0x4BF8 (mOnlineRoadRageScoring), ss+0x4CC0 (mOnlineBurningHomeRunScoring), ss+0x4D44
//       (mOnlineStuntRunModeScoring). Each is set by that sub-object's OWN constructor; hand-setting
//       vtable pointers is forbidden and unnecessary.
//   (2) The -1 sentinels the asm stores at ss+0x29C (668), ss+0x4A90 (19088) and ss+0x4B10 (19216)
//       all fall INSIDE embedded sub-scorers (the crash scorer @0x20 resp. the road-rage scorer that
//       precedes mOnlineRaceModeScoring), so they too are set by those sub-objects' ctors.
//   (3) The four head CgsSystem::Time members (mStartTime/mEndTime/mTotalTime/mTimeRemaining at
//       ss+0x00/0x08/0x10/0x18) -- the asm zero-stores miSeconds=0 / mfFraction=0.0f for each, which
//       is exactly what CgsSystem::Time's default ctor produces, so the implicit member init covers it.
//   (4) The per-car array construction loop (_vector_constructor_iterator_ over maCarData[8] +
//       maRaceCarPositioningData[8] + maBurnoutSkillzData[8] and the inner arrays) -- automatic via
//       the array members' element constructors.
//
// The SINGLE ScoringSystem-owned scalar the asm explicitly stores is the very last instruction
// (stw r28(=-1), 0x5D40(r31)): ss+0x5D40 == 23872 == miUpdateRacePositionsPM (proven by the
// "miUpdateRacePositionsPM >= 0" assert + its `== -1` sentinel test elsewhere in the class). Every
// other ScoringSystem scalar is left untouched by the ctor -- the post-construction ClearData() pass
// (run from Construct) seeds them -- so we faithfully leave them unset here as well.
ScoringSystem::ScoringSystem()
    : miUpdateRacePositionsPM(-1)   // X360 0x827E0B14: stw r28(=-1), 0x5D40(r31)
{
}

// ----------------------------------------------------------------------------
// lifecycle / mode hooks
// ----------------------------------------------------------------------------

// X360 0x82337FE0. One-time construction: bring each embedded sub-scorer up (the crash scorer
// clears, the stunt scorer takes the achievement manager, the three online scorers construct),
// reset all scoring state via ClearData(true), clear the disconnected-player set, and empty the
// burnout-skillz tally (every record cleared, every parallel id slot freed to INVALID).
void ScoringSystem::Construct(StuntModeScoring::AchievementManager* lpAchievementManager)
{
    mCrashModeScoring.ClearData();
    mStuntModeScoring.Construct(lpAchievementManager);

    mOnlineRaceModeScoring.Construct();
    mOnlineRoadRageScoring.Construct();
    mOnlineBurningHomeRunScoring.Construct();

    ClearData(true);
    ClearDisconnectedPlayers();

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        maBurnoutSkillzData[liSlot].Clear();
        maBurnoutSkillzPlayerIDs[liSlot] = K_INVALID_PLAYER_ID;
    }

    meCurrentMedalTarget   = E_CURRENT_MEDAL_TARGET_TIME_NONE;
    meCurrentMedalAchieved = E_CURRENT_MEDAL_TARGET_TIME_NONE;
}

// X360 0x8232A430. Allocate-time prepare: ready the stunt sub-scorer, then prepare each of the
// eight per-car records against the network heap allocator. Always returns true on the X360.
bool ScoringSystem::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
{
    mStuntModeScoring.Prepare();

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        maCarData[liSlot].Prepare(lpHeapMalloc);
    }

    return true;
}

// X360 0x823124A0. Release every per-car record (each frees its road-rule high-score table back
// to the network heap). Always returns true on the X360.
bool ScoringSystem::Release()
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        maCarData[liSlot].Release();
    }

    return true;
}

// :333. The mode-end hook has no out-of-line work in the recovered build (the DecFIGS dwarfdump
// body is empty); the per-mode teardown lives in the callers.
void ScoringSystem::OnModeEnd(bool /*lbAbort*/)
{
}

// X360 0x8231F140. Per-car cumulative-data reset across all eight slots, then clear the per-event
// online results aggregate. The PS3 DecFIGS dwarfdump SHAPE shows only the per-car
// CarData::ClearCumulativeData() sweep (the PS3 layout has no mOnlineGameResults member); the X360
// body (0x8231F140) additionally ends with OnlineGameResults::Clear(this+0x4DD0) -- now reconstructed
// here as mOnlineGameResults.Clear() since that member landed by-value on the keystone (the
// per-car loop emits the inlined `miCumulativePoints := 0; miRoundDisconnectedIn := -1` pair the
// X360 stores at CarData +0x130/+0x134, which CarData::ClearCumulativeData() performs).
void ScoringSystem::ClearCumulativeData()
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        maCarData[liSlot].ClearCumulativeData();
    }

    mOnlineGameResults.Clear();   // X360 0x8231F190: OnlineGameResults::Clear(this+0x4DD0)
}

// ----------------------------------------------------------------------------
// player roster
// ----------------------------------------------------------------------------

// X360 0x8231E288. Claim the first free per-car slot (a slot is free when its stored race-car
// index is the COUNT sentinel), mark it pending (race-car index + network id both INVALID), reset
// its road-rule high scores, and return the slot as the new player's scoring index. Fires the
// "no free slots" assert and returns the COUNT sentinel when the roster is full.
GameStateModuleIO::EPlayerScoringIndex ScoringSystem::AddPlayer()
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_COUNT)
        {
            maCarData[liSlot].SetActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID);
            maCarData[liSlot].SetNetworkPlayerID(K_INVALID_PLAYER_ID);
            maCarData[liSlot].ResetRoadRulesScores();
            return static_cast<GameStateModuleIO::EPlayerScoringIndex>(liSlot);
        }
    }

    CGS_ASSERT(false, "No more free slots for a player!");
    return GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
}

// X360 sub_8231E340 (the AddPlayer(NetworkPlayerID, EPlayerTeam) overload). Claim a free slot via
// the no-arg AddPlayer(), assert the id is not already registered, then stamp the slot's team,
// round-start team and network id. Returns the new scoring index.
GameStateModuleIO::EPlayerScoringIndex ScoringSystem::AddPlayer(BrnNetwork::NetworkPlayerID lID,
                                                                GameStateModuleIO::EPlayerTeam leTeam)
{
    GameStateModuleIO::EPlayerScoringIndex leScoringIndex = AddPlayer();

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        CGS_ASSERT((maCarData[liSlot].GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_COUNT) ||
                   (lID != maCarData[liSlot].GetNetworkPlayerID()),
                   "lNetworkPlayerID != maCarData[leCheckPlayerIndex].GetNetworkPlayerID()");
    }

    CarData* lpCarData = GetCarDataFromPlayerScoringIndex(leScoringIndex);
    lpCarData->SetTeam(leTeam);
    lpCarData->SetRoundStartTeam(leTeam);
    lpCarData->SetNetworkPlayerID(lID);

    return leScoringIndex;
}

// X360 0x82310FB0. Bind a scoring slot to its active-race-car index. Asserts the slot is either
// unbound (INVALID) or already bound to the same race car, then stores the index.
void ScoringSystem::SetPlayerRaceCarIndex(GameStateModuleIO::EPlayerScoringIndex leScoringIndex,
                                          EActiveRaceCarIndex leRaceCarIndex)
{
    CarData* lpCarData = GetCarDataFromPlayerScoringIndex(leScoringIndex);

    CGS_ASSERT((lpCarData->GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID) ||
               (lpCarData->GetActiveRaceCarIndex() == leRaceCarIndex),
               "(maCarData[ lePlayerIndex ].GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID) || "
               "(maCarData[ lePlayerIndex ].GetActiveRaceCarIndex() == leActiveRaceCarIndex)");

    lpCarData->SetActiveRaceCarIndex(leRaceCarIndex);
}

// :358. Remove the player occupying a given active-race-car slot: reset the record and drop the
// lead / last bookkeeping if it referenced the removed car. (The X360 keys its single out-of-line
// RemovePlayer on the network id -- see the NetworkPlayerID overload below; this race-car-index
// overload mirrors that reset path through the by-index record lookup.)
void ScoringSystem::RemovePlayer(EActiveRaceCarIndex leRaceCarIndex)
{
    CarData* lpCarData = GetCarData(leRaceCarIndex);
    if (lpCarData != NULL)
    {
        lpCarData->Construct();
        lpCarData->ResetRoadRulesScores();

        EActiveRaceCarIndex leRemovedIndex = lpCarData->GetActiveRaceCarIndex();
        if (leRemovedIndex == meLastRaceCarIndex)
        {
            meLastRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }
        if (leRemovedIndex == meLeadRaceCarIndex)
        {
            meLeadRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }
    }
}

// :362. No recoverable out-of-line body (no X360 export and an empty DecFIGS dwarfdump entry);
// the rival-eliminated marking is fully inlined into its callers in the recovered build.
// FLAGGED BLOCKED -- left declare-only (no reconstruction source).

// :367. Remove the player with the given network id: look the record up by id, reset it, and drop
// the lead / last bookkeeping if it referenced the removed car. This is the X360 0x8236B0A0 body
// (sub_8231DD88 == GetCarData(NetworkPlayerID)).
void ScoringSystem::RemovePlayer(BrnNetwork::NetworkPlayerID lID)
{
    CarData* lpCarData = GetCarData(lID);
    if (lpCarData != NULL)
    {
        lpCarData->Construct();
        lpCarData->ResetRoadRulesScores();

        EActiveRaceCarIndex leRemovedIndex = lpCarData->GetActiveRaceCarIndex();
        if (leRemovedIndex == meLastRaceCarIndex)
        {
            meLastRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }
        if (leRemovedIndex == meLeadRaceCarIndex)
        {
            meLeadRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }
    }
}

// :330 / X360 0x82338220 -- OnModeStart.
// Per-mode start hook: reset all scoring state (ClearData), seed the per-mode targets, then -- for the
// online modes -- stamp the online-results aggregate's event header. Reconstructed from the ASM.
//
// SCOPE / RESIDUAL FLAGS:
//   * Road-rage / marked-man case (X360 jumptable cases 0,5 == game modes E_MODE_ROAD_RAGE(3) /
//     E_MODE_MARKED_MAN(8)): copies GameModeParams +0x858 VERBATIM into miMaximumPlayerCrashedNumber via
//     the committed GetAStarDistanceFunctionRaw() accessor (the GameModeParamsField prereq) -- the task's
//     headline deliverable, fully grounded.
//   * Stunt-attack / online-stunt-run cases (game modes 7 and 12/14/17): Activate the offline
//     (mStuntModeScoring) resp. the online (mOnlineStuntModeScoring) stunt scorer with the target score
//     read from GameModeParams +0x68 (ASM `lfs f0, 0x68(r29); fctiwz` -- a float narrowed to s32). +0x68
//     resolves to mfNeedForSilver by member-offset reconstruction (the DWARF dwarfdump carries no byte
//     offsets, so this is an alignment-derived identity, MEDIUM confidence -- FLAGGED; if it proves to be
//     an adjacent threshold the named member swaps but the shape is unchanged). StuntModeScoring::Activate
//     is declare-only on the slice (fine for `cl /c`).
//   * Online block (game mode >= 10): miEventType <- leGameMode and miReserved0x30 <- (network-player
//     count + 1) are reconstructed against named mOnlineGameResults members (the same fields
//     SaveNetworkRoundData reads back). miNumberOfRounds (ASM `stw r28, 0x4DF8` <- r7) is FLAGGED: r7 is
//     a 4th register argument the recovered DWARF C++ signature (3 params) -- and therefore the keystone
//     declaration -- drops; there is no in-signature source for it, so the round count is left unwritten
//     here rather than fabricated. Growing the keystone signature to carry the 4th arg is a separate
//     additive change in the keystone home, out of scope for this partial.
void ScoringSystem::OnModeStart(GameStateModuleIO::EGameModeType leGameMode,
                                const GameModeParams* lpParams, bool lbRestart)
{
    ClearData(lbRestart);

    switch (leGameMode)
    {
        case GameStateModuleIO::E_MODE_ROAD_RAGE:
        case GameStateModuleIO::E_MODE_MARKED_MAN:
            // X360 jumptable cases 0,5: lwz r11,0x858(params) / stw r11,0x4B58(this). Verbatim copy.
            miMaximumPlayerCrashedNumber = lpParams->GetAStarDistanceFunctionRaw();
            break;

        case GameStateModuleIO::E_MODE_STUNT_ATTACK:
            // X360 case 4: Activate(this+0x350, (s32)*(params+0x68)). Offline stunt scorer.
            // FLAG: +0x68 == mfNeedForSilver by offset reconstruction (medium confidence).
            mStuntModeScoring.Activate(static_cast<s32>(lpParams->mfNeedForSilver));
            break;

        case GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:
        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:
        case GameStateModuleIO::E_MODE_ONLINE_MODE_END:
            // X360 cases 9,11,14: Activate(this+0x2620, (s32)*(params+0x68)). Online stunt scorer.
            mOnlineStuntModeScoring.Activate(static_cast<s32>(lpParams->mfNeedForSilver));
            break;

        default:
            break;
    }

    // Post-switch per-mode resets (ASM stw r11,0x4B5C / stb r11,0x4B70 with r11 == 0).
    miCurrentPlayerCrashedNumber = 0;
    mbPlayerTotalled             = false;

    // Online modes (game mode >= 10 == E_MODE_ONLINE_MODE_START): stamp the online-results event header.
    if (leGameMode >= GameStateModuleIO::E_MODE_ONLINE_MODE_START)
    {
        // FLAG: miNumberOfRounds <- r7 (4th register arg absent from the keystone's 3-param signature);
        //       left unwritten -- see this method's header note.
        mOnlineGameResults.miEventType    = leGameMode;                       // ASM stw r30,0x4E04
        mOnlineGameResults.miReserved0x30 = lpParams->miNumNetworkPlayers + 1; // ASM lbz 1(params) / extsb / +1 / stw 0x4E00
    }
}

// :374 / X360 0x8232AE98 -- WriteDataToOutput.
// Publishes the per-game scoring snapshot into the (offline) ScoringOutputInterface and, for online
// modes, drives the current online sub-scorer to fill the OnlineScoringOutputInterface. Reconstructed
// from the X360 ASM @ 0x8232AE98..0x8232B27C; members + output fields accessed BY NAME.
//
// All blockers from the prior round are now resolved by the prereq grows:
//   * SharedIOShapes added ScoringOutputInterface::maiTeamStuntScores[9] (+0xA10) and
//     mePlayerRaceCarIndex (+0xA38) -- so the per-team stunt-score publication loop and the car-count
//     store both land on named members.
//   * KeystoneMembers / online-scorer prereqs declared mpCurrentOnlineModeScoring's BaseOnlineModeScoring
//     vtable (slot 5 Update(const ScoringSystem*, s32) == X360 vtable+0x14; slot 8
//     WriteDataToOutput(OnlineScoringOutputInterface*) == X360 vtable+0x20) and the per-team helpers
//     GetTeamStuntScore / GetLeadingStuntTeam (BrnScoringSystem_Standings.cpp) + OnlineStuntRunModeScoring::GetTeamScore.
//   * The DWARF dwarfdump for this body (BrnScoringSystem.cpp:799) confirms the SOURCE reads the embedded
//     sub-scorers through getters (it names CrashModeScoring::GetNumCarsCrashed + StuntModeScoring::GetCurrentStunts);
//     the X360 free build inlined those getters to raw offsets, so this reconstruction calls the getters
//     by name rather than replaying the inlined offset math (semantic parity, not byte-exact).
//
// 4TH ARG NOW IN-SIGNATURE (2026-06-18). The keystone declaration was retyped to carry the hidden X360
// r7 (== ASM pseudocode `a5`, HIDWORD(v9)) as `EActiveRaceCarIndex lePlayerRaceCarIndex`, so the prior
// LABEL_26-only fallback is replaced by the real branch. ASM grounding of where r7 actually flows:
//   * out.mePlayerRaceCarIndex (out+0xA38): the ASM stores `*(a1+0x4EE8)` == muCarsInCurrentMode here,
//     NOT r7 -- so this remains the car-count store (unchanged from the prior round). The task brief's
//     listing of mePlayerRaceCarIndex as an r7 site is a misread of this slot; the ASM is authoritative.
//   * online Update(this, *(a1+0x4EE8)) 2nd arg: again muCarsInCurrentMode (a car count), NOT r7.
//   * online WriteDataToOutput(a3) dispatch arg: a3 == lpOnlineOutput, carries no car index.
//   * online-stunt sub-branch (ASM 6670-6687): the SOLE genuine r7 consumer. When online AND
//     lePlayerRaceCarIndex is a valid index (!= INVALID), the body looks the car up via GetCarData(r7),
//     reads its team, and publishes out.miCurrentScore = mOnlineStuntRunModeScoring.GetTeamScore(team)
//     and out.miTargetScore = GetTeamStuntScore(GetLeadingStuntTeam(team)). When r7 == -1 (ASM LABEL_26),
//     it falls back to reading the stunt-display scores directly off the selected scorer. Both paths are
//     now reconstructed faithfully against lePlayerRaceCarIndex.
//     (OnlineStuntRunModeScoring::GetTeamScore is declare-only/BLOCKED on the CarScoreData +0xD4 stunt
//     accessor in its home header -- fine for `cl /c`; the call site here is signature-complete.)
void ScoringSystem::WriteDataToOutput(GameStateModuleIO::ScoringOutputInterface* lpOutput,
                                      GameStateModuleIO::OnlineScoringOutputInterface* lpOnlineOutput,
                                      bool lbOnline,
                                      EActiveRaceCarIndex lePlayerRaceCarIndex)
{
    GameStateModuleIO::ScoringOutputInterface& lrOut = *lpOutput;

    // Empty stand-in record for slots with no live car (X360 builds a cleared CarScoreData on the stack).
    GameStateModuleIO::CarScoreData lEmptyScoreData;
    lEmptyScoreData.ClearData();

    // ---- per-car block: copy each car's score record + the cumulative/id/eliminated scalars ----
    for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
    {
        const EActiveRaceCarIndex leCar = static_cast<EActiveRaceCarIndex>(liSlot);
        const CarData* lpCarData = GetCarData(leCar);

        lrOut.maCarScoreData[liSlot]        = (lpCarData != NULL) ? *lpCarData->GetScoreData() : lEmptyScoreData;
        lrOut.maiCumulativeScoreData[liSlot]= (lpCarData != NULL) ? lpCarData->GetCumulativePoints() : 0;
        // Car id + eliminated flag live on CarData (the id) / its embedded CarScoreData (the flag);
        // the empty-slot fallbacks are a zero id and the cleared record's eliminated flag.
        lrOut.maCarIds[liSlot]              = (lpCarData != NULL) ? lpCarData->GetCarID() : CgsID(0);
        lrOut.mabPlayerEliminated[liSlot]   = (lpCarData != NULL) ? lpCarData->GetScoreData()->GetEliminated()
                                                                  : lEmptyScoreData.GetEliminated();
    }

    // ASM lwz 0x4EE8 / stw 0xA38(out): the active-car count published into the output's race-car-index slot.
    lrOut.mePlayerRaceCarIndex = static_cast<EActiveRaceCarIndex>(muCarsInCurrentMode);

    // ---- online half: drive the current online sub-scorer ----
    if (lbOnline)
    {
        CGS_ASSERT(mpCurrentOnlineModeScoring != NULL, "mpCurrentOnlineModeScoring");
        mpCurrentOnlineModeScoring->Update(this, static_cast<s32>(muCarsInCurrentMode));
        // SLICE-IDENTITY NOTE: BaseOnlineModeScoring::WriteDataToOutput (BrnBaseOnlineModeScoring.h:57) is
        // declared against the header's forward-only placeholder `BrnGameState::OnlineScoringOutputInterface`
        // (a documented pointer-only stand-in that "self-completes when that TU is worked"), NOT the real
        // GameStateModuleIO::OnlineScoringOutputInterface that lpOnlineOutput points at. The X360 dispatches
        // this through the vtable on the same byte address, so the two are the same object; bridge the
        // slice's placeholder/real type split with a pointer reinterpret_cast (both are incomplete-pointer
        // compatible). When the online-scorer TU lands and unifies the type, this cast drops out.
        mpCurrentOnlineModeScoring->WriteDataToOutput(
            reinterpret_cast<OnlineScoringOutputInterface*>(lpOnlineOutput));
    }

    // ---- road-rage scalars (gated on the road-rage scorer being active) ----
    if (mRoadRageModeScoring.IsActive())
    {
        lrOut.miRoadRageNumTakedowns  = mRoadRageModeScoring.GetNumTakedownsAchieved();
        lrOut.miRoadRageTakedownTarget= mRoadRageModeScoring.GetTargetNumTakedowns();
    }
    else
    {
        lrOut.miRoadRageNumTakedowns   = 0;
        lrOut.miRoadRageTakedownTarget = 0;
    }

    // ---- per-team stunt-score publication (X360 loop t in [0, KI_MAX_PLAYER_TEAMS): out[t] = GetTeamStuntScore(t)) ----
    for (s32 liTeam = 0; liTeam < KI_MAX_PLAYER_TEAMS; ++liTeam)
    {
        lrOut.maiTeamStuntScores[liTeam] = GetTeamStuntScore(liTeam);
    }

    // ---- showtime / crash scalars (DWARF: source reads these through CrashModeScoring getters) ----
    lrOut.miShowtimeCarsCrashed      = mCrashModeScoring.GetNumCarsCrashed();    // ASM: sum of maiNumCarsCrashed[4]
    lrOut.miShowtimeScoreMultiplier  = mCrashModeScoring.GetScoreMultiplier();
    lrOut.miShowtimeComboMultiplier  = mCrashModeScoring.GetCurrentComboCount();
    lrOut.mfShowtimeDistanceTravelled= mCrashModeScoring.GetDistanceTravelled();

    // ---- stunt block: offline scorer (mStuntModeScoring) vs online scorer (mOnlineStuntModeScoring) ----
    // ASM: v20 = online scorer (this+0x2620 == mOnlineStuntModeScoring) when online, else this+0x350 ==
    // mStuntModeScoring. miCurrentScore/miTargetScore have TWO sources depending on the 4th arg:
    StuntModeScoring& lrStunt = lbOnline ? mOnlineStuntModeScoring : mStuntModeScoring;

    if (lbOnline && lePlayerRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        // ASM 6670-6687 -- online AND a valid player index: publish the player's team stunt scores.
        CGS_ASSERT((lePlayerRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                   (lePlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leActiveRaceCarIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpPlayerCarData = GetCarData(lePlayerRaceCarIndex);
        const s32 liTeam = (lpPlayerCarData != NULL) ? static_cast<s32>(lpPlayerCarData->GetTeam()) : 0;

        // ASM: out+0xA64 = mOnlineStuntRunModeScoring.GetTeamScore(team, this);
        //      out+0xA68 = GetTeamStuntScore(GetLeadingStuntTeam(team)).
        lrOut.miCurrentScore = mOnlineStuntRunModeScoring.GetTeamScore(liTeam, this);
        lrOut.miTargetScore  = GetTeamStuntScore(GetLeadingStuntTeam(liTeam));
    }
    else
    {
        // ASM LABEL_26 -- offline, or online with no valid index (r7 == -1): read directly off the scorer.
        lrOut.miCurrentScore = lrStunt.GetCurrentScore();
        lrOut.miTargetScore  = lrStunt.GetTargetScore();
    }

    lrOut.miComboScore      = lrStunt.GetComboScore();
    lrOut.miComboMultiplier = lrStunt.GetComboMultiplier();
    lrOut.muCurrentStunts   = lrStunt.GetCurrentStunts();
    lrOut.muAllStunts       = lrStunt.GetAllStuntTypesForInProgressStunt();

    // Combo-warning tail (X360 inlines IsComboWarningActive / GetTimeSinceComboWarningActivated; the slice
    // exposes them as getters, so this calls them by name).
    lrOut.mbComboWarningActive    = lrStunt.IsComboWarningActive();
    lrOut.mfComboWarningTimeActive= lrStunt.GetTimeSinceComboWarningActivated();
    lrOut.mbComboInProgress       = lrStunt.IsComboInProgress();

    lrStunt.OutputStuntsToDisplay(1, lrOut.maStunts);
}

}
