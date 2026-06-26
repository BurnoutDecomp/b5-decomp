// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_UpdateA.cpp
// ============================================================================
// Out-of-line method BODIES for the ScoringSystem "UpdateA" group
// (BrnScoringSystem.h lines 402-419): the road-rules high-score lookup plus the
// part-A per-frame update pass.
//
// SHAPE = DecFIGS DWARF; BODY = X360 pseudocode (overrides DWARF on conflict).
// Members accessed BY NAME against the keystone layout -- no offset casts.
//
// ----------------------------------------------------------------------------
// STATUS: every addressed declare-only method in this range is currently BLOCKED
// by a type/method that has NO committed definition in the source tree (the body
// would not compile, and the dependency RULE forbids ad-hoc-slicing a shared
// keystone). Each blocker is recorded below; the body is omitted (left
// declare-only) so this TU lands clean and the methods can be picked up once
// their blocking keystone is reconstructed.
//
//   GetHighestLobbyRoadRuleScore  (0x8232B280)  [NOW LANDED -- see body below]
//       Reads each CarData's road-rule ChallengeHighScoreEntry table and runs a
//       best-score scan via BrnStreetData::ChallengeData::ContainsData(...) and
//       BrnStreetData::ChallengeData::CompareScores(...). Both methods now resolve
//       under cl /c: ContainsData is the inline ChallengeData predicate
//       (BrnChallengeData.h:89) and CompareScores is its declared three-way
//       comparator (BrnChallengeData.h:107, body in the BrnChallengeData.cpp TU).
//       ChallengeHighScoreEntry::GetScore (inline) and CarData::GetRoadRulesScores
//       (keystone, declare-only) complete the dependency set, so the body lands below.
//
//   UpdateNumberOfCarsInMode      (0x8231F3F0)  [NOW LANDED -- see body below]
//       Body is muCarsInCurrentMode = lpOutput->maCarsInTheRace.GetLength()
//       (with the <=8 assert). The keystone now typedefs
//       BrnGameState::ActiveRaceCarOutputInterface =
//       BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface, whose
//       public maCarsInTheRace (Array<CarsInTheRaceData,8>) is declared in
//       BrnRaceCarEntityModuleOutputInterface.h (included below). The body reaches
//       maCarsInTheRace.GetLength() BY NAME; GetLength() itself owns the -1
//       "Array used before Construct/Clear" sentinel assert (CgsArray.h:62), so the
//       body carries only the X360 "too many global race cars" bound assert.
//
//   UpdateRacePositions           (0x8232A668)  [NOW LANDED -- see body below]
//       Iterates lpActive->maCarsInTheRace, gathers distances from lpAI, queries
//       lpModeManager->GetCheckpointPosition(...) and lpActive->IsRaceCarActive(...),
//       qsorts the positioning scratch and assigns race positions. The three GameState-side
//       interface params + ModeManager RESOLVE: ActiveRaceCarOutputInterface /
//       GlobalRaceCarOutputInterface (typedefs -> RCEntity{Active,Global}RaceCarOutputInterface,
//       BrnRaceCarEntityModuleOutputInterface.h, included below), AICarOutputInterface (typedef
//       -> BrnAI::AIModuleIO::AICarOutputInterface, BrnAICarOutputInterface.h, included below) and
//       ModeManager's GetCheckpointPosition(u32) const (committed minimal slice, BrnModeManager.h:92).
//       No ModeManager method OUTSIDE the committed slice is called.
//       The two CarScoreData slots that previously blocked this body NOW HAVE named accessors in their
//       home (GameStateModuleIO::CarScoreData, BrnGameStateSharedIO.h):
//         * +0x20 mfDistanceToNextCheckpointLive (f32): the per-car LIVE distance-to-NEXT-checkpoint.
//           Get/SetDistanceToNextCheckpointLive (BrnGameStateSharedIO.h:475-476). The X360 sets it
//           from RaceCarPositioningData::mfDistanceToNextCheckpoint for a running car, and to 0.0 for
//           a finished car (asm 0x8232AC1C stfs 0x20 / 0x8232AC28 stfs f29(0.0),0x20). It is the
//           companion of mfDistanceToFinishLive(+0x18): the loop writes +0x18 and +0x20 as a pair.
//         * +0x44 mbRacePositionImproved (bool): a per-frame "race position improved this frame" flag.
//           Get/SetRacePositionImproved (BrnGameStateSharedIO.h:477-478). The loop clears it to 0 each
//           car (asm 0x8232AC34 stb 0,0x44), then sets it to 1 when the car's new race position is
//           better (numerically lower) than its recorded best AND that best was not still the ClearData
//           seed 9 (asm 0x8232AC48 stb 1,0x44 guarded by miHighestRacePosition != 9). DISTINCT from
//           mbHasFinished(+0x45). Body lands below; the complete store-for-store mapping is preserved
//           in the inline comments on each statement.
//
//   UpdateTeamStats               (0x8231F308)  [NOW LANDED -- see body below]
//       Per car, accumulates the frame delta-time into two CgsSystem::Time slots
//       inside the embedded CarScoreData (X360 +0x6C mTimeInCurrentTeam, +0x74
//       maTimeInTeam[] indexed by the CarData team field @+0x13C). The Foundation
//       phase grew CarScoreData with NAMED accessors for both slots
//       (Get/SetTimeInCurrentTeam @+0x6C, Get/SetTimeInTeam(team) @+0x74), so the
//       body now reaches them BY NAME with no blob arithmetic. Body lands below.
//
//   UpdateTakedowns               (0x8232AC88)  [NOW LANDED -- see body below]
//       Iterates lpQueue (TakedownEvent records) and tallies takedowns/marks onto
//       the aggressor/victim CarData. The queue parameter type now RESOLVES:
//       InputBuffer::TakedownEventQueue is completed in BrnScoringSystemEventQueues.h
//       (empty struct deriving CgsModule::EventQueue<TakedownEvent,8>), carrying the
//       full GetLength/GetEvent surface; TakedownEvent is committed in
//       BrnTakedownManagerTypes.h. Body lands below. FLAG: the X360 0x8232AC88 carries
//       two extra params (a3/a4) the DWARF single-arg signature drops; they gate ONLY
//       the trailing StuntModeScoringOnline::DealWithTakedown online-scoring call
//       (0x8232AE40-0x8232AE7C). That call cannot be expressed under the committed
//       single-arg keystone signature and StuntModeScoringOnline is not reachable here,
//       so the tail is omitted -- every CarScoreData tally the body DOES own
//       (takedowns/against +0x4C/+0x50, marked-man events +0x108/+0x10C, traitorous
//       +0x120/+0x124) is reconstructed store-for-store.
//
//   UpdatePaybackTakedowns        (0x82338320)  [NOW LANDED -- see body below]
//       Merges the two DirtyTrickQueue inputs into a local DirtyTrickQueue and tallies
//       payback takedowns onto the aggressor/victim CarData per the DirtyTrickEvent
//       status word. GameStateToNetworkInterface::DirtyTrickQueue now RESOLVES (empty
//       struct deriving CgsModule::EventQueue<DirtyTrickEvent,28> in
//       BrnScoringSystemEventQueues.h -- carries Construct/Append/GetEvent/GetLength);
//       DirtyTrickEvent is committed in BrnNetworkSharedIO.h. Matches the keystone TWO
//       queue-pointer signature. Body lands below.
//
//   UpdateCrashes                 (0x8231F9B8)  [NOW LANDED -- see body below]
//       Iterates lpQueue (RaceCarCrashEvent records); for each primary crash, decodes
//       the crashed car's active-race-car index from the packed crash-event volume
//       instance id and bumps that car's crash tally (CarScoreData +0x54).
//       VehicleManagerOutputInterface::RaceCarCrashEventQueue now RESOLVES (empty struct
//       deriving CgsModule::EventQueue<RaceCarCrashEvent,8> in BrnScoringSystemEventQueues.h);
//       RaceCarCrashEvent is committed in BrnVehicleEvents.h. Body lands below. FLAG: the
//       crashed-car index is a 14-bit field packed in RaceCarCrashEvent::mRaceCarVolumeInstanceID
//       (X360 (highword(muId) >> 10) & 0x3FFF); CgsSceneManager::VolumeInstanceId carries NO
//       committed index accessor, so the X360-proven decode is reproduced in-TU (see helper).
//
// (GetOnlinePlayersChallengeHighScores at header line 403 carries only a ':749'
//  DWARF line and NO X360 0x82 address comment, so it is not a target of this
//  addressed-body pass and has no dossier to reconstruct from.)
// ----------------------------------------------------------------------------

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

