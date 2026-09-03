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
#include "GameSource/GameState/ModeManager/Scoring/BrnOnlineStuntRunModeScoring.h"          // OnlineStuntRunModeScoring (by value, the X360 ss+0x4D44 online stunt-run scorer)
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

// UpdateNetworkPlayerResults' lpResults param is BrnNetwork::BrnNetworkModuleIO::PlayerResultsInterface
// (proven: the X360 body's per-record assert path is
// GameSource/Network/SharedIO/BrnNetworkModulePlayerResultsInterface.h; the DWARF type is the network
// one). Forward-declare it and alias it into BrnGameState so the keystone names it BY POINTER without
// pulling the heavy network IO header in -- the body TU (BrnScoringSystem_UpdateB.cpp) includes
// BrnNetworkModuleIO.h to complete it for the deref.
namespace BrnNetwork { namespace BrnNetworkModuleIO { struct PlayerResultsInterface; } }
namespace BrnAI { namespace AIModuleIO { struct AICarOutputInterface; } }  // real home BrnAICarOutputInterface.h

namespace BrnGameState
{
    class  GameModeParams;
    class  ModeManager;
    typedef BrnNetwork::BrnNetworkModuleIO::PlayerResultsInterface PlayerResultsInterface;
    // DWARF typedefs: ActiveRaceCarOutputInterface / GlobalRaceCarOutputInterface ARE the
    // RaceCarEntityModuleIO output interfaces (NOT distinct GameState types). The .cpp partials
    // #include BrnRaceCarEntityModuleOutputInterface.h to complete them for member access.
    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarOutputInterface;
    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface GlobalRaceCarOutputInterface;
    // DWARF typedef: AICarOutputInterface IS BrnAI::AIModuleIO::AICarOutputInterface (reconstructed,
    // BrnAICarOutputInterface.h). The .cpp partials #include that header to deref it; pointer use here.
    typedef BrnAI::AIModuleIO::AICarOutputInterface AICarOutputInterface;

    // GameActionQueue is the real GameStateModuleIO typedef (BrnGameStateSharedIO.h); only
    // TakedownEventQueue stays an incomplete nested forward decl here.
    namespace InputBuffer                  { struct TakedownEventQueue; }
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

        // ⭐ RETIRED 2026-09-03 (link-closure lane P3). Two declare-only grows stood here for the
        // BrnGameState::DeveloperChallengeManager TU:
        //     s32  GetFinishScore() const;    // "the finish score word at CarData+252"
        //     bool IsFlawless() const;        // "the flawless/no-damage flag at CarData+32"
        // Neither is in the DWARF and neither reading survives the asm:
        //   * CarData+252 (0xFC) is inside the embedded CarScoreData and is already named there --
        //     miBarrelRollCount, pinned by ScoringSystem::PlayerPerformedBarrelRolls @0x823634D0
        //     (`lwz/add/stw 0xFC(GetCarData)`). DeveloperChallengeManager::OnEventWin @0x8238DE64
        //     reads that same slot, so the call site now says GetScoreData()->GetBarrelRollCount().
        //   * "CarData+32" was a mis-attribution: OnEventWin's flawless test @0x8238DF78..0x8238DF8C
        //     reads +0x20 of a DIFFERENT object -- the BoostOutputInfo the ACTIVE-CAR OUTPUT
        //     INTERFACE hands back (GetBoostOutputInfoN @0x823101C0 returns iface+0x210+36*idx, and
        //     +0x20 of that 36-byte record is BoostOutputInfo::meBoostType). Nothing on CarData.
        // Both had exactly one caller and no body anywhere in the tree, so they are removed rather
        // than given invented bodies; the caller now reads the two real members by name.

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
        // [road-rage wave 2026-09-02] ModeManager::SetupGameMode @0x8234B5C8..0x8234B5D8 writes the
        // medal seed (mauiMedalScores[0..2] @ss+0x4B60, meCurrentMedalTarget @0x5D08,
        // meCurrentMedalAchieved @0x5D0C) directly -- the DWARF declares no setter for them, so the
        // only shape that reproduces those stores by name is a friend grant. Additive; no member moves.
        friend class ModeManager;

