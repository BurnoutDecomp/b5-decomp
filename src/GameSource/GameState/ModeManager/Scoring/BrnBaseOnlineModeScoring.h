#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                  // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8)
#include "GameSource/GameState/BrnGameStateSharedIO.h"    // GameStateModuleIO::EPlayerTeam
#include "GameSource/GameState/BrnGameStateTypes.h"       // BrnGameState::EOnlineAwardID (maOnlineAwards[])
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h" // BrnNetwork::NetworkPlayerID (Compare* params)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"  // CgsSystem::Time (Compare* params)

namespace BrnGameState
{
// Forward-only: the scoring system is reached by the derived scorers only through the typed base
// methods declared below (Update/UpdatePlayerTeams take it by pointer), so an incomplete
// declaration suffices for this header. Full layout lands with BrnScoringSystem's own TU.
class ScoringSystem;

// The scorers' output record is the shared-IO GameStateModuleIO::OnlineScoringOutputInterface. The
// class spells it unqualified; a separate forward-declared BrnGameState::OnlineScoringOutputInterface
// made the derived WriteDataToOutput overloads instead of overrides (an eleventh vtable slot).
using GameStateModuleIO::OnlineScoringOutputInterface;

// Max players in a network game (== BrnWorld::KI_MAX_ACTIVE_RACE_CARS on this build). Modelled as a
// file-visible constant rather than pulling in the BrnWorld header for a single bound
// (BrnGameModeParams precedent).
const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

// Base scorer for the online game modes (OnlineRaceModeScoring, OnlineRoadRageModeScoring, ...). Root
// polymorphic type (vptr at offset 0, no base). Abstract: the reference build's own
// base vtable carries the pure-virtual handler in the eight lifecycle slots and real entries only for
// AwardNetworkRatings and GetCurrentPlayerTeam; the console never emits a base vtable at all. Byte
// offsets are NOT console-faithful on the x64 host (the vptr is 8 bytes); member ORDER + named access
// are preserved for semantic parity.
//
// Pure virtuals with a body: Prepare / Release / ClearData / Update / WriteDataToOutput are the shared
// halves the derived overrides call by qualified name. Construct / Destruct / UpdatePlayerPoints are
// pure with no body: no override calls them and the console image carries no copy.
//
// DWARF layout (BrnBaseOnlineModeScoring.h:46/193-199):
//   off 0x00  vptr
//   off 0x04  EOnlineAwardID  maOnlineAwards[8]
//   off 0x24  s32             maiOnlineAwardVariables[8]
//   off 0x44  EPlayerTeam     maePlayerTeams[8]      <-- GetCurrentPlayerTeam
//   off 0x64  s32             maiPlayerPositions[8]  <-- Get/SetPlayerPosition
class BaseOnlineModeScoring
{
public:
    // ---- polymorphic lifecycle (vtable slots 0..8; GetCurrentPlayerTeam below is slot 9) ----------
    // Declared in vtable order; every console derived vtable has exactly these ten slots. The
    // ScoringSystem dispatches through mpCurrentOnlineModeScoring into these: UpdateCumulativeResults
    // calls vtable+0x1C (== AwardNetworkRatings, slot 7) and UpdateNetworkPlayerResults drives
    // Update/UpdatePlayerPoints.
    virtual void Construct() = 0;                                    // slot 0
    virtual bool Prepare() = 0;                                      // slot 1  (body: shared half)
    virtual bool Release() = 0;                                      // slot 2  (body: shared half)
    virtual void Destruct() = 0;                                     // slot 3
    virtual void ClearData() = 0;                                    // slot 4  (body: shared half)
    virtual void Update(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars) = 0;       // slot 5  (body: shared half)
    virtual void UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumberOfCars) = 0; // slot 6
    // Slot 7, never overridden. luNumActiveRaceCars is unused: the body reads the scoring system's own
    // active-car count. Body in BrnBaseOnlineModeScoring_wN1_01.cpp.
    virtual void AwardNetworkRatings(const ScoringSystem* lpScoringSystem, u32 luNumActiveRaceCars);
    virtual void WriteDataToOutput(OnlineScoringOutputInterface* lpOutput) = 0;              // slot 8  (body: shared half)

    // X360 @ 0x823106F8 (BrnBaseOnlineModeScoring.h:341). Finishing position recorded for slot.
    s32 GetPlayerPosition(s32 liRaceCarIndex);

    // X360 @ 0x82314638 (BrnBaseOnlineModeScoring.cpp:1005). Team assigned to the slot. Virtual
    // (DWARF vtable slot 9, last). Derived online-mode scorers override the team-assignment policy.
    virtual GameStateModuleIO::EPlayerTeam GetCurrentPlayerTeam(s32 liRaceCarIndex);

