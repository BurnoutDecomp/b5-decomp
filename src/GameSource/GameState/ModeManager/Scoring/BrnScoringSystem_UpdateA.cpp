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
//   UpdateRacePositions           (0x8232A668)
//       Iterates lpActive->maCarsInTheRace, gathers distances from lpAI, queries
//       lpModeManager->GetCheckpointPosition(...) and
//       lpActive->IsRaceCarActive(...), qsorts the positioning scratch and assigns
//       race positions. Dereferences three forward-declared GameState-side
//       interfaces plus ModeManager.
//       MISSING: BrnGameState::ActiveRaceCarOutputInterface / AICarOutputInterface
//                / ModeManager (definitions).
//
//   UpdateTeamStats               (0x8231F308)  [NOW LANDED -- see body below]
//       Per car, accumulates the frame delta-time into two CgsSystem::Time slots
//       inside the embedded CarScoreData (X360 +0x6C mTimeInCurrentTeam, +0x74
//       maTimeInTeam[] indexed by the CarData team field @+0x13C). The Foundation
//       phase grew CarScoreData with NAMED accessors for both slots
//       (Get/SetTimeInCurrentTeam @+0x6C, Get/SetTimeInTeam(team) @+0x74), so the
//       body now reaches them BY NAME with no blob arithmetic. Body lands below.
//
//   UpdateTakedowns               (0x8232AC88)
//       Iterates lpQueue (TakedownEvent records) and tallies takedowns/marks onto
//       the aggressor/victim CarData. lpQueue is InputBuffer::TakedownEventQueue*,
//       forward-declared only; the per-event accessor BrnGameState::TakedownEvent
//       and StuntModeScoringOnline::DealWithTakedown are likewise undefined here.
//       MISSING: BrnGameState::InputBuffer::TakedownEventQueue (+ TakedownEvent
//                accessor) (definitions).
//
//   UpdatePaybackTakedowns        (0x82338320)
//       Merges the two DirtyTrickQueue inputs into a DirtyTrickEvent_28 list and
//       tallies payback-used / payback-succeeded onto the CarData of each event.
//       Dereferences the forward-declared DirtyTrickQueue and the undefined
//       BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent_28_ container.
//       MISSING: BrnGameState::GameStateToNetworkInterface::DirtyTrickQueue
//                (+ BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent_28_) (definitions).
//
//   UpdateCrashes                 (0x8231F9B8)
//       Iterates lpQueue (RaceCarCrashEvent records); for each crash that counts,
//       increments the victim CarData's crash tally. lpQueue is
//       VehicleManagerOutputInterface::RaceCarCrashEventQueue*, forward-declared
//       only; the per-event accessor is likewise undefined here.
//       MISSING: BrnGameState::VehicleManagerOutputInterface::RaceCarCrashEventQueue
//                (definition).
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

namespace BrnGameState
{
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
}