    public:
        // X360 0x827E0998. Default constructor. The X360 body sets each embedded sub-object's
        // vtable + its internal -1 sentinels and runs the per-car array element ctors (all of
        // which are AUTOMATIC in C++ via the embedded members' own constructors) and zero-inits
        // the four head CgsSystem::Time members (also automatic -- Time's default ctor yields
        // {0, 0.0f}); the only ScoringSystem-OWNED scalar it explicitly stores is the tail
        // miUpdateRacePositionsPM = -1 sentinel. All other ScoringSystem scalars are left for the
        // post-construction ClearData() pass (the X360 ctor does not touch them either).
        ScoringSystem();                                                            // X360 0x827E0998

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

        // X360 0x8232AE98 carries a 4th arg (r7) the PS3 DWARF 3-param sig drops: the player's
        // active-race-car index, used for out.mePlayerRaceCarIndex + the online GetCarData(idx) path.
        void WriteDataToOutput(GameStateModuleIO::ScoringOutputInterface* lpOutput,
                               GameStateModuleIO::OnlineScoringOutputInterface* lpOnlineOutput,
                               bool lbOnline,
                               EActiveRaceCarIndex lePlayerRaceCarIndex);                                    // :374 / 0x8232AE98

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
        // X360 0x82326708: the PS3 DWARF 1-Time sig is wrong; the body + its caller pass (Time&,
        // raceCarIndex, online-flag) -- the index picks the looked-up car (SetEliminated @+0xD9) and
        // the flag selects the online (mOnlineStuntModeScoring) vs offline (mStuntModeScoring) scorer.
        bool HasStuntAttackModeEnded(const CgsSystem::Time& lTime, EActiveRaceCarIndex leRaceCarIndex, bool lbOnline); // :501 / 0x82326708

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
                                   GameStateModuleIO::GameActionQueue* lpQueue);          // :569 / 0x8234CE08
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

        // ===== ADDITIVE GROW (declare-only) for the AchievementManagerBase TU =====
        // FLAG: these three accessors name the deep reads AchievementManagerBase makes
        // THROUGH its mpScoringSystem back-pointer (X360 OnBodyShop 0x8235AA18, OnTakedown
        // 0x8235AAE0, OnEventWin 0x82372978). The X360 reads them as raw offsets into the
        // ScoringSystem and its embedded RoadRage/CarScore sub-objects; the precise members
        // these offsets land on are owned by the ScoringSystem TU and are NOT mapped here.
        // Signatures + semantics are X360-asm-attested; offsets are out of scope. Bodies land
        // with the ScoringSystem TU; declare-only suffices for the `cl /c` compile gate.

        // OnBodyShop: count of cars newly wrecked-but-not-yet-repaired this frame -- the X360
        // computes (this+0x4B58) - (this+0x4B5C) and fires E_ACHIEVEMENT_REPAIR_FIRST_WRECKED_CAR
        // when it equals 1 in E_MODE_ROAD_RAGE.
        s32 GetNewlyWreckedCarCount() const;

        // OnTakedown: takedowns the player has scored in the current (road-rage) mode; X360
        // reads this+0x4B40 and requires it >= 10 for the perfect-road-rage achievement.
        s32 GetPlayerModeTakedowns() const;

        // OnTakedown: crashes the player has suffered in the current (road-rage) mode; X360
        // reads this+0x4B5C and requires it == 0 (no crashes) for perfect-road-rage.
        s32 GetPlayerModeCrashes() const;

        // OnEventWin (E_MODE_STUNT_ATTACK): the player's score for the millionaires-club check.
        // The X360 selects the online (this+0x2620) vs offline (this+0x350) car-score block by
        // lbOnline and reads its +0x10 score field, comparing >= 1,000,000.
        s32 GetPlayerScore(bool lbOnline) const;

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

        // ===== stunt-score setters / online-stunt pre-world (bodies in BrnScoringSystem_UpdateB.cpp) =====
        // Per-car online stunt score (CarScoreData +0xD4). SetPlayerStuntScore keys by active-race-car
        // slot, SetNetworkStuntScore by network player id; both store via CarData::GetScoreData().
        void SetPlayerStuntScore(EActiveRaceCarIndex leRaceCarIndex, s32 liScore);                         // 0x8231F0C8
        void SetNetworkStuntScore(BrnNetwork::NetworkPlayerID lID, s32 liScore);                           // 0x8231FC20
        // SetNetworkStuntMultiplier (0x8231FC50) arms the per-car "chainable stunt multiplier" trio
        // (CarScoreData +0xC8 miChainableScore / +0xD0 miCurrentChainableMultiplier / +0xD8 mbChainActive)
        // on GetCarData(lID); reconciled arg order from the X360 register setup (r5->+0xD0, r6->+0xC8).
        // Body in BrnScoringSystem_UpdateB.cpp (writes via the CarScoreData chainable-trio accessors).
        void SetNetworkStuntMultiplier(BrnNetwork::NetworkPlayerID lID, s32 liMultiplier, s32 liChainableScore); // 0x8231FC50

        // PlayerPerformedBarrelRolls (0x823634D0) adds liCount to the per-car barrel-roll tally at
        // CarScoreData +0xFC (carved as miBarrelRollCount). Body in BrnScoringSystem_UpdateB.cpp.
        void PlayerPerformedBarrelRolls(EActiveRaceCarIndex leRaceCarIndex, s32 liCount);                  // 0x823634D0

        // Forward a completed world-stunt action to the online (mOnlineStuntModeScoring) or offline
        // (mStuntModeScoring) stunt scorer; the small WorldStuntAction is modeled by pointer in the
        // scorer's home (BrnStuntModeScoring.h, included above).
        void DealWithStunt(const GameStateModuleIO::WorldStuntAction* lpAction, bool lbOnline);            // 0x823384F0
        // Per-frame pre-world step driving the online stunt scorer's PreWorldUpdate (reconciled
        // AddEvent-queue signature). VariableEventQueue<13312,16> is forward-declared via the stunt header.
        void UpdateOnlineStuntModeScorePreWorld(s32 liCurrentTimeMs,
                                                CgsModule::VariableEventQueue<13312, 16>* lpOutputActionQueue); // 0x8234CE48

        // ===== points leader / loser / standings (declare-only -- search) =====
        BrnNetwork::NetworkPlayerID GetPointsLeader() const;                                  // :868 / 0x82312648
        EActiveRaceCarIndex GetPointsLoser() const;                                           // :872
        s32  GetRaceCarStandingsPosition(EActiveRaceCarIndex leRaceCarIndex) const;           // :877 / 0x82320190
        s32  GetCumulativePoints(EActiveRaceCarIndex leRaceCarIndex);                         // :882

        CarData::EPlayerStatus GetPlayerStatus(EActiveRaceCarIndex leRaceCarIndex) const;     // :886
        void SetPlayerStatus(EActiveRaceCarIndex leRaceCarIndex, CarData::EPlayerStatus leStatus); // :891
        void SetPlayerEliminated(EActiveRaceCarIndex leRaceCarIndex, EActiveRaceCarIndex leEliminator); // :897 / 0x823267A0
        void SetPlayerDisconnected(BrnNetwork::NetworkPlayerID lID);                          // :902 / 0x8231D950
        // [!] ADDRESSES CORRECTED 2026-08-26 (wave-B fix round) -- the two overloads were swapped.
        // I dumped both: 0x8231DA00 fires "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID"
        // (@0x8231DA30) and ends in `lbz r3, 0x4F69(r11)` -- that is the NetworkPlayerID overload.
        // The EActiveRaceCarIndex overload is the header-inline the compiler folded at 0x82326878:
        // its assert is "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR..." (BrnScoringSystem.h:1900) and
        // its body is `GetCarData(i) ? carData+0x69 : 0` (`lbz r3, 0x69(r3)` @0x823268E4).
        bool GetPlayerDisconnected(EActiveRaceCarIndex leRaceCarIndex) const;                 // :907 / folded @0x82326878
        bool GetPlayerDisconnected(BrnNetwork::NetworkPlayerID lID) const;                    // :911 / 0x8231DA00
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
        // Per-team roster / elimination queries (bodies in BrnScoringSystem_Standings.cpp). Team index is
        // the raw per-player team slot (free-for-all stunt-run), compared against CarData::GetTeam() as
        // an s32 -- the GetTeamStuntScore convention. Reconciled from the X360 search-loop bodies.
        s32                          GetTeamPlayerCount(s32 liTeam) const;                                 // 0x823203D8
        BrnNetwork::NetworkPlayerID  GetFirstTeamPlayer(s32 liTeam) const;                                 // 0x82320460
        bool                         IsTeamEliminated(s32 liTeam) const;                                   // 0x823205B8
        bool                         AreAllOtherTeamsEliminated(s32 liTeam) const;                         // 0x82320770

        // ===== sub-scorer accessors (trivial address-of; inline) =====
        CrashModeScoring*           GetCrashScorer()         { return &mCrashModeScoring; }   // :939
        StuntModeScoring*           GetStuntScorer()         { return &mStuntModeScoring; }   // :943
        // Address-of the X360-only ONLINE stunt scorer (mOnlineStuntModeScoring @ ss+0x2620).
        // Additive accessor (no layout change) so HUDMessageLogic's online stunt-run time
        // generator can poll IsComboInProgress() on the online scorer by name -- the X360 reads
        // ss+0x2620 directly (BrnHUDMessageLogic.cpp / 0x82394B88).
        StuntModeScoring*           GetOnlineStuntScorer()   { return &mOnlineStuntModeScoring; }
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

        // ADDITIVE GROW (declare-only) for the BrnGameState::DeveloperChallengeManager TU.
        // OnTakedown's "took down every rival" sweep compares the taken-down count against the number
        // of cars in the scoring system minus one (X360 reads the car-count word at ss+20200). Body +
        // the real count member land with the ScoringSystem TU. FLAG: offset 20200, count semantics.
        s32 GetCarCount() const;

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

        // The embedded online stunt-run sub-scorer (X360 ss+0x4D44). Callers that need the team
        // standings reach it inline (e.g. OnlineStuntRunMode::ShouldFinish @0x8233A3F0 calls
        // GetWinnerTea on `a2+0x4D44`, and WriteDataToOutput calls GetTeamScore there). De-inlined to
        // this named address-of accessor over the embedded member, mirroring GetScoreData()'s pattern.
        const OnlineStuntRunModeScoring* GetOnlineStuntRunScorer() const { return &mOnlineStuntRunModeScoring; }

        // ===== burnout-skillz tally (declare-only) =====
        void AddPlayerBurnoutSkillz(BrnNetwork::NetworkPlayerID lID, BrnNetwork::NetworkPlayerID lOtherID); // :1141 / 0x823561D0
        // by-active-race-car-index entry 0x8231E408: translates the index to a key (per-car table
        // @ scoring+0x5044, stride 0x158) then forwards to the inner by-key lookup at 0x82311110.
        BurnoutSkillzData* GetBurnoutSkillzData(EActiveRaceCarIndex leRaceCarIndex);          // :1146 / 0x8231E408
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

        // [!!] MOVED TO public 2026-08-26 (wave-B fix round) -- ACCESS ONLY, declaration text
        // unchanged. The DWARF's `private:` placement cannot be right for this build: the X360
        // ModeManager::SendModeStopMessages @0x8234BEC0 calls it directly from OUTSIDE the class --
        // `addi r3, r28, 0xDB0` (== &mScoringSystem) / `mr r4, r29` / `bl ScoringSystem::ClearData`
        // at 0x8234C6B0 -- and a cross-class caller cannot reach a private member (measured:
        // C2248 when the parked call site is un-parked). Keeping it private is what forces that
        // call to stay parked, and while it is parked every event's scores/timers/finish flags leak
        // into the next event.
        // [x] LINK FRONTIER CLOSED 2026-08-26 (stuntrace waveB CLOSURE round). The note that used
        // to stand here -- "ClearData(bool) has NO body anywhere in src/" -- is REFUTED: the body
        // is BrnScoringSystem_Lookup.cpp:474 `void ScoringSystem::ClearData(bool lbResetCarData)`,
        // a full reconstruction of 0x8232A4A8 (ClearModeTimer / ClearTimeLimit, the six sub-scorer
        // resets, the FLT_MAX distance seeds). Both halves are therefore in the tree, and
        // BrnModeManager_Start.cpp:828 calls it un-parked -- that call site's own banner records
        // what was at stake (every event's scores/timers/finish flags leaking into the next).
        void ClearData(bool lbFull);                                                          // :1197 / 0x8232A4A8

        // [stuntrace waveB fix round, 2026-08-26] DECLARE-ONLY. maRaceCarPositioningData[8] is
        // private with no accessor, and two console legs reach it from ModeManager:
        // StartGameMode's 8-iteration `*(this + 0x677C + 24*i) = 0` loop and
        // SetUpCheckPointsForGameMode's identical sweep at 0x82329188-0x823291A4 -- both are
        // "every car is back at no-checkpoint/no-finish-position". ss+0x59C0 stride 0x18 is already
        // pinned in the tree (BrnCarData.cpp:33, BrnScoringSystem_UpdateA.cpp:367), so 0x59CC is the
        // fourth dword. hazards H9 forbids reaching it by raw offset from ModeManager, which is why
        // both legs are parked without this pair.
        RaceCarPositioningData*       GetRaceCarPositioningData(::EActiveRaceCarIndex leActiveRaceCarIndex);
        const RaceCarPositioningData* GetRaceCarPositioningData(::EActiveRaceCarIndex leActiveRaceCarIndex) const;

    private:

        // ===== data members (DWARF declared order + types) =====
        CgsSystem::Time mStartTime;                  // :1199
        CgsSystem::Time mEndTime;                    // :1200
        CgsSystem::Time mTotalTime;                  // :1201
        CgsSystem::Time mTimeRemaining;              // :1202

        CrashModeScoring    mCrashModeScoring;       // :1206 (by value)
        StuntModeScoring    mStuntModeScoring;       // :1209 (by value)  X360 ss+0x350 (offline stunt scorer)
        // X360-only SECOND StuntModeScoring at ss+0x2620 -- the ONLINE-path stunt scorer (the PS3
        // DecFIGS DWARF omits it; X360 carries extra online-stunt machinery). Pinned by:
        //   * Construct (0x82337FE0) refreshes its vtable right after mStuntModeScoring@0x350 and
        //     before the online scorers (`addi r3, r31, 0x2620` at 0x823380E0);
        //   * HasStuntAttackModeEnded (0x82326708): online branch (a4!=0) uses `ss+0x2620`, offline
        //     branch uses `ss+0x350`;
        //   * WriteDataToOutput (0x8232AE98): selects `v20 = ss+0x2620` (online) vs `ss+0x350`
        //     (offline) and reads its stunt-display fields.
        // Embedded by value -- a big slice (sizeof StuntModeScoring == 0x2620-0x350 == 0x22D0 on X360),
        // but ScoringSystem reaches it by NAME only (no byte-exact sizeof asserted on the keystone).
        StuntModeScoring    mOnlineStuntModeScoring; // X360 ss+0x2620 (online stunt scorer)
        RoadRageModeScoring mRoadRageModeScoring;    // :1212 (by value)
        s32  miMaximumPlayerCrashedNumber;           // :1213  X360 ss+0x4B58
        s32  miCurrentPlayerCrashedNumber;           // :1214  X360 ss+0x4B5C
        // [!!] ORDER CORRECTED 2026-08-26 (wave-B fix round). The DWARF declaration order put
        // mbPlayerTotalled first, which seats the bool at ss+0x4B60 and the array at 0x4B64..0x4B74.
        // The asm says the opposite, and it is not a judgement call -- the two claims are mutually
        // exclusive and both halves are attested:
        //   * mbPlayerTotalled is a BYTE at ss+0x4B70: `stb r30, 0x4B70(r31)` in
        //     ScoringSystem::Construct @0x823381BC and in OnModeStart @0x823382F8, `stb r29, 0x4B70`
        //     in OnRoadRagePlayerCrashed @0x823445AC, and it is read back as a byte by
        //     SurvivorMode::FillInGameModeSpecificResults @0x823163A8 (`lbz r11, 0x4B70(r4)`).
        //   * the medal scores are WORDS starting at ss+0x4B60: ModeManager::SetupGameMode
        //     @0x8234B158 writes the run in one go -- `stw r29, 0x4B60` / `stw r9, 0x4B64` /
        //     `stw r8, 0x4B68` @0x8234B5D0..0x8234B5D8.
        // The old order also happened to end at 0x4B74 (where mOnlineRaceModeScoring starts), so the
        // "the 28 bytes only close with this order" argument is NOT what settles it -- the byte
        // store at 0x4B70 and the word stores at 0x4B60/64/68 are.
        u32  mauiMedalScores[4];                     // :1216  X360 ss+0x4B60..0x4B6F
        bool mbPlayerTotalled;                       // :1215  X360 ss+0x4B70

        OnlineRaceModeScoring           mOnlineRaceModeScoring;        // :1219 (by value)  X360 ss+0x4B74
        OnlineRoadRageModeScoring       mOnlineRoadRageScoring;        // :1220 (by value)  X360 ss+0x4BF8
        OnlineBurningHomeRunModeScoring mOnlineBurningHomeRunScoring;  // :1221 (by value)  X360 ss+0x4CC0
        // X360-only FOURTH online sub-scorer at ss+0x4D44 (the PS3 DecFIGS DWARF omits it). Pinned by:
        //   * StartOnlineGameModeScoring (0x823126C8): the game-mode switch assigns
        //     mpCurrentOnlineModeScoring (ss+0x4DC8) to ss+0x4D44 for game-mode cases 12/14/17, the
        //     three online stunt-run modes (`addi r11, r3, 0x4D44; stw r11, 0x4DC8(r3)`);
        //   * WriteDataToOutput (0x8232AE98): calls OnlineStuntRunModeScoring::GetTeamScore on
        //     `ss+0x4D44` (`addi r3, r31, 0x4D44; bl ...OnlineStuntRunModeScoring__GetTeamScore`).
        // It is the LAST online scorer (next member mpCurrentOnlineModeScoring is at 0x4DC8). Embedded
        // by value -- it derives from BaseOnlineModeScoring and (like its siblings) adds no own data.
        OnlineStuntRunModeScoring       mOnlineStuntRunModeScoring;    // X360 ss+0x4D44 (online stunt-run scorer)

        BaseOnlineModeScoring* mpCurrentOnlineModeScoring;            // :1224  X360 ss+0x4DC8

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
        // NOTE: muCarsInCurrentMode IS the X360 ss+0x4EE8 member -- it is NOT a separate player
        // active-race-car-index field. Layout-anchored backward from the tail bools:
        //   ss+0x4EF8 mbNewLeader, +0x4EF9 mbNewLastPlace, +0x4EFA mbACarHasFinishedTheRace
        //   (ClearData 0x8232A4A8 stb's); +0x4EF0 meLeadRaceCarIndex (GetLead 0x82310DA0),
        //   +0x4EF4 meLastRaceCarIndex (GetLast 0x82356028); so +0x4EEC = muActualNumberOfCars...
        //   and +0x4EE8 = muCarsInCurrentMode. UpdateNumberOfCarsInMode (0x8231F3F0) STORES the
        //   active-car COUNT here (assert "Too many global race cars ... in the current mode", <=8);
        //   WriteDataToOutput (0x8232AE98) reads ss+0x4EE8 and passes it as the online Update
        //   virtual's `liNumberOfCars` 2nd arg -- a car count, not a player index. (The work-item
        //   instruction to add a NEW mePlayerActiveRaceCarIndex@0x4EE8 was a misread of this slot.)
        u32  muCarsInCurrentMode;                    // :1234  X360 ss+0x4EE8 (active-car count)
        u32  muActualNumberOfCarsInCurrentMode;      // :1235  X360 ss+0x4EEC
        EActiveRaceCarIndex meLeadRaceCarIndex;      // :1236  X360 ss+0x4EF0
        EActiveRaceCarIndex meLastRaceCarIndex;      // :1237  X360 ss+0x4EF4
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
