#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h
// ============================================================================
// KEYSTONE home for BrnGameState::ScoringSystem and the file-local records the DWARF places
// alongside it: CarData, RaceCarPositioningData, NetworkRoundData.
//
// SHAPE is DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../BrnScoringSystem.h): the ScoringSystem member run, the
// CarData / RaceCarPositioningData / NetworkRoundData layouts, and EVERY method signature are
// reproduced from the full dwarfdump in declared order + types (float32_t -> f32).
//
// This header is the single owner of these three types -- grow it in place, do NOT fork. The
// trivial getters/setters that are a 1-3 line read/write off the layout are given INLINE bodies;
// every substantial method (AddPlayer, ProcessFinishDistances, UpdateNetworkPlayerResults, the
// team logic, Construct/Release, the per-mode score updates, ...) is DECLARE-ONLY -- its body
// lands with BrnScoringSystem's own TU in a later round (the gate here is `cl /c`, compile only).
//
// EMBED-BY-VALUE rule: ScoringSystem embeds the per-mode sub-scorers (CrashModeScoring,
// StuntModeScoring, RoadRageModeScoring, OnlineRaceModeScoring, OnlineRoadRageModeScoring,
// OnlineBurningHomeRunModeScoring) and the BurnoutSkillzData[8] tally BY VALUE; each is a
// COMPLETE, COMPILABLE slice (named members + the right ORDER/TYPES), not a byte-exact sizeof.
//
// INCLUDE-CYCLE note: the two online-RACE / BURNING-HOME-RUN scorer headers themselves #include
// THIS header (their .cpp reaches the per-car records through ScoringSystem::GetCarData). They
// only name ScoringSystem / CarData BY POINTER in their class bodies, so the forward declarations
// below (emitted before those includes) satisfy them on the first pass; the full definitions here
// then let ScoringSystem embed the scorers by value.

#include "types.hpp"

#include "GameSource/BurnoutConstants.h"                  // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8)
#include "GameSource/GameState/BrnGameStateSharedIO.h"    // GameStateModuleIO::{CarScoreData, EPlayerTeam, EPlayerScoringIndex,
                                                          //   EGameModeType, ERoadRageCrashType, ScoringOutputInterface,
                                                          //   OnlineScoringOutputInterface, OutputBuffer}
#include "GameSource/GameState/BrnGameStateTypes.h"       // BrnGameState::LandmarkIndex
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h" // BrnNetwork::NetworkPlayerID (== s32), BrnNetwork::Road::ChallengeIndex
#include "GameShared/GameClasses/System/Timer/CgsTime.h"  // CgsSystem::Time
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N>
#include "GameShared/GameClasses/Core/CgsID.h"            // CgsID (== u64)
#include "SharedClasses/StreetData/BrnChallengeData.h"    // BrnStreetData::ScoreType (by-value param)

// CgsMemory::HeapMalloc is the network-heap allocator (DWARF type; no committed home in the tree
// yet -- distinct from CgsMemory::LinearMalloc). Used by pointer only (CarData::Prepare,
// ScoringSystem::Prepare, CarData::mpNetworkHeapMalloc), so a forward declaration suffices.
namespace CgsMemory { class HeapMalloc; }

// ---- forward declarations for the embedded sub-scorers (defined by the slice includes below) ----
// Emitted BEFORE the slice includes so that the online-scorer headers -- which include THIS header
// and name ScoringSystem / CarData by pointer -- resolve them on their first parse pass.
namespace BrnGameState
{
    class  ScoringSystem;        // keystone (defined below)
    struct CarData;              // file-local record (defined below)
    struct RaceCarPositioningData;
    struct NetworkRoundData;
}

#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"        // CrashModeScoring (by value)
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"                   // StuntModeScoring (by value) + AchievementManager
#include "GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.h"                // RoadRageModeScoring (by value)
#include "GameSource/GameState/ModeManager/Scoring/BrnOnlineRaceModeScoring.h"              // OnlineRaceModeScoring (by value)
#include "GameSource/GameState/ModeManager/Scoring/BrnOnlineRoadRageModeScoring.h"          // OnlineRoadRageModeScoring (by value)
#include "GameSource/GameState/ModeManager/Scoring/BrnOnlineBurningHomeRunModeScoring.h"    // OnlineBurningHomeRunModeScoring (by value)
#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"              // BaseOnlineModeScoring (mpCurrentOnlineModeScoring*)
#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h"                  // BurnoutSkillzData (by value)
#include "GameSource/GameState/BrnGameActions.h"                                            // GameStateModuleIO::OnlineGameResults (by value, the X360 +19920 member)

// ---- forward-declared peripheral types used BY POINTER/REFERENCE in declare-only signatures ----
// (No instance data; pointer/reference use only -- full homes are reconstructed by their own TUs.)
// The DWARF qualifies the event queues under per-system NAMESPACES (the project models them as
// `namespace X { struct Queue; }`, matching the BrnCarSelectManager / RoadRageScoring precedents),
// so each is forward-declared as a namespace + nested struct rather than a class with a nested type.

// The two RaceCarEntityModuleIO output interfaces the ScoringSystem update pass reads are real
// namespace-scoped types (defined in BrnRaceCarEntityModuleOutputInterface.h, which the .cpp
// partials include). Forward-declare them so the BrnGameState typedefs below can alias them by
// pointer without pulling that heavy IO header into the keystone (StuntModeScoring.h precedent).
namespace BrnWorld { namespace RaceCarEntityModuleIO {
    struct RCEntityActiveRaceCarOutputInterface;
    struct RCEntityGlobalRaceCarOutputInterface;
} }