// ActiveRaceCarOutputInterface resolves (via the keystone typedef) to
// BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface; its public
// maCarsInTheRace (Array<CarsInTheRaceData,8>) is declared here. Needed by
// UpdateNumberOfCarsInMode to dereference the interface BY NAME.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

// ChallengeHighScoreEntry (: ChallengeData) -- the concrete per-car road-rule score record
// GetHighestLobbyRoadRuleScore stack-allocates, fills via CarData::GetRoadRulesScores, then
// probes with ChallengeData::ContainsData / GetScore / CompareScores. Pulls in CgsNetwork::PlayerName
// (GetScore's out-param) and the shared ChallengeData home transitively.
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"

// Completes BrnGameState::AICarOutputInterface (a typedef alias of
// BrnAI::AIModuleIO::AICarOutputInterface): UpdateRacePositions reads each car's distance-to-checkpoint
// via lpAI->GetAICarDistanceToCheckpoint(slot) BY NAME (the accessor owns the X360 in-range assert).
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"

// ModeManager::GetCheckpointPosition(u32) const -- UpdateRacePositions queries the checkpoint world
// position to recompute a car's distance-to-finish when the AI gave no usable distance.
#include "GameSource/GameState/ModeManager/BrnModeManager.h"

// rw::math::vpu Vector3 vocabulary (operator- / Length), found by ADL on Vector3, used by
// UpdateRacePositions to turn the car->checkpoint offset into a scalar distance.
#include "SharedClasses/Maths/BrnVectorMaths.h"

// CgsDev::PerfMonCpu::Start/StopMonitor(handle) -- UpdateRacePositions brackets its work in the
// miUpdateRacePositionsPM monitor (X360 this+0x5D44).
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

#include <stdlib.h>   // qsort (sorts the maRaceCarPositioningData[8] scratch best-first)

// NOTE on EActiveRaceCarIndex scoping (the project's unresolved dual-scope gotcha):
// the keystone ScoringSystem (BrnScoringSystem.h) is built on the GLOBAL ::EActiveRaceCarIndex
// (from BurnoutConstants.h) -- GetCarData / GetPlayerTeam / IsRaceCarActive all take that type.
// The three event-queue ELEMENT types, however, spell their index fields with the namespaced
// BrnGameState::EActiveRaceCarIndex (homed in BrnTakedownManagerTypes.h, identical values, NOT
// yet unified with the global one). Pulling BrnScoringSystemEventQueues.h in ABOVE the existing
// bodies would make BrnGameState::EActiveRaceCarIndex shadow the global one inside `namespace
// BrnGameState`, breaking the already-landed bodies (GetHighestLobbyRoadRuleScore /
// UpdateRacePositions) that rely on unqualified == global. So the event-queue header is included
// BELOW the existing namespace block, and the three new bodies live in a SECOND namespace block
// after it -- there they static_cast each event's BrnGameState::EActiveRaceCarIndex to the global
// ::EActiveRaceCarIndex the keystone surface expects. Existing bodies stay textually untouched.

namespace BrnGameState
{
    namespace
    {
        // "No valid distance yet" sentinel (X360 flt_82CDB7D0). RaceCarPositioningData::Construct() /
        // ClearData seed the scratch distances with it; UpdateRacePositions seeds the same value here and
        // tests equality against it (AI-distance-missing / already-set guard). The magnitude is recovered
        // from the PS3 DecFIGS ScoringSystem::ClearData (0x1E364C, same source): flt_82CDB7D0 == FLT_MAX
        // (3.4028235e38). The X360 0x8232A4A8 stores the same flt_82CDB7D0 along this path. The sibling
        // BrnScoringSystem_Lookup.cpp uses the identical literal so the seed/equality test is consistent.
        const f32 KF_INVALID_RACE_DISTANCE = 3.4028235e38f;   // flt_82CDB7D0 == FLT_MAX (PS3 0x1E364C)