    // X360 @ 0x823219B8 (BrnBaseOnlineModeScoring.cpp:205). Public non-virtual. Assigns/refreshes
    // the per-slot team table from the scoring system. The race-mode Update forwards to this.
    // Declared-only here (body belongs to the BrnBaseOnlineModeScoring TU).
    void UpdatePlayerTeams(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

protected:
    // X360 @ 0x82310770 (BrnBaseOnlineModeScoring.h:354). Store the finishing position for the slot.
    void SetPlayerPosition(s32 liRaceCarIndex, s32 liPlayerPosition);

    // Per-field comparison helpers the derived qsort comparators chain together. Each folds one
    // decisive comparison into the running *lpResult, but ONLY while *lpResult is still 0 (so the
    // first non-zero comparison in the chain wins). DWARF-attested arg/return signatures (protected,
    // void -- the IDA "returns a pointer" is the unused return-register artifact). Declared `static`:
    // the X360 bodies (verified) take their first Time/scalar in r3 and never touch `this`, so they
    // are callable from the derived classes' static qsort comparators (same deviation as those
    // comparators being static). Bodies belong to the BrnBaseOnlineModeScoring TU.
    static void CompareDistanceToFinish(f32 lfDistance1, f32 lfDistance2, s32* lpResult);                       // X360 0x823146B0 (.cpp:1023)
    static void CompareNetworkPlayerID(BrnNetwork::NetworkPlayerID lID1, BrnNetwork::NetworkPlayerID lID2,
                                       s32* lpResult);                                                          // X360 0x82314748 (.cpp:1058)
    static void CompareFinishTime(const CgsSystem::Time* lpTime1, const CgsSystem::Time* lpTime2, s32* lpResult); // X360 0x823147C8 (.cpp:1092)
    static void CompareCheckpointsReached(s32 liCheckpoints1, s32 liCheckpoints2, s32* lpResult);               // X360 0x82314928 (.cpp:1129)
    static void CompareTakedowns(s32 liTakedowns1, s32 liTakedowns2, s32* lpResult);                            // X360 0x823149A8 (.cpp:1163)
    static void CompareTimeAsRunner(CgsSystem::Time lTime1, CgsSystem::Time lTime2, s32* lpResult);             // X360 0x82314A28 (.cpp:1231) -- by value per DWARF

    // DWARF BrnBaseOnlineModeScoring.h:193 (this+0x04). Per-slot end-of-event award id. The online
    // scorers reset these to E_ONLINE_AWARD_INVALID in ClearData and copy them to the output. Protected
    // so derived scorers read them directly.
    EOnlineAwardID maOnlineAwards[KI_MAX_ACTIVE_RACE_CARS];

    // DWARF BrnBaseOnlineModeScoring.h:194 (this+0x24). Per-slot award parameter value.
    s32 maiOnlineAwardVariables[KI_MAX_ACTIVE_RACE_CARS];

    // DWARF BrnBaseOnlineModeScoring.h:195 (this+0x44). Read by GetCurrentPlayerTeam and by the
    // derived scorers' ClearData / WriteDataToOutput. Protected.
    GameStateModuleIO::EPlayerTeam maePlayerTeams[KI_MAX_ACTIVE_RACE_CARS];

private:
    // ---- end-of-event award rating (AwardNetworkRatings and its helpers) ---------------------------
    // One row per active race car, gathered from the scoring system then re-sorted once per award by
    // that award's rating comparator. 68-byte stride (qsort element size +0x44); pointer-free, so the
    // host layout matches the console one.
    struct NetworkAwardData
    {
        EActiveRaceCarIndex meRaceCarIndex;             // +0x00
        s32                 miRaceCarPosition;          // +0x04
        s32                 miTakedownsFor;             // +0x08
        s32                 miTakedownsAgainst;         // +0x0C
        s32                 miNumberOfCrashes;          // +0x10
        CgsSystem::Time     mFastestLap;                // +0x14
        f32                 mfDistanceDriven;           // +0x1C
        bool                mbFinishedRace;             // +0x20
        CgsSystem::Time     mTimeInLastPlace;           // +0x24
        CgsSystem::Time     mTimeInFirstPlace;          // +0x2C
        CgsSystem::Time     mTimeBoosting;              // +0x34
        f32                 mfLongestDrift;             // +0x3C
        s32                 miOverallStandingsPosition; // +0x40
    };
    static_assert(sizeof(NetworkAwardData) == 0x44, "NetworkAwardData is the 68-byte qsort row");

    // Indexed by EOnlineAwardID. The rating comparators order the rows best-first for that award; the
    // give-award tests then decide whether the leading row has earned it. maAwardPriorities is the
    // order the awards are tried in.
    static int (* const maAwardRatingFunctions[E_ONLINE_AWARD_COUNT])(const void* lpData1, const void* lpData2);
    static bool (* const maGiveAwardFunctions[E_ONLINE_AWARD_COUNT])(const NetworkAwardData* lpaNetworkAwardData,
                                                                     s32 liNumberOfRaceCars);
    static const EOnlineAwardID maAwardPriorities[E_ONLINE_AWARD_COUNT];

    s32 GetAwardParameter(const NetworkAwardData* lpAwardData, EOnlineAwardID leAwardID);

    static bool _GiveRaceWinnerAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveTakedownsForAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveTakedownsAgainstAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveMostCrashesAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveFastestLapAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveShortestDistanceAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveLongestDistanceAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveTimeInLastPlaceAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveTimeInFirstPlaceAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveMostTimeBoostingAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);
    static bool _GiveLongestDriftAward(const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars);

    static int _RaceWinnerCompare(const void* lpData1, const void* lpData2);
    static int _TakedownsForCompare(const void* lpData1, const void* lpData2);
    static int _TakedownsAgainstCompare(const void* lpData1, const void* lpData2);
    static int _MostCrashesCompare(const void* lpData1, const void* lpData2);
    static int _FastestLapCompare(const void* lpData1, const void* lpData2);
    static int _ShortestDistanceLapCompare(const void* lpData1, const void* lpData2);
    static int _LongestDistanceLapCompare(const void* lpData1, const void* lpData2);
    static int _TimeInFirstPlaceCompare(const void* lpData1, const void* lpData2);
    static int _TimeInLastPlaceCompare(const void* lpData1, const void* lpData2);
    static int _MostTimeBoostingAwardCompare(const void* lpData1, const void* lpData2);
    static int _LongestDriftCompare(const void* lpData1, const void* lpData2);

    // DWARF BrnBaseOnlineModeScoring.h:199 (this+0x64). Written by SetPlayerPosition, read by GetPlayerPosition.
    s32 maiPlayerPositions[KI_MAX_ACTIVE_RACE_CARS];
};
}