namespace BrnGameState
{
    class  GameModeParams;
    class  ModeManager;
    class  PlayerResultsInterface;
    // DWARF typedefs: ActiveRaceCarOutputInterface / GlobalRaceCarOutputInterface ARE the
    // RaceCarEntityModuleIO output interfaces (NOT distinct GameState types). The .cpp partials
    // #include BrnRaceCarEntityModuleOutputInterface.h to complete them for member access.
    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarOutputInterface;
    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface GlobalRaceCarOutputInterface;
    // AICarOutputInterface's real home (BrnAI::AIModuleIO::AICarOutputInterface, a 2f TU) is not
    // reconstructed yet -> forward-declared class. Methods that DEREFERENCE it (UpdateRacePositions)
    // stay declare-only until it lands.
    class  AICarOutputInterface;

    namespace InputBuffer                  { struct GameActionQueue; struct TakedownEventQueue; }
    namespace GameStateToNetworkInterface  { struct DirtyTrickQueue; }
    namespace VehicleManagerOutputInterface{ struct RaceCarCrashEventQueue; }
    namespace VehicleOutputInterface       { struct PhysicalTrafficStateQueue; }

    // Pointer-only param of OnRoadRagePlayerCrashed. Full home is BrnGameStateModuleIO.h; a forward
    // declaration keeps that heavy IO header out of the keystone.
    namespace GameStateModuleIO { struct OutputBuffer; }
}

namespace BrnStreetData
{
    class ChallengeHighScoreEntry;           // pointer-only param of Get/SetRoadRulesScores etc.
}