        // qsort(3) comparator trampoline. The X360 build passes the address of
        // ScoringSystem::CompareRaceCarDistances straight to qsort -- the function provably never
        // touches `this` (see BrnScoringSystem_UpdateB.cpp), so it is a C comparator in all but its
        // DWARF declaration. A non-static member-function pointer cannot be converted to a plain
        // function pointer in standard C++ (MSVC C2440), so this file-local free function forwards to
        // it. `this` is never read inside the comparator, so the null instance is never dereferenced;
        // this preserves the single source of the ordering logic (no duplicated comparison) while
        // matching the binary's free-function call. (Keystone surface untouched -- the member stays a
        // member; this trampoline lives only in this TU.)
        int CompareRaceCarDistancesTrampoline(const void* lpA, const void* lpB)
        {
            ScoringSystem* lpNullThis = NULL;
            return (lpNullThis->*&ScoringSystem::CompareRaceCarDistances)(lpA, lpB);
        }
    }

    // ------------------------------------------------------------------------
    // ScoringSystem::UpdateTeamStats  (X360 0x8231F308)
    // ------------------------------------------------------------------------
    // Per-frame team-time accumulator. For every active-race-car slot that owns a
    // CarData record, fold the frame delta-time into that car's per-team time
    // tallies inside the embedded CarScoreData:
    //   * maTimeInTeam[team] += lDeltaTime   (X360 +0x74, team = CarData @+0x13C)
    //   * mTimeInCurrentTeam += lDeltaTime   (X360 +0x6C)
    //
    // X360 detail (asm at 0x8231F308): the frame delta arrives as the f32 argument
    // (passed in the FPR, reused for both Time(f32) constructions). muCarsInCurrentMode
    // is asserted <= KI_MAX_ACTIVE_RACE_CARS (== E_ACTIVE_RACE_CAR_INDEX_COUNT, 8)
    // before the walk. The walk runs GetCarData(slot) for slots 0..7 (sub_8231DCD0 ==
    // the const EActiveRaceCarIndex GetCarData); NULL slots are skipped. For each live
    // car the team-indexed slot (+0x74 + (team<<3)) is bumped first, then the
    // current-team slot (+0x6C) -- both by the SAME delta. The team index is the raw
    // CarData team field (GetTeam()); reached here BY NAME through the committed
    // CarScoreData accessors (Get/SetTimeInTeam, Get/SetTimeInCurrentTeam). The
    // optimizer's per-iteration "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" re-assert
    // is the EActiveRaceCarIndex post-increment bounds artifact; the clean
    // `< E_ACTIVE_RACE_CAR_INDEX_COUNT` loop subsumes it (matches the sibling TUs).
    void ScoringSystem::UpdateTeamStats(f32 lfDeltaTime)
    {
        CGS_ASSERT(GetNumberOfActiveCars() <= static_cast<u32>(E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "muCarsInCurrentMode <= uint32_t(BrnWorld::KI_MAX_ACTIVE_RACE_CARS)");

        const CgsSystem::Time lDeltaTime(lfDeltaTime);

        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            CarData* lpCarData = GetCarData(static_cast<EActiveRaceCarIndex>(liSlot));
            if (lpCarData != NULL)
            {
                GameStateModuleIO::CarScoreData* lpScoreData = lpCarData->GetScoreData();
                const u32 luTeam = static_cast<u32>(lpCarData->GetTeam());

                // +0x74: per-team accumulated time, keyed by the car's team field.
                CgsSystem::Time lTeamTime = lpScoreData->GetTimeInTeam(luTeam);
                lTeamTime += lDeltaTime;
                lpScoreData->SetTimeInTeam(luTeam, lTeamTime);

                // +0x6C: time in the car's current team.
                CgsSystem::Time lCurrentTeamTime = lpScoreData->GetTimeInCurrentTeam();
                lCurrentTeamTime += lDeltaTime;
                lpScoreData->SetTimeInCurrentTeam(lCurrentTeamTime);
            }
        }
    }

