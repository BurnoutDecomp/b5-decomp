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
//   GetHighestLobbyRoadRuleScore  (0x8232B280)
//       Reads each CarData's road-rule ChallengeHighScoreEntry table and runs a
//       best-score scan via BrnStreetData::ChallengeData::ContainsData(...) and
//       BrnStreetData::ChallengeData::CompareScores(...). Neither method is
//       declared on the committed ChallengeData (BrnChallengeData.h only homes
//       Construct/GetScore/SetScore/Copy). Growing ChallengeData with those two
//       methods is the ChallengeData TU's job, not this body's.
//       MISSING: BrnStreetData::ChallengeData::ContainsData / ::CompareScores.
//
//   UpdateNumberOfCarsInMode      (0x8231F3F0)
//       Body is muCarsInCurrentMode = lpOutput->maCarsInTheRace.GetLength()
//       (with the <=8 assert). lpOutput is BrnGameState::ActiveRaceCarOutputInterface*,
//       which the keystone only forward-declares -- the GameState-side interface
//       has no committed definition (the World-side
//       RCEntityActiveRaceCarOutputInterface is a DISTINCT C++ type; aliasing the
//       two is a keystone/layout change, out of scope for a body agent).
//       MISSING: BrnGameState::ActiveRaceCarOutputInterface (definition).
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
//   UpdateTeamStats               (0x8231F308)
//       Per car, accumulates the frame delta-time into two CgsSystem::Time slots
//       inside the embedded CarScoreData (X360 +0x6C unconditional, +0x74 indexed
//       by a per-car field). Those two Time slots are NOT named members of the
//       committed CarScoreData (BrnGameStateSharedIO.h) -- they fall inside the
//       unnamed maStorage6A[26] padding blob. Reaching them requires either offset
//       arithmetic into a blob (forbidden) or growing CarScoreData with named Time
//       fields (CarScoreData TU's job, and its byte accounting is load-bearing).
//       MISSING: named per-team CgsSystem::Time fields on
//                GameStateModuleIO::CarScoreData (currently unnamed maStorage6A blob).
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

// No compiling out-of-line bodies land in this TU yet -- see the BLOCKED ledger
// above. The translation unit is intentionally body-free so it compiles clean
// (cl /c) and the seven blocked methods stay cleanly deferred to the round that
// reconstructs their blocking keystones.