// By-value enum param of the declare-only OnPlayerHitsRival. Opaque-enum forward declaration with
// the committed underlying type (BrnVehicleConstants.h: `enum EImpactType : s32`) so the keystone
// can name it by value without pulling the physics-constants header in.
namespace BrnPhysics { namespace Vehicle { enum EImpactType : s32; } }

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // File-scope constants the DWARF places in this header (BrnScoringSystem.h:52/53).
    // ------------------------------------------------------------------------
    const s32 KI_MAX_ROAD_RAGE_TAKEDOWN_COUNT = 3;
    const s32 KI_MAX_MARKED_MAN_TAKEDOWN_COUNT = 3;

    // ------------------------------------------------------------------------
    // Medal-target selector. DWARF home BrnScoringSystemMedals.h:26 (not yet reconstructed in
    // the source tree); the ScoringSystem keystone is the first type to need it, so it is grown
    // here. Enumerators verbatim from the DWARF.
    // ------------------------------------------------------------------------
    enum ECurrentMedalTargetTime : s32
    {
        E_CURRENT_MEDAL_TARGET_TIME_START  = 0,
        E_CURRENT_MEDAL_TARGET_TIME_GOLD   = 0,
        E_CURRENT_MEDAL_TARGET_TIME_SILVER = 1,
        E_CURRENT_MEDAL_TARGET_TIME_BRONZE = 2,
        E_CURRENT_MEDAL_TARGET_TIME_NONE   = 3,
        E_CURRENT_MEDAL_TARGET_TIME_SIZE   = 4,
    };

    // ------------------------------------------------------------------------
    // NetworkRoundData -- per-round cumulative network stats. DWARF home BrnScoringSystem.h:69.
    // Full member run is DWARF-authoritative (in order); both accessors are declare-only.
    // ------------------------------------------------------------------------
    struct NetworkRoundData
    {
        CgsSystem::Time mSecondsInEvent;             // :73
        s32 miNumberOfCrashes;                       // :74
        f32 mfMetersDriven;                          // :75
        s32 miNumberOfRounds;                        // :76

        s32 miTakedownsFor;                          // :78
        s32 miTakedownsAgainst;                      // :79

        s32 miTraitorousTakedownsFor;                // :81
        s32 miTraitorousTakedownsAgainst;            // :82

        s32 miMarksFor;                              // :85
        s32 miMarksAgainst;                          // :86
        s32 miMarkedManTakedownsFor;                 // :87
        s32 miMarkedManTakedownsAgainst;             // :88
        s32 miPaybacksUsedFor;                       // :89
        s32 miPaybacksUsedAgainst;                   // :90
        s32 miPaybacksSuceededFor;                   // :91
        s32 miPaybacksSuccededAgainst;               // :92

        // DWARF :97 / :101 -- declare-only (bodies in this TU).
        void SetPosition(s32 liIndex, s32 liPosition);
        s32  GetPosition(s32 liIndex) const;

    private:
        s32 maiPositions[10];                        // :104
    };

    // ------------------------------------------------------------------------
    // RaceCarPositioningData -- transient per-car race-position scratch. DWARF home
    // BrnScoringSystem.h:109. Full member run in order; Construct is declare-only.
    // ------------------------------------------------------------------------
    struct RaceCarPositioningData
    {
        EActiveRaceCarIndex meActiveRaceCarIndex;    // :111
        f32  mfDistanceToNextCheckpoint;             // :112
        f32  mfDistanceToFinish;                     // :113
        s32  miCurrentCheckpoint;                    // :114
        s32  miFinishPosition;                       // :115
        bool mbDisconnected;                         // :116

        void Construct();                            // :119  (declare-only)
    };

    // ------------------------------------------------------------------------
    // CarData -- per-car scoring record. DWARF home BrnScoringSystem.h:123. Embeds the per-car
    // CarScoreData at offset 0 (so GetScoreData() yields the same address as the CarData*), then
    // carries the car id, cumulative points, team/status, race-car index and network player id.
    //
    // The full member run + every method signature are DWARF-authoritative. The cluster-1 online
    // scorers reach the per-car record exclusively through GetScoreData() / GetActiveRaceCarIndex()
    // / GetNetworkPlayerID(), whose signatures are PRESERVED. Most methods are declare-only; the
    // trivial field getters/setters are given inline bodies.
    // ------------------------------------------------------------------------
    struct CarData
    {
    public:
        // DWARF BrnScoringSystem.h:126. Per-car play/eliminated status.
        enum EPlayerStatus
        {
            E_PLAYER_STATUS_PLAYING    = 0,
            E_PLAYER_STATUS_ELIMINATED = 1,
            E_PLAYER_STATUS_COUNT      = 2,
        };

        // ---- lifecycle (declare-only; bodies in this TU) ----
        void Construct();                                                 // :135
        bool Prepare(CgsMemory::HeapMalloc* lpNetworkHeapMalloc);         // :139
        bool Release();                                                   // :142
        void Clear();                                                     // :145
        void ClearCumulativeData();                                       // :148

        // ---- per-car score record (embedded CarScoreData @ offset 0) ----
        // DWARF :151 / :154. Trivial address-of; inline. Signatures the online scorers call.
        GameStateModuleIO::CarScoreData*       GetScoreData()       { return &mCarScoreData; }
        const GameStateModuleIO::CarScoreData* GetScoreData() const { return &mCarScoreData; }

        // ---- field getters (trivial reads; inline) ----
        CgsID       GetCarID() const                  { return mCarId; }                  // :157
        s32         GetCumulativePoints() const       { return miCumulativePoints; }      // :160
        s32         GetRoundDisconnected() const      { return miRoundDisconnectedIn; }   // :163
        f32         GetDriftDistance() const          { return mfCurrentDriftDistance; }  // :166
        EPlayerStatus GetStatus() const               { return mePlayerStatus; }          // :169
        GameStateModuleIO::EPlayerTeam GetTeam() const           { return mePlayerTeam; }            // :172
        GameStateModuleIO::EPlayerTeam GetRoundStartTeam() const { return meRoundStartPlayerTeam; }  // :175
        EActiveRaceCarIndex GetActiveRaceCarIndex() const { return meRaceCarIndex; }      // :178 (scorers call this)
        BrnNetwork::NetworkPlayerID GetNetworkPlayerID() const { return mNetworkPlayerID; } // :181 (scorers call this)

        // ---- field setters (trivial writes; inline) ----
        void SetCarID(CgsID lCarId)                       { mCarId = lCarId; }                 // :186
        void SetCumulativePoints(s32 liPoints)            { miCumulativePoints = liPoints; }   // :190
        void SetRoundDisconnected(s32 liRound)            { miRoundDisconnectedIn = liRound; } // :194
        void SetDriftDistance(f32 lfDistance)             { mfCurrentDriftDistance = lfDistance; } // :198
        void SetStatus(EPlayerStatus leStatus)            { mePlayerStatus = leStatus; }       // :202
        void SetTeam(GameStateModuleIO::EPlayerTeam leTeam)            { mePlayerTeam = leTeam; }           // :206
        void SetRoundStartTeam(GameStateModuleIO::EPlayerTeam leTeam)  { meRoundStartPlayerTeam = leTeam; } // :210

        // ---- road-rules high-score table (declare-only; touches mpaOnlineGameRoadRuleHighScores) ----
        void GetRoadRulesScores(BrnNetwork::Road::ChallengeIndex lChallenge,
                                BrnStreetData::ChallengeHighScoreEntry* lpEntry) const;       // :216
        void SetRoadRulesScores(BrnNetwork::Road::ChallengeIndex lChallenge,
                                BrnStreetData::ChallengeHighScoreEntry* lpEntry);             // :222
        void ResetRoadRulesScores();                                                          // :226

        // ---- more trivial setters (inline) ----
        void SetActiveRaceCarIndex(EActiveRaceCarIndex leIndex)        { meRaceCarIndex = leIndex; }       // :230
        void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lID)       { mNetworkPlayerID = lID; }         // :234
        void IncrementCumulativePoints(s32 liPoints)                  { miCumulativePoints += liPoints; } // :239
        void IncrementDriftDistance(f32 lfDistance)                   { mfCurrentDriftDistance += lfDistance; } // :243
        void SetEliminated()                                          { mbIsEliminated = true; }          // :246
        void IncrementCheckpointCount()                               { ++miCurrentCheckPoint; }          // :249
        s8   GetCurrentCheckPoint()                                   { return miCurrentCheckPoint; }     // :252
        void SetCurrentCheckPoint(s8 liCheckPoint)                    { miCurrentCheckPoint = liCheckPoint; } // :256
        void SetHasFever(bool lbHasFever)                             { mbHasFever = lbHasFever; }        // :260
        bool HasFever()                                              { return mbHasFever; }              // :263

    private:
        // ---- data members (DWARF declared order + types) ----
        GameStateModuleIO::CarScoreData mCarScoreData;        // :268  (offset 0)
        CgsID                           mCarId;               // :269
        s32                             miCumulativePoints;   // :270
        s32                             miRoundDisconnectedIn;// :271
        f32                             mfCurrentDriftDistance;// :272

        EPlayerStatus                   mePlayerStatus;       // :275
        GameStateModuleIO::EPlayerTeam  mePlayerTeam;         // :276
        GameStateModuleIO::EPlayerTeam  meRoundStartPlayerTeam;// :279

        EActiveRaceCarIndex             meRaceCarIndex;       // :281
        BrnNetwork::NetworkPlayerID     mNetworkPlayerID;     // :282

        BrnStreetData::ChallengeHighScoreEntry* mpaOnlineGameRoadRuleHighScores; // :285
        CgsMemory::HeapMalloc*          mpNetworkHeapMalloc;  // :286

        bool                            mbIsEliminated;       // :288
        s8                              miCurrentCheckPoint;  // :289 (DWARF int8_t)
        bool                            mbHasFever;           // :290
    };

    // ------------------------------------------------------------------------
    // ScoringSystem -- the per-game scoring system. DWARF home BrnScoringSystem.h:306.
    // Member run + every method signature are DWARF-authoritative (in order). The per-mode
    // sub-scorers + maCarData[8] + the positioning / skillz arrays are embedded by value.
    //
    // Trivial getters/setters get inline bodies; substantial methods are declare-only.
    // ------------------------------------------------------------------------
    class ScoringSystem
    {
    public:
        // ===== lifecycle / mode hooks (declare-only -- substantial) =====
        void Construct(StuntModeScoring::AchievementManager* lpAchievementManager); // :314 / X360 0x82337FE0
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc);                          // :319 / 0x8232A430
        bool Release();                                                             // :323 / 0x823124A0
        void OnModeStart(GameStateModuleIO::EGameModeType leGameMode,
                         const GameModeParams* lpParams, bool lbRestart);           // :330 / 0x82338220
        void OnModeEnd(bool lbAbort);                                               // :333
        void ClearCumulativeData();                                                 // :337 / 0x8231F140

        // ===== player roster (declare-only -- substantial) =====
        GameStateModuleIO::EPlayerScoringIndex AddPlayer();                                                  // :341
        GameStateModuleIO::EPlayerScoringIndex AddPlayer(BrnNetwork::NetworkPlayerID lID,
                                                         GameStateModuleIO::EPlayerTeam leTeam);             // :347 / 0x8231E288
        void SetPlayerRaceCarIndex(GameStateModuleIO::EPlayerScoringIndex leScoringIndex,
                                   EActiveRaceCarIndex leRaceCarIndex);                                      // :353 / 0x82310FB0
        void RemovePlayer(EActiveRaceCarIndex leRaceCarIndex);                                               // :358 / 0x8236B0A0
        void SetRivalEliminated(EActiveRaceCarIndex leRaceCarIndex);                                         // :362
        void RemovePlayer(BrnNetwork::NetworkPlayerID lID);                                                  // :367

        void WriteDataToOutput(GameStateModuleIO::ScoringOutputInterface* lpOutput,
                               GameStateModuleIO::OnlineScoringOutputInterface* lpOnlineOutput,
                               bool lbOnline);                                                               // :374 / 0x8232AE98

        // ===== mode timer =====
        void StartModeTimer(const CgsSystem::Time& lTime);                          // :381
        bool IsPlayerTotalled() const { return mbPlayerTotalled; }                  // :385 (inline)

        void OnRoadRagePlayerCrashed(GameStateModuleIO::OutputBuffer* lpOutput,
                                     GameStateModuleIO::ERoadRageCrashType leCrashType); // :391 / 0x823444B0
        void ResetRoadRageCrashesForPlayer();                                       // :394
        s32  GetRoadRagePlayerCrashes();                                            // :397
        s32  GetPlayerCrashesRemaining();                                           // :400

        void StopModeTimer(const CgsSystem::Time& lTime, EActiveRaceCarIndex leRaceCarIndex, bool lbForce); // :407 / 0x8231F590
        void StartOnlineGameModeScoring(GameStateModuleIO::EGameModeType leGameMode); // :412 / 0x823126C8
        void ClearModeTimer();                                                      // :417
        void ClearHighestPositions();                                               // :422 / 0x82326690
        bool IsTimerActive() const;                                                 // :426

        void SetTimeLimitSeconds(f32 lfSeconds);                                    // :432 / 0x82310838
        void SetMedalModeTimer(f32 lfGold, f32 lfSilver, f32 lfBronze);             // :439 / 0x823108E0
        void SetTimeLimitPerKm(f32 lfSecondsPerKm);                                 // :445 / 0x82312740
        const CgsSystem::Time GetTimeLimit() const;                                 // :449
        bool IsTimeLimitActive() const;                                             // :453
        void IncreaseTimeLimit(f32 lfSeconds);                                      // :458 / 0x823109E8
        void ClearTimeLimit();                                                      // :462
        const CgsSystem::Time GetElapsedTime(const CgsSystem::Time& lTime) const;   // :468
        const CgsSystem::Time GetModeTimeRemaining(const CgsSystem::Time& lTime);   // :474 / 0x82310A80
        bool HasModeTimeExpired(const CgsSystem::Time& lTime);                      // :480 / 0x82310B20
        void UpdateTimerForEliminator(const CgsSystem::Time& lTime);                // :485

        void SetCheckPointsForCarsWithinRace(s32 liCheckpoints);                    // :490
        bool AcheivedGold();                                                        // :493
        bool HasCrashModeEnded() const;                                             // :497
        bool HasStuntAttackModeEnded(CgsSystem::Time lTime);                        // :501 / 0x82326708

        // ===== laps / checkpoints / landmarks (trivial scalars inline) =====
        void SetTotalLaps(u32 luTotalLaps)            { muTotalLaps = luTotalLaps; }       // :508
        u32  GetTotalLaps() const                     { return muTotalLaps; }              // :512
        void SetTotalCheckpoints(s32 liTotalCheckpoints) { miTotalCheckpoints = liTotalCheckpoints; } // :517
        s32  GetTotalCheckpoints() const              { return miTotalCheckpoints; }       // :521
        void SetOnlineLandmarks(LandmarkIndex* lpLandmarks, s32 liCount);                  // :527 (declare-only)
        s32  GetTotalOnlineLandmarks() const          { return miTotalOnlineLandmarks; }   // :531

        void SetCheckpointDistances(u32 luCheckpoint, f32 lfDistance);              // :537 / 0x82310C30
        void SetPlayerDistanceToFinish(f32 lfDistance) { mfPlayerDistanceToFinishLastFrame = lfDistance; } // :542
        f32  GetPlayerDistanceToFinish()              { return mfPlayerDistanceToFinishLastFrame; } // :546
        f32  GetCheckpointDistanceToFinish(u32 luCheckpoint) const;                // :551 / 0x82310CC8
        bool IsCheckPointDistanceToFinishReady() const { return mbCheckPointDistancesToFinishReady; } // :555
        void ProcessFinishDistances(s32 liNumCheckpoints);                         // :560 / 0x823124F0
        const f32 GetTotalRaceDistance() const        { return mfTotalRaceDistance; } // :563

        // ===== gameplay event hooks (declare-only -- substantial) =====
        void OnPlayerDoesATakedown(CgsSystem::Time lTime,
                                   InputBuffer::GameActionQueue* lpQueue);          // :569 / 0x8234CE08
        void OnPlayerHitsRival(BrnPhysics::Vehicle::EImpactType leImpactType);      // :574
        void RegisterFinishForCar(bool lbFinished, EActiveRaceCarIndex leRaceCarIndex,
                                  const CgsSystem::Time& lTime);                    // :582 / 0x8231F198
        void RegisterCheckpointForCar(GameStateModuleIO::EGameModeType leGameMode, s32 liCheckpoint,
                                      LandmarkIndex lLandmark, EActiveRaceCarIndex leRaceCarIndex); // :589
        void RaceCarHasReachedCheckPointWithinEvent(EActiveRaceCarIndex leRaceCarIndex,
                                                    GameStateModuleIO::EGameModeType leGameMode);  // :594 / 0x82326E50
        bool IsOnlineLandmarkVisited(EActiveRaceCarIndex leRaceCarIndex, LandmarkIndex lLandmark); // :599
        s32  GetOnlineLandmarksVisited(EActiveRaceCarIndex leRaceCarIndex);                        // :603

        // ===== per-car race queries (declare-only -- search/compute) =====
        f32  GetRaceCarLapTimeSeconds(EActiveRaceCarIndex leRaceCarIndex, u32 luLap) const;  // :610
        u32  GetRaceCarNumCompletedLaps(EActiveRaceCarIndex leRaceCarIndex) const;           // :615 / 0x82326900
        CgsSystem::Time GetRaceCarTimeInCurrentTeam(EActiveRaceCarIndex leRaceCarIndex) const; // :620
        bool HasRaceCarFinished(EActiveRaceCarIndex leRaceCarIndex) const;                   // :625
        u32  GetCarRacePosition(EActiveRaceCarIndex leRaceCarIndex) const;                   // :630 / 0x82326980
        s32  GetCarRaceFinishPosition(EActiveRaceCarIndex leRaceCarIndex) const;             // :635 / 0x82326A08
        f32  GetRaceCarDistanceToFinish(EActiveRaceCarIndex leRaceCarIndex) const;           // :640 / 0x82326A90
        f32  GetRaceCarDistanceToPlayer(EActiveRaceCarIndex leRaceCarIndex) const;           // :645
        f32  GetPositionedCarDistanceToFinish(s32 liPosition) const;                         // :650
        f32  GetRaceCarDistanceToFinishAtRoundEnd(EActiveRaceCarIndex leRaceCarIndex) const; // :655 / 0x82326B18
        bool HasAnyCarFinished() const                { return mbACarHasFinishedTheRace; }   // :659 (inline)
        EActiveRaceCarIndex GetPositionedCarIndex(s32 liPosition) const;                     // :664
        EActiveRaceCarIndex GetRaceCarEliminatorIndex(EActiveRaceCarIndex leRaceCarIndex) const; // :669 / 0x82326BB0
        s32  GetNumberOfEliminations(EActiveRaceCarIndex leRaceCarIndex) const;              // :674 / 0x82326C38
        bool GetOvertakenRival(EActiveRaceCarIndex leRaceCarIndex) const;                    // :679

        // ===== lead / last (trivial inline) =====
        EActiveRaceCarIndex GetLead() const           { return meLeadRaceCarIndex; }         // :683 / 0x82310DA0
        EActiveRaceCarIndex GetLast() const           { return meLastRaceCarIndex; }         // :687 / 0x82356028
        bool GetNewLeader() const                     { return mbNewLeader; }                // :691
        bool GetNewLast() const                       { return mbNewLastPlace; }             // :695

        CgsSystem::Time GetFinishTime(EActiveRaceCarIndex leRaceCarIndex) const;             // :700 / 0x82326CB8
        const CgsSystem::Time GetRaceCarTotalTime(EActiveRaceCarIndex leRaceCarIndex,
                                                  const CgsSystem::Time& lTime) const;       // :707 / 0x8231F480
        EActiveRaceCarIndex GetCurrentLastPlaceActiveRaceCar();                              // :711
        void SetNumberOfCarsForEliminator(u32 luNumCars);                                    // :716
        const CgsSystem::Time GetRaceCarFastestLapTime(EActiveRaceCarIndex leRaceCarIndex) const; // :721 / 0x8231F880

        const s32 GetNumberOfTakedowns(EActiveRaceCarIndex leRaceCarIndex) const;            // :726 / 0x82326D50
        const s32 GetNumberOfCrashes(EActiveRaceCarIndex leRaceCarIndex) const;              // :731
        const s32 GetNumberOfTakedownsAgainst(EActiveRaceCarIndex leRaceCarIndex) const;     // :736 / 0x82326DD0
        const u32 GetNumberOfActiveCars() const       { return muCarsInCurrentMode; }        // :739 (inline)
        const s32 GetNumberOfFinishedCars() const     { return static_cast<s32>(muNumCarsFinishedRace); } // :743

        // ===== road-rules high scores (declare-only) =====
        void GetOnlinePlayersChallengeHighScores(BrnNetwork::Road::ChallengeIndex lChallenge,
                                                 BrnStreetData::ChallengeHighScoreEntry* lpEntries); // :749
        void GetHighestLobbyRoadRuleScore(BrnNetwork::Road::ChallengeIndex lChallenge,
                                          BrnStreetData::ScoreType leScoreType,
                                          s32* lpiScore, EActiveRaceCarIndex* lpeRaceCarIndex);      // :757 / 0x8232B280

        // ===== per-frame update pass (declare-only -- substantial) =====
        void UpdateNumberOfCarsInMode(const ActiveRaceCarOutputInterface* lpOutput);         // :762 / 0x8231F3F0
        void UpdateRacePositions(const ActiveRaceCarOutputInterface* lpActive,
                                 const GlobalRaceCarOutputInterface* lpGlobal,
                                 const AICarOutputInterface* lpAI,
                                 ModeManager* lpModeManager);                                 // :770 / 0x8232A668
        void UpdateTeamStats(f32 lfDeltaTime);                                                // :775 / 0x8231F308
        void UpdateTakedowns(const InputBuffer::TakedownEventQueue* lpQueue);                 // :780 / 0x8232AC88
        void UpdatePaybackTakedowns(const GameStateToNetworkInterface::DirtyTrickQueue* lpQueueA,
                                    const GameStateToNetworkInterface::DirtyTrickQueue* lpQueueB); // :786 / 0x82338320
        void UpdateCrashes(const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpQueue); // :791 / 0x8231F9B8
        void UpdateNetworkPlayerResults(const PlayerResultsInterface* lpResults, bool lbFinal); // :797 / 0x8231FA90
        void UpdateCumulativeResults(u32 luRound, s32 liNumCars, bool lbFinal);               // :804 / 0x8231FCA0
        int  CompareRaceCarDistances(const void* lpA, const void* lpB);                       // :810 / 0x823125B8
        void UpdateDistanceToPlayer(const ActiveRaceCarOutputInterface* lpOutput);            // :815 / 0x8232B408
        void DetectPlayerDrivingWrongWay(const ActiveRaceCarOutputInterface* lpOutput, f32 lfDeltaTime); // :821 / 0x8232B6B8
        void DetectPlayerStationary(const ActiveRaceCarOutputInterface* lpOutput, f32 lfDeltaTime);      // :827 / 0x82320008
        void UpdateCrashModeScore(const ActiveRaceCarOutputInterface* lpOutput,
                                  const VehicleOutputInterface::PhysicalTrafficStateQueue* lpQueue,
                                  f32 lfDeltaTime);                                            // :834
        void UpdateStuntAttackModeScore(const ActiveRaceCarOutputInterface* lpOutput, f32 lfDeltaTime); // :840
        void UpdateRoadRageModeScore(const ActiveRaceCarOutputInterface* lpOutput, f32 lfDeltaTime);    // :846
        void SetRoadRageDetails(u32 luTargetTakedowns, u32 luExtensionTime);                  // :851
        void CheckRoadRageMedalAwarded(u32 luTakedowns);                                      // :855 / 0x82312840
        void UpdateGeneralStats(const ActiveRaceCarOutputInterface* lpOutput, f32 lfDeltaTime, bool lbOnline); // :864 / 0x8232B8C0

        // ===== points leader / loser / standings (declare-only -- search) =====
        BrnNetwork::NetworkPlayerID GetPointsLeader() const;                                  // :868 / 0x82312648
        EActiveRaceCarIndex GetPointsLoser() const;                                           // :872
        s32  GetRaceCarStandingsPosition(EActiveRaceCarIndex leRaceCarIndex) const;           // :877 / 0x82320190
        s32  GetCumulativePoints(EActiveRaceCarIndex leRaceCarIndex);                         // :882

        CarData::EPlayerStatus GetPlayerStatus(EActiveRaceCarIndex leRaceCarIndex) const;     // :886
        void SetPlayerStatus(EActiveRaceCarIndex leRaceCarIndex, CarData::EPlayerStatus leStatus); // :891
        void SetPlayerEliminated(EActiveRaceCarIndex leRaceCarIndex, EActiveRaceCarIndex leEliminator); // :897 / 0x823267A0
        void SetPlayerDisconnected(BrnNetwork::NetworkPlayerID lID);                          // :902 / 0x8231D950
        bool GetPlayerDisconnected(EActiveRaceCarIndex leRaceCarIndex) const;                 // :907 / 0x8231DA00
        bool GetPlayerDisconnected(BrnNetwork::NetworkPlayerID lID) const;                    // :911
        void ClearDisconnectedPlayers();                                                      // :915 / 0x8231DBB0

        // ===== team (declare-only -- search) =====
        GameStateModuleIO::EPlayerTeam GetPlayerTeam(EActiveRaceCarIndex leRaceCarIndex) const;        // :919 / 0x8231FDB8
        void SetPlayerTeam(EActiveRaceCarIndex leRaceCarIndex, GameStateModuleIO::EPlayerTeam leTeam); // :924 / 0x8231FE38
        GameStateModuleIO::EPlayerTeam GetRoundStartPlayerTeam(EActiveRaceCarIndex leRaceCarIndex) const; // :929
        void SetRoundStartPlayerTeam(EActiveRaceCarIndex leRaceCarIndex, GameStateModuleIO::EPlayerTeam leTeam); // :935
        // Per-team online-stunt-score queries (bodies in BrnScoringSystem_Standings.cpp). Team index ranges
        // over the per-player team slots, not the 3-value EPlayerTeam enum.
        s32 GetTeamStuntScore(s32 liTeam) const;                                                          // 0x82320650
        s32 GetLeadingStuntTeam(s32 liTeamToExclude) const;                                               // 0x823206E0

        // ===== sub-scorer accessors (trivial address-of; inline) =====
        CrashModeScoring*           GetCrashScorer()         { return &mCrashModeScoring; }   // :939
        StuntModeScoring*           GetStuntScorer()         { return &mStuntModeScoring; }   // :943
        RoadRageModeScoring*        GetRoadRageScoring()       { return &mRoadRageModeScoring; } // :947
        const RoadRageModeScoring*  GetRoadRageScoring() const { return &mRoadRageModeScoring; } // :950

        bool IsBlueTeamEliminated() const;                                                    // :954 / 0x8231FEE8
        bool PrepareRoadRageScoring(s32 liTargetNumTakedowns, u16 luExtensionTime);           // :960
        bool AreAllRemotePlayersDisconnected(EActiveRaceCarIndex leRaceCarIndex);             // :965
        s32  GetNumberOfNonDisconnectedPlayers();                                             // :969 / 0x8231FF80
        void SetCheckPointDistancesToFinishReady(bool lbReady) { mbCheckPointDistancesToFinishReady = lbReady; } // :974 (inline)
        void StoreCarIds(const ActiveRaceCarOutputInterface* lpOutput);                       // :979 / 0x8232B7C0

        f32  GetTotalDistanceDriven(EActiveRaceCarIndex leRaceCarIndex) const;                // :984
        bool IsNetworkCarsDistanceDrivenValid(EActiveRaceCarIndex leRaceCarIndex) const;      // :989
        CgsSystem::Time GetTimeSpentInFirstPlace(EActiveRaceCarIndex leRaceCarIndex) const;   // :994 / 0x82326F28
        CgsSystem::Time GetTimeSpentInLastPlace(EActiveRaceCarIndex leRaceCarIndex) const;    // :999 / 0x82326FC0
        bool HaveCarsBeenSortedIntoRacePositions() const;                                     // :1003
        CgsSystem::Time GetTimeSpentBoosting(EActiveRaceCarIndex leRaceCarIndex) const;       // :1008 / 0x82327058
        f32  GetLongestDrift(EActiveRaceCarIndex leRaceCarIndex) const;                       // :1013
        s32  GetMarkedManTakedowns(EActiveRaceCarIndex leRaceCarIndex) const;                 // :1018

        // ===== network round data (declare-only + trivial accessor) =====
        void SaveNetworkRoundData(BrnNetwork::NetworkPlayerID lID, CgsSystem::Time lTime,
                                  EActiveRaceCarIndex leRaceCarIndex);                         // :1025 / 0x8232BC78
        void ClearNetworkRoundData();                                                         // :1029
        NetworkRoundData* GetNetworkRoundData()       { return &mNetworkRoundData; }          // :1033 (inline)

        f32  GetPlayerWrongWayTime() const            { return mfPlayerTimeHeadingTheWrongWay; } // :1037
        f32  GetPlayerStationaryTime() const          { return mfPlayerTimeStationary; }      // :1041
        f32  GetPlayerNoInputTime() const             { return mfPlayerTimeWithoutInput; }    // :1045
        void ResetPlayerNoInputTime()                 { mfPlayerTimeWithoutInput = 0.0f; }    // :1049
        void ResetPlayerStationaryTime()              { mfPlayerTimeStationary = 0.0f; }      // :1053

        // ===== per-car record lookup (the keystone surface the cluster-1 scorers call) =====
        // X360 0x8231DC18 (non-const) / 0x8231DCD0 (const). Returns &maCarData[slot] whose
        // meRaceCarIndex matches leActiveRaceCarIndex, else NULL. Declare-only (search loop body
        // lands in this TU); the signatures the online scorers depend on are PRESERVED.
        const CarData* GetCarData(EActiveRaceCarIndex leActiveRaceCarIndex) const;            // :1057
        CarData*       GetCarData(EActiveRaceCarIndex leActiveRaceCarIndex);                  // :1061 / 0x8231DC18
        CarData*       GetCarData(BrnNetwork::NetworkPlayerID lID);                           // :1065
        const CarData* GetCarData(BrnNetwork::NetworkPlayerID lID) const;                     // :1069

        // X360 0x82310E30 (non-const). Direct index by player scoring slot (no match search).
        CarData*       GetCarDataFromPlayerScoringIndex(GameStateModuleIO::EPlayerScoringIndex leIndex);       // :1073 / 0x82310E30
        const CarData* GetCarDataFromPlayerScoringIndex(GameStateModuleIO::EPlayerScoringIndex leIndex) const; // :1077

        GameStateModuleIO::EPlayerScoringIndex GetPlayerScoringIndex(EActiveRaceCarIndex leRaceCarIndex) const;   // :1081 / 0x8231DE38
        GameStateModuleIO::EPlayerScoringIndex GetPlayerScoringIndex(BrnNetwork::NetworkPlayerID lID) const;      // :1085
        bool IsPlayerInScoringSystem(GameStateModuleIO::EPlayerScoringIndex leIndex) const;   // :1089 / 0x8231DFC8
        bool IsNetworkPlayerInScoringSystem(BrnNetwork::NetworkPlayerID lID) const;           // :1093 / 0x8231E050

        s32  GetNumberOfNetworkPlayers() const;                                               // :1098 / 0x82311020
        s32  GetNumberOfNetworkPlayersStillConnected() const;                                 // :1101 / 0x823560B8
        bool AreAllRaceCarsSetup() const;                                                     // :1104 / 0x82311098
        void LocalPlayerQuit(BrnNetwork::NetworkPlayerID lID);                                 // :1108

        // ===== medal / eliminator targets (declare-only -- substantial) =====
        void SetNextMedalTargetTime();                                                        // :1111
        void SetNextEliminatorTargetTime();                                                   // :1114
        void SetNextTargetTime();                                                             // :1117
        f32  GetCurrentLandmarkDistance();                                                    // :1120
        const CgsSystem::Time GetCurrentModeTimeTarget() const;                               // :1123
        const ECurrentMedalTargetTime GetCurrentMedalTarget()   { return meCurrentMedalTarget; }   // :1126 (inline)
        const ECurrentMedalTargetTime GetCurrentMedalAchieved() { return meCurrentMedalAchieved; } // :1129 (inline)

        EActiveRaceCarIndex GetNextTeamMember(EActiveRaceCarIndex leRaceCarIndex,
                                              GameStateModuleIO::EPlayerTeam leTeam) const;    // :1135 / 0x82320270

        // ===== burnout-skillz tally (declare-only) =====
        void AddPlayerBurnoutSkillz(BrnNetwork::NetworkPlayerID lID, BrnNetwork::NetworkPlayerID lOtherID); // :1141 / 0x823561D0
        BurnoutSkillzData* GetBurnoutSkillzData(EActiveRaceCarIndex leRaceCarIndex);          // :1146 / 0x82311110
        BurnoutSkillzData* GetBurnoutSkillzData(BrnNetwork::NetworkPlayerID lID);             // :1151
        void ClearAllBurnoutSkillzData();                                                     // :1155 / 0x82311198
        void ClearPlayersBurnoutSkillzData(BrnNetwork::NetworkPlayerID lID);                  // :1160 / 0x82311210
        void SetBurnoutSkillzData(BrnNetwork::NetworkPlayerID lID, const BurnoutSkillzData* lpData); // :1166 / 0x82356138

        bool DidPlayerFinishOnlineEventFirst(EActiveRaceCarIndex leRaceCarIndex);             // :1171
        s32  GetOnlineFinishPosition(EActiveRaceCarIndex leRaceCarIndex);                     // :1176 / 0x823112A8
        bool HasBeatenRoadRageTarget();                                                       // :1180
        bool HasTargetScoreBeenExceeded() const;                                              // :1183
        void ResetOnlineCheckpointsVisited(EActiveRaceCarIndex leRaceCarIndex);               // :1188
        void ReducePlayerDurability();                                                        // :1192

    private:
        void ClearData(bool lbFull);                                                          // :1197 / 0x8232A4A8

        // ===== data members (DWARF declared order + types) =====
        CgsSystem::Time mStartTime;                  // :1199
        CgsSystem::Time mEndTime;                    // :1200
        CgsSystem::Time mTotalTime;                  // :1201
        CgsSystem::Time mTimeRemaining;              // :1202

        CrashModeScoring    mCrashModeScoring;       // :1206 (by value)
        StuntModeScoring    mStuntModeScoring;       // :1209 (by value)
        RoadRageModeScoring mRoadRageModeScoring;    // :1212 (by value)
        s32  miMaximumPlayerCrashedNumber;           // :1213
        s32  miCurrentPlayerCrashedNumber;           // :1214
        bool mbPlayerTotalled;                       // :1215
        u32  mauiMedalScores[4];                     // :1216

        OnlineRaceModeScoring           mOnlineRaceModeScoring;        // :1219 (by value)
        OnlineRoadRageModeScoring       mOnlineRoadRageScoring;        // :1220 (by value)
        OnlineBurningHomeRunModeScoring mOnlineBurningHomeRunScoring;  // :1221 (by value)

        BaseOnlineModeScoring* mpCurrentOnlineModeScoring;            // :1224

        NetworkRoundData mNetworkRoundData;          // :1227

        // X360 ScoringSystem +19920 (0x4DC0). The per-event online results aggregate the keystone
        // layout previously omitted -- restored as a by-value member so SaveNetworkRoundData /
        // ClearData / WriteDataToOutput can reach it by name (the full type is in BrnGameActions.h,
        // its own DWARF home; semantic-parity placement, not byte-exact -- accessed by name only).
        GameStateModuleIO::OnlineGameResults mOnlineGameResults;

        u32  muTotalLaps;                            // :1230
        u32  muNumCarsFinishedRace;                  // :1231
        s32  miTotalCheckpoints;                     // :1232
        s32  miTotalOnlineLandmarks;                 // :1233
        u32  muCarsInCurrentMode;                    // :1234
        u32  muActualNumberOfCarsInCurrentMode;      // :1235
        EActiveRaceCarIndex meLeadRaceCarIndex;      // :1236
        EActiveRaceCarIndex meLastRaceCarIndex;      // :1237
        bool mbNewLeader;                            // :1238
        bool mbNewLastPlace;                         // :1239
        bool mbACarHasFinishedTheRace;               // :1240

        CarData                maCarData[8];                 // :1243 (by value)
        RaceCarPositioningData maRaceCarPositioningData[8];  // :1244
        BrnNetwork::NetworkPlayerID maBurnoutSkillzPlayerIDs[8]; // :1245
        BurnoutSkillzData      maBurnoutSkillzData[8];       // :1246 (by value)

        f32  mafCheckpointSeparations[16];           // :1248
        f32  mafCheckpointDistancesToFinish[16];     // :1249
        bool mbCheckPointDistancesToFinishReady;     // :1250

        f32  mfTotalRaceDistance;                    // :1252

        f32  mfPlayerDistanceToFinishLastFrame;      // :1255
        f32  mfPlayerTimeHeadingTheWrongWay;         // :1256
        f32  mfPlayerTimeStationary;                 // :1257
        f32  mfPlayerTimeWithoutInput;               // :1258

        bool mbMedalMode;                            // :1261
        f32  mafMedalTimes[4];                       // :1262
        ECurrentMedalTargetTime meCurrentMedalTarget;   // :1263
        ECurrentMedalTargetTime meCurrentMedalAchieved; // :1264

        bool mbEliminationMode;                      // :1267
        bool mbPlayerElimated;                       // :1268
        EActiveRaceCarIndex mLastCarRaceCarIndex;    // :1269
        f32  mfEliminatorTimeStep;                   // :1270
        bool mbEliminationRequired;                  // :1271
        u32  muCarsThatHaventBeenEliminated;         // :1272

        Array<EActiveRaceCarIndex, 7u> maEliminatedActiveRaceCarIndexs; // :1275

        s32  miUpdateRacePositionsPM;                // :1279
    };
}