    // ------------------------------------------------------------------------
    // ScoringSystem::UpdateNumberOfCarsInMode  (X360 0x8231F3F0)
    // ------------------------------------------------------------------------
    // Snapshot the live race-car count from the active output interface into this
    // ScoringSystem's muCarsInCurrentMode, then assert it fits the active-race-car
    // slot budget.
    //
    // X360 detail (asm at 0x8231F3F0): reads *(a2 + 512) -- the count field of
    // lpOutput->maCarsInTheRace -- through Array<>::GetLength (which fires the
    // "Array used before Construct/Clear was called" assert on the -1/KI_UNCONSTRUCTED
    // sentinel; reproduced here by simply calling GetLength()). It stores that count
    // into v3[5050] (== muCarsInCurrentMode) UNCONDITIONALLY, then -- if the stored
    // count exceeds 8 -- fires "Too many global race cars think they are in the current
    // mode" (BrnScoringSystem.cpp:601). The bound 8 == E_ACTIVE_RACE_CAR_INDEX_COUNT
    // (BurnoutConstants.h). The store-before-assert order is preserved verbatim.
    void ScoringSystem::UpdateNumberOfCarsInMode(const ActiveRaceCarOutputInterface* lpOutput)
    {
        muCarsInCurrentMode = lpOutput->maCarsInTheRace.GetLength();

        CGS_ASSERT(muCarsInCurrentMode <= static_cast<u32>(E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "Too many global race cars think they are in the current mode");
    }

    // ------------------------------------------------------------------------
    // ScoringSystem::GetHighestLobbyRoadRuleScore  (X360 0x8232B280)
    // ------------------------------------------------------------------------
    // Scan every in-scoring-system car's road-rule high-score table for the single best
    // recorded score for one challenge (lChallenge) under one score type (leScoreType),
    // returning that best score (*lpiScore) and the race-car index that owns it
    // (*lpeActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_INVALID if no car holds a score).
    //
    // X360 detail (asm at 0x8232B280):
    //   * Three leading debug guards: lpiScore != NULL (line 1431), lpeActiveRaceCarIndex
    //     != NULL (1432), and lChallenge in [0, KI_MAX_CHALLENGES) (1433). The challenge
    //     bound BrnGameState::KI_MAX_CHALLENGES (== 64) is homed in the not-yet-reconstructed
    //     BrnGameStateStreetManager.h; rather than fork that constant's home, the bound is
    //     written numerically with the X360 assert string preserved verbatim.
    //   * Best-score seed depends on the score-type's ranking direction: for the CRASH-style
    //     type (leScoreType != 0) higher is better, so the running best starts at 0; for the
    //     TIME-style type (leScoreType == 0) lower is better, so it starts at INT_MAX. This
    //     mirrors `v10 = a3 ? 0 : 0x7FFFFFFF`.
    //   * The walk steps the private maCarData[] record array directly (X360 v13 = this+5137
    //     dwords, stride 86 dwords == sizeof(CarData)), reading each record's meRaceCarIndex.
    //     A record is live when that index is neither E_ACTIVE_RACE_CAR_INDEX_COUNT (8, the
    //     empty-slot sentinel) nor E_ACTIVE_RACE_CAR_INDEX_INVALID (-1). For each live car the
    //     body fills a ChallengeHighScoreEntry from CarData::GetRoadRulesScores, and -- only if
    //     that entry actually carries this score type (ContainsData) -- reads the entry's score
    //     and three-way-compares it against the running best (CompareScores < 0 == the entry is
    //     the better score), updating the best score + owning car index on a win.
    //   * The optimizer's per-iteration `leEnumIndex <= E_PLAYER_SCORING_INDEX_COUNT` re-assert
    //     is the post-increment bounds artifact of the EPlayerScoringIndex counter; the clean
    //     `< E_ACTIVE_RACE_CAR_INDEX_COUNT` loop subsumes it (same pattern as the sibling TUs).
    void ScoringSystem::GetHighestLobbyRoadRuleScore(BrnNetwork::Road::ChallengeIndex lChallenge,
                                                     BrnStreetData::ScoreType leScoreType,
                                                     s32* lpiScore,
                                                     EActiveRaceCarIndex* lpeRaceCarIndex)
    {
        CGS_ASSERT(lpiScore != NULL, "lpiScore");
        CGS_ASSERT(lpeRaceCarIndex != NULL, "lpeActiveRaceCarIndex");
        // BrnGameState::KI_MAX_CHALLENGES == 64 (BrnGameStateStreetManager.h, not yet homed).
        CGS_ASSERT(lChallenge < 64 && lChallenge >= 0,
                   "liIndex < BrnGameState::KI_MAX_CHALLENGES && liIndex >= 0");

        // Best-score seed: higher-is-better types start at 0, lower-is-better (TIME) at INT_MAX.
        s32 liBestScore = (leScoreType != BrnStreetData::E_SCORE_TYPE_TIME) ? 0 : 0x7FFFFFFF;
        EActiveRaceCarIndex leBestRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;

        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            const EActiveRaceCarIndex leRaceCarIndex = maCarData[liSlot].GetActiveRaceCarIndex();
            if (leRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT &&
                leRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                BrnStreetData::ChallengeHighScoreEntry lEntry;
                maCarData[liSlot].GetRoadRulesScores(lChallenge, &lEntry);

                if (lEntry.ContainsData(leScoreType))
                {
                    s32 liCandidateScore = 0;
                    CgsNetwork::PlayerName lPlayerName;
                    lEntry.GetScore(leScoreType, &liCandidateScore, &lPlayerName);

                    // CompareScores(type, score0, score1) < 0 => score0 (the candidate) is better.
                    if (lEntry.CompareScores(leScoreType, liCandidateScore, liBestScore) < 0)
                    {
                        leBestRaceCarIndex = leRaceCarIndex;
                        liBestScore = liCandidateScore;
                    }
                }
            }
        }

        *lpiScore = liBestScore;
        *lpeRaceCarIndex = leBestRaceCarIndex;
    }

    // ------------------------------------------------------------------------
    // ScoringSystem::UpdateRacePositions  (X360 0x8232A668)
    // ------------------------------------------------------------------------
    // Per-frame race-ordering pass: build a distance-to-finish scratch row for every car in the
    // race, qsort it (CompareRaceCarDistances), then walk the sorted scratch handing out 1-based
    // race positions, tracking the new leader / last-place car, and folding each car's live
    // distances + best-position-so-far back onto its CarScoreData.
    //
    // Reconstructed store-for-store against the 0x8232A668 ASM, members named against the keystone
    // layout. The two CarScoreData slots that previously blocked this body now have named accessors:
    //   +0x20 mfDistanceToNextCheckpointLive (Get/SetDistanceToNextCheckpointLive)
    //   +0x44 mbRacePositionImproved        (Get/SetRacePositionImproved)
    //
    // KF_INVALID_RACE_DISTANCE == flt_82CDB7D0 == FLT_MAX (3.4028235e38, recovered from the PS3
    // DecFIGS ScoringSystem::ClearData 0x1E364C), the "no valid distance yet" sentinel that
    // RaceCarPositioningData::Construct() / ClearData seed the scratch distances with. The sibling
    // reset BrnScoringSystem_Lookup.cpp uses the identical literal so the equality test against the
    // seed is consistent across the two TUs.
    // The 1.35f checkpoint-distance fudge factor is flt_82020F7C (X360-proven).
    void ScoringSystem::UpdateRacePositions(const ActiveRaceCarOutputInterface* lpActive,
                                            const GlobalRaceCarOutputInterface* lpGlobal,
                                            const AICarOutputInterface* lpAI,
                                            ModeManager* lpModeManager)
    {
        (void)lpGlobal;  // X360 stashes a5 (lpGlobal) but never reads it in this pass.

        CgsDev::PerfMonCpu::StartMonitor(miUpdateRacePositionsPM);   // X360 this+0x5D44

        // --- PASS 1: build maRaceCarPositioningData[] from the live race list ---
        // The loop bound is the active interface's live-race-car count (X360 *(a2+512), via
        // Array<>::GetLength which owns the "Array used before Construct/Clear" sentinel assert).
        for (u32 liSlot = 0; liSlot < lpActive->maCarsInTheRace.GetLength(); ++liSlot)
        {
            const EActiveRaceCarIndex leIdx = lpActive->maCarsInTheRace[liSlot].meActiveRaceCarIndex; // +0x30

            RaceCarPositioningData& lPos = maRaceCarPositioningData[liSlot];   // this+0x59C0, stride 24
            lPos.mfDistanceToNextCheckpoint = KF_INVALID_RACE_DISTANCE;        // +0x04 (flt_82CDB7D0 seed)
            lPos.miCurrentCheckpoint        = -1;                              // +0x0C
            lPos.mfDistanceToFinish         = KF_INVALID_RACE_DISTANCE;        // +0x08 (seed)
            lPos.meActiveRaceCarIndex       = E_ACTIVE_RACE_CAR_INDEX_INVALID; // +0x00
            lPos.miFinishPosition           = 8;                              // +0x10 (KI_MAX_ACTIVE_RACE_CARS)
            lPos.mbDisconnected             = false;                          // +0x14

            CarData* lpCarData = GetCarData(leIdx);                           // sub_8231DCD0
            if (lpCarData == NULL)
            {
                // No record for this slot: leave the row marked unset (X360 *v28 = -1) and move on.
                lPos.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;  // +0x00
                continue;
            }

            // X360 reads *(4*(slot+1283)+a18): the AI distance-to-checkpoint indexed by the SLOT (not
            // the race-car index). GetAICarDistanceToCheckpoint owns the X360 in-range bound assert
            // (liAICarIndex >= 0 && < KI_MAX_OUT_OF_RANGE_RACE_CARS).
            const f32 lfAIDist = lpAI->GetAICarDistanceToCheckpoint(static_cast<s32>(liSlot));
            const GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();
            // mafCheckpointDistancesToFinish indexed by the car's current checkpoint (X360 *(4*(cp+5928)+a1)).
            const f32 lfCheckpointBaseline =
                mafCheckpointDistancesToFinish[lpCarData->GetCurrentCheckPoint()];

            lPos.meActiveRaceCarIndex       = leIdx;                          // +0x00
            lPos.mfDistanceToNextCheckpoint = lfAIDist;                       // +0x04
            lPos.miCurrentCheckpoint        = lpCarData->GetCurrentCheckPoint(); // +0x0C (CarData+0x154)
            lPos.miFinishPosition           = lpScore->GetFinishPosition();   // +0x10 (CarScoreData+0x14)
            lPos.mbDisconnected             = lpScore->GetDisconnected();     // +0x14 (CarScoreData+0x69)

            if (lfAIDist == KF_INVALID_RACE_DISTANCE)   // AI gave no usable distance
            {
                // X360: if mfDistanceToFinish was already set (!= sentinel) the recompute is skipped
                // and the +0x08 store is bypassed (goto LABEL_16). The seed above leaves it == sentinel,
                // so on the normal path this recomputes; the guard is preserved for fidelity.
                if (lPos.mfDistanceToFinish == KF_INVALID_RACE_DISTANCE)
                {
                    f32 lfGuessDistToCheckpoint = 0.0f;
                    if (lpActive->IsRaceCarActive(leIdx))
                    {
                        const Vector3 lv3CheckpointPos =
                            lpModeManager->GetCheckpointPosition(static_cast<u32>(lPos.miCurrentCheckpoint));
                        const Vector3 lv3CarPos =
                            lpActive->GetRaceCarState(leIdx)->mTransform.wAxis; // RaceCarState+0x1F0+0x30
                        lfGuessDistToCheckpoint = Length(lv3CheckpointPos - lv3CarPos) * 1.35f; // flt_82020F7C
                    }
                    lPos.mfDistanceToFinish = lfGuessDistToCheckpoint + lfCheckpointBaseline; // +0x08
                }
            }
            else
            {
                lPos.mfDistanceToFinish = lfCheckpointBaseline + lfAIDist;    // +0x08
            }

            // X360 :689 / :690 -- both distances must be non-negative (the asserts format the value
            // into "Distance: " via the message buffer; reproduced as a value-bearing CGS_ASSERT).
            CGS_ASSERT(lPos.mfDistanceToNextCheckpoint >= 0.0f,
                       "RwMathFPU::IsValid( lfDistanceToNextCheckpoint )");
            CGS_ASSERT(lPos.mfDistanceToFinish >= 0.0f,
                       "RwMathFPU::IsValid( lfCheckpointDistanceToFinish )");
        }

        CGS_ASSERT(lpActive->maCarsInTheRace.GetLength() == muCarsInCurrentMode,
                   "lpActiveRaceCarInterface->maCarsInTheRace.GetLength() == muCarsInCurrentMode"); // :693

        // --- SORT: best (closest-to-finish) car first ---
        // X360 qsort(&maRaceCarPositioningData, 8, sizeof(RaceCarPositioningData),
        //            &ScoringSystem::CompareRaceCarDistances). Routed through the file-local trampoline
        // (the member comparator never reads `this`) -- see CompareRaceCarDistancesTrampoline above.
        qsort(maRaceCarPositioningData, 8, sizeof(RaceCarPositioningData),
              CompareRaceCarDistancesTrampoline);

        // --- PASS 2: assign 1-based positions over the sorted scratch ---
        mbNewLeader    = false;                       // X360 stb 0,0x4EF8
        mbNewLastPlace = false;                       // X360 stb 0,0x4EF9
        muActualNumberOfCarsInCurrentMode = 0;        // X360 stw 0,0x4EEC
        s32 liPosition = 0;
        for (u32 liSlot = 0; liSlot < muCarsInCurrentMode; ++liSlot)
        {
            const EActiveRaceCarIndex leIdx = maRaceCarPositioningData[liSlot].meActiveRaceCarIndex;
            if (leIdx == E_ACTIVE_RACE_CAR_INDEX_INVALID)
                continue;

            CarData* lpCarData = GetCarData(leIdx);
            if (lpCarData == NULL)
                continue;

            // Skip cars already eliminated (X360 EActiveRaceCarIndex_7_::FindFirstInstanceOf != -1).
            if (maEliminatedActiveRaceCarIndexs.FindFirstInstanceOf(leIdx) != -1)
                continue;

            ++liPosition;   // 1-based race position
            CGS_ASSERT(leIdx >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= 0");           // :732
            CGS_ASSERT(leIdx <  E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");                // :733

            GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();

            // Newly first: this car is at the front of the sorted scratch but did not previously hold
            // race position 1. (X360 reads the OLD +0x08 value here, BEFORE the writeback below.)
            if (lpScore->GetRacePosition() != 1 && liSlot == 0)
            {
                meLeadRaceCarIndex = leIdx;            // X360 stw 0x4EF0
                mbNewLeader        = true;            // X360 stb 1,0x4EF8
            }
            // Newly last: this car is at the back of the field (slot == count-1) but did not previously
            // hold the last position.
            if (lpScore->GetRacePosition() != static_cast<s32>(muCarsInCurrentMode) &&
                muCarsInCurrentMode - 1 == liSlot)
            {
                meLastRaceCarIndex = leIdx;           // X360 stw 0x4EF4
                mbNewLastPlace     = true;            // X360 stb 1,0x4EF9
            }

            if (lpScore->GetHasFinished())            // CarScoreData+0x45
            {
                // Finished cars freeze their live distances at 0 and keep their already-assigned
                // race position (the X360 does NOT write +0x08 in this branch).
                lpScore->SetDistanceToFinishLive(0.0f);            // +0x18
                lpScore->SetDistanceToNextCheckpointLive(0.0f);    // +0x20
            }
            else
            {
                lpScore->SetRacePosition(liPosition);              // +0x08
                lpScore->SetDistanceToFinishLive(
                    maRaceCarPositioningData[liSlot].mfDistanceToFinish);          // +0x18
                lpScore->SetDistanceToNextCheckpointLive(
                    maRaceCarPositioningData[liSlot].mfDistanceToNextCheckpoint);  // +0x20
            }

            const s32 liNewPos  = lpScore->GetRacePosition();      // +0x08
            const s32 liBestPos = lpScore->GetHighestRacePosition(); // +0x0C
            lpScore->SetRacePositionImproved(false);               // +0x44 (cleared each car)
            if (liNewPos < liBestPos)                              // improved (lower is better)
            {
                // Mark "improved this frame" unless the best was still the ClearData seed 9.
                if (liBestPos != 9)
                    lpScore->SetRacePositionImproved(true);        // +0x44
                lpScore->SetHighestRacePosition(liNewPos);         // +0x0C
            }
        }

        muActualNumberOfCarsInCurrentMode = static_cast<u32>(liPosition);   // this+0x4EEC
        CgsDev::PerfMonCpu::StopMonitor(miUpdateRacePositionsPM);
    }
}

// ============================================================================
// Event-queue update group (UpdateTakedowns / UpdateCrashes / UpdatePaybackTakedowns).
// ============================================================================
// Pulled in BELOW the first BrnGameState block on purpose: this header transitively defines
// BrnGameState::EActiveRaceCarIndex (BrnTakedownManagerTypes.h) which would otherwise shadow
// the GLOBAL ::EActiveRaceCarIndex the keystone surface (and the bodies above) rely on -- see
// the scoping note at the top of this file. It completes the three event-queue parameter types
// (empty structs deriving CgsModule::EventQueue<T,N>; full GetLength/GetEvent/Construct/Append
// surface) and the three committed element-type homes:
//   TakedownEvent      (BrnTakedownManagerTypes.h)
//   RaceCarCrashEvent  (BrnVehicleEvents.h)
//   DirtyTrickEvent    (BrnNetworkSharedIO.h)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h"

namespace BrnGameState
{
    namespace
    {
        // Decode the crashed car's active-race-car index out of a packed
        // CgsSceneManager::VolumeInstanceId. X360 UpdateCrashes (0x8231F9B8) reads the 64-bit
        // muId, takes its high 32-bit word and extracts a 14-bit field at bit 10 (asm:
        // ld 0(event) / srdi 32 / extrwi r4, hi, 14, 8 == (hi >> 10) & 0x3FFF). VolumeInstanceId
        // is a minimal-recon u64 with NO committed index accessor and the dependency RULE forbids
        // growing that home from this TU, so the X360-proven bit math lives here. FLAG: when
        // VolumeInstanceId grows a named race-car-index/instance accessor, route this through it.
        // Returns the GLOBAL ::EActiveRaceCarIndex (the type GetCarData expects).
        ::EActiveRaceCarIndex DecodeCrashedRaceCarIndex(const CgsSceneManager::VolumeInstanceId& lVolumeId)
        {
            const u32 luHighWord = static_cast<u32>(lVolumeId.muId >> 32);
            return static_cast< ::EActiveRaceCarIndex>((luHighWord >> 10) & 0x3FFF);
        }
    }

    // ------------------------------------------------------------------------
    // ScoringSystem::UpdateTakedowns  (X360 0x8232AC88)
    // ------------------------------------------------------------------------
    // Walk the per-frame takedown event queue and credit each event to the relevant
    // cars' CarScoreData records:
    //   * aggressor.miTakedowns        (+0x4C)  += 1   (every event)
    //   * victim.miTakedownsAgainst    (+0x50)  += 1   (every event)
    //   * if the event is a marked-man takedown (TakedownEvent::mbMarkedManTakeDown):
    //       aggressor.miMarkedManTakedownEventsFor     (+0x108) += 1
    //       victim.miMarkedManTakedownEventsAgainst    (+0x10C) += 1
    //   * if both cars are on the same (non-NONE) team -- a "traitorous" takedown:
    //       aggressor.miTraitorousTakedownsFor         (+0x120) += 1
    //       aggressor.miTraitorousTakedownsAgainst     (+0x124) += 1
    //
    // X360 detail (asm at 0x8232AC88): asserts lpTakedownQueue != NULL (cpp:987), loops
    // i in [0, GetLength()) (*(queue+8)). Each iteration copies the 40-byte TakedownEvent
    // out of GetEvent(i) (the 5-qword `do` loop), reads its first two words as the
    // aggressor / victim race-car indices, and bounds-asserts each in [0,8) (cpp:997/1001).
    // GetCarData(idx) (sub_8231DCD0) maps an index to its CarData* (NULL on no match); when
    // BOTH cars resolve it bumps +0x4C(aggressor)/+0x50(victim), then -- gated on
    // mbMarkedManTakeDown (event +0x24) -- +0x108(aggressor)/+0x10C(victim), then -- gated
    // on GetPlayerTeam(aggressor) != NONE && GetPlayerTeam(aggressor) == GetPlayerTeam(victim)
    // -- +0x120/+0x124 (both on the aggressor). FLAG: the trailing
    // StuntModeScoringOnline::DealWithTakedown call (0x8232AE40-0x8232AE7C) is gated on the
    // two extra X360 params (a3/a4) the committed single-arg DWARF signature drops and reaches
    // a scorer not declared on the keystone; it is omitted (see file header).
    //
    // The event's index fields are BrnGameState::EActiveRaceCarIndex; GetCarData / GetPlayerTeam
    // take the GLOBAL ::EActiveRaceCarIndex (identical values, not-yet-unified) -- so each is
    // static_cast across the dual scope at the call boundary.
    void ScoringSystem::UpdateTakedowns(const InputBuffer::TakedownEventQueue* lpQueue)
    {
        CGS_ASSERT(lpQueue != NULL, "lpTakedownQueue != NULL");

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const TakedownEvent& lEvent = lpQueue->GetEvent(liEvent);

            const s32 liAggressor = static_cast<s32>(lEvent.meAggressorIndex);
            const s32 liVictim    = static_cast<s32>(lEvent.meVictimIndex);
            const ::EActiveRaceCarIndex leAggressor = static_cast< ::EActiveRaceCarIndex>(liAggressor);
            const ::EActiveRaceCarIndex leVictim    = static_cast< ::EActiveRaceCarIndex>(liVictim);

            // X360 bounds asserts (cpp:997/1001); compared on the raw value to dodge the
            // dual-scope EActiveRaceCarIndex enumerator ambiguity (0 == E_ACTIVE_RACE_CAR_INDEX_0,
            // 8 == E_ACTIVE_RACE_CAR_INDEX_COUNT).
            CGS_ASSERT((liAggressor >= 0) && (liAggressor < 8),
                       "(leAggressorRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leAggressorRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");
            CGS_ASSERT((liVictim >= 0) && (liVictim < 8),
                       "(leVictimRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leVictimRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

            CarData* lpAggressorCarData = GetCarData(leAggressor);
            CarData* lpVictimCarData    = GetCarData(leVictim);
            if (lpAggressorCarData != NULL && lpVictimCarData != NULL)
            {
                GameStateModuleIO::CarScoreData* lpAggressorScore = lpAggressorCarData->GetScoreData();
                GameStateModuleIO::CarScoreData* lpVictimScore    = lpVictimCarData->GetScoreData();

                // +0x4C aggressor takedowns / +0x50 victim takedowns-against.
                lpAggressorScore->SetTakedowns(lpAggressorScore->GetTakedowns() + 1);
                lpVictimScore->SetTakedownsAgainst(lpVictimScore->GetTakedownsAgainst() + 1);

                // Marked-man takedown event: +0x108 aggressor / +0x10C victim.
                if (lEvent.mbMarkedManTakeDown)
                {
                    lpAggressorScore->SetMarkedManTakedownEventsFor(
                        lpAggressorScore->GetMarkedManTakedownEventsFor() + 1);
                    lpVictimScore->SetMarkedManTakedownEventsAgainst(
                        lpVictimScore->GetMarkedManTakedownEventsAgainst() + 1);
                }

                // Traitorous takedown: aggressor on a real team AND same team as the victim.
                const GameStateModuleIO::EPlayerTeam leAggressorTeam = GetPlayerTeam(leAggressor);
                if (leAggressorTeam != GameStateModuleIO::E_PLAYER_TEAM_NONE &&
                    leAggressorTeam == GetPlayerTeam(leVictim))
                {
                    // X360 bumps BOTH traitorous slots on the aggressor record (+0x120/+0x124).
                    lpAggressorScore->SetTraitorousTakedownsFor(
                        lpAggressorScore->GetTraitorousTakedownsFor() + 1);
                    lpAggressorScore->SetTraitorousTakedownsAgainst(
                        lpAggressorScore->GetTraitorousTakedownsAgainst() + 1);
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // ScoringSystem::UpdateCrashes  (X360 0x8231F9B8)
    // ------------------------------------------------------------------------
    // Walk the per-frame race-car-crash event queue; for every PRIMARY crash, decode the
    // crashed car's active-race-car index and bump that car's crash tally (+0x54).
    //
    // X360 detail (asm at 0x8231F9B8): asserts lpRaceCarCrashQueue != NULL (cpp:1107),
    // loops i in [0, GetLength()). GetEvent(i) yields a const RaceCarCrashEvent* asserted
    // non-NULL (cpp:1117). Only events with mbIsPrimaryCrash set (event +0x38, lbz 0x38)
    // are counted: the crashed car's index is decoded from mRaceCarVolumeInstanceID (the
    // (highword >> 10) & 0x3FFF bitfield, see DecodeCrashedRaceCarIndex), GetCarData maps
    // it to a CarData* (skip on NULL), and that car's +0x54 slot is incremented
    // (lwz/stw 0x54). NOTE: +0x54 is the committed miMarkedManTakedownsFor field; the file
    // header records the standing FLAG that UpdateCrashes uses this slot as the per-car
    // crash tally (the "marked-man" label on +0x54 predates this work and may be a misnomer);
    // left UNCHANGED -- the byte offset is X360-proven and the home is grow-only.
    void ScoringSystem::UpdateCrashes(const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpQueue)
    {
        CGS_ASSERT(lpQueue != NULL, "lpRaceCarCrashQueue != NULL");

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const BrnPhysics::Vehicle::RaceCarCrashEvent* lpCrashEvent = &lpQueue->GetEvent(liEvent);
            // X360 asserts the per-event pointer non-NULL (cpp:1117). GetEvent returns a
            // reference into the inline buffer, so the address is non-NULL by construction;
            // the assert is preserved verbatim as a tripwire.
            CGS_ASSERT(lpCrashEvent != NULL, "lpCrashEvent != NULL");

            if (lpCrashEvent->mbIsPrimaryCrash)   // event +0x38
            {
                const ::EActiveRaceCarIndex leCrashedIndex =
                    DecodeCrashedRaceCarIndex(lpCrashEvent->mRaceCarVolumeInstanceID);

                CarData* lpCarData = GetCarData(leCrashedIndex);
                if (lpCarData != NULL)
                {
                    GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();
                    // +0x54 crash tally (committed slot miMarkedManTakedownsFor -- see FLAG above).
                    lpScore->SetMarkedManTakedownsFor(lpScore->GetMarkedManTakedownsFor() + 1);
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // ScoringSystem::UpdatePaybackTakedowns  (X360 0x82338320)
    // ------------------------------------------------------------------------
    // Merge the two per-frame dirty-trick (payback) event queues into one local queue, then
    // tally each event onto the aggressor/victim CarScoreData by the event's status word
    // (DirtyTrickEvent::meDirtyTrickStatus, the 4th event word):
    //   status == 4 (succeeded):
    //       aggressor.miTakedowns                (+0x4C)  += 1
    //       victim.miTakedownsAgainst            (+0x50)  += 1
    //       aggressor.miPaybackTakedownsType4For (+0x118) += 1
    //       victim.miPaybackTakedownsType4Against(+0x11C) += 1
    //   status == 2 (used):
    //       aggressor.miPaybackTakedownsType2For (+0x110) += 1
    //       victim.miPaybackTakedownsType2Against(+0x114) += 1
    //
    // X360 detail (asm at 0x82338320): DirtyTrickQueue::Construct(local) then Append(local, A)
    // and Append(local, B) merge both inputs (the local's length field is also explicitly
    // zeroed before the appends). Loop i in [0, local.GetLength()): GetEvent(i) supplies the
    // aggressor index (word0), victim index (word1) and status (word3). For status 4 it maps
    // both indices via GetCarData (sub_8231DCD0), asserts each non-NULL ("lpAggressorCarData"
    // cpp:1072 / "lpVictimCarData" cpp:1073), and bumps +0x4C/+0x50/+0x118/+0x11C; for status 2
    // it does the same map + asserts (cpp:1087/1088) and bumps +0x110/+0x114. Matches the
    // keystone TWO queue-pointer signature. (Event index fields are BrnGameState::EActiveRaceCarIndex;
    // static_cast to the GLOBAL ::EActiveRaceCarIndex GetCarData expects -- see scoping note.)
    void ScoringSystem::UpdatePaybackTakedowns(const GameStateToNetworkInterface::DirtyTrickQueue* lpQueueA,
                                               const GameStateToNetworkInterface::DirtyTrickQueue* lpQueueB)
    {
        GameStateToNetworkInterface::DirtyTrickQueue lPaybackEventQueue;
        lPaybackEventQueue.Construct();
        lPaybackEventQueue.Append(*lpQueueA);
        lPaybackEventQueue.Append(*lpQueueB);

        for (s32 liEvent = 0; liEvent < lPaybackEventQueue.GetLength(); ++liEvent)
        {
            const BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent& lEvent =
                lPaybackEventQueue.GetEvent(liEvent);

            const ::EActiveRaceCarIndex leAggressor =
                static_cast< ::EActiveRaceCarIndex>(static_cast<s32>(lEvent.meAggressorActiveRaceCarIndex)); // word0
            const ::EActiveRaceCarIndex leVictim =
                static_cast< ::EActiveRaceCarIndex>(static_cast<s32>(lEvent.meVictimActiveRaceCarIndex));    // word1
            const s32 liStatus = static_cast<s32>(lEvent.meDirtyTrickStatus);                                // word3

            if (liStatus == 4)
            {
                CarData* lpAggressorCarData = GetCarData(leAggressor);
                CarData* lpVictimCarData    = GetCarData(leVictim);
                CGS_ASSERT(lpAggressorCarData != NULL, "lpAggressorCarData");
                CGS_ASSERT(lpVictimCarData    != NULL, "lpVictimCarData");

                GameStateModuleIO::CarScoreData* lpAggressorScore = lpAggressorCarData->GetScoreData();
                GameStateModuleIO::CarScoreData* lpVictimScore    = lpVictimCarData->GetScoreData();

                lpAggressorScore->SetTakedowns(lpAggressorScore->GetTakedowns() + 1);                  // +0x4C
                lpVictimScore->SetTakedownsAgainst(lpVictimScore->GetTakedownsAgainst() + 1);          // +0x50
                lpAggressorScore->SetPaybackTakedownsType4For(
                    lpAggressorScore->GetPaybackTakedownsType4For() + 1);                              // +0x118
                lpVictimScore->SetPaybackTakedownsType4Against(
                    lpVictimScore->GetPaybackTakedownsType4Against() + 1);                             // +0x11C
            }
            else if (liStatus == 2)
            {
                CarData* lpAggressorCarData = GetCarData(leAggressor);
                CarData* lpVictimCarData    = GetCarData(leVictim);
                CGS_ASSERT(lpAggressorCarData != NULL, "lpAggressorCarData");
                CGS_ASSERT(lpVictimCarData    != NULL, "lpVictimCarData");

                GameStateModuleIO::CarScoreData* lpAggressorScore = lpAggressorCarData->GetScoreData();
                GameStateModuleIO::CarScoreData* lpVictimScore    = lpVictimCarData->GetScoreData();

                lpAggressorScore->SetPaybackTakedownsType2For(
                    lpAggressorScore->GetPaybackTakedownsType2For() + 1);                              // +0x110
                lpVictimScore->SetPaybackTakedownsType2Against(
                    lpVictimScore->GetPaybackTakedownsType2Against() + 1);                             // +0x114
            }
        }
    }
}
