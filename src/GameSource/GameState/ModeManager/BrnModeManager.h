#pragma once

#include "types.hpp"

#include "BrnCommonTypes.h"                             // Vector3 (== rw::math::vpu::Vector3), GetCheckpointPosition return
#include "GameSource/GameState/BrnGameStateSharedIO.h" // GameStateModuleIO::EGameModeType, CarCheckpointData, KI_MAX_LANDMARKS_IN_MODE
#include "GameSource/GameState/BrnGameEvents.h"        // GameStateModuleIO::StartNetworkGameEvent (SetOnlineRaceCars signature)

// ADDITIVE GROW (the ModeManager TU): the core mode-management methods bodied by this TU embed the
// ScoringSystem by value and reach the per-car checkpoint trackers / the checkpoint TriggerData by
// name, so the full owning headers are pulled in here (the header is no longer pointer-only).
#include "GameSource/BurnoutConstants.h"                                // EGlobalRaceCarIndex/EActiveRaceCarIndex (the 35/8 car spaces)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // BrnGameState::ScoringSystem (embedded by value)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // BrnGameState::GameMode (current-mode accessors)
#include "SharedClasses/Trigger/BrnTriggerData.h"                       // BrnTrigger::TriggerData (GetCheckpointPosition landmark lookup)
#include "SharedClasses/Trigger/BrnLandmark.h"                          // BrnTrigger::Landmark / BoxRegion::GetPosition
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT (house assert macro)

namespace BrnProgression
{
// Forward decl: ModeManager::GetProgressionManager hands one out by pointer only (the
// offline modes walk it to resolve landmark AI-section indices). Real owning header is
// GameSource/GameState/Progression/BrnProgressionManager.h, reconstructed by its own TU.
class ProgressionManager;
}

namespace BrnTraffic
{
// Forward decl: ModeManager::GetTrafficData hands out the live TrafficData by pointer only.
// Real owning header is SharedClasses/Traffic/BrnTrafficDataResourceType.h (the callers that
// walk it #include it). A pointer-only use needs no layout here.
struct TrafficData;
}

namespace CgsNumeric
{
// Forward decl: ModeManager::SetupOnlineStartingGrid takes the per-event start-grid shuffle
// generator by pointer only. Real owning header is
// GameShared/GameClasses/Numeric/CgsRandom.h (the online-mode .cpp that calls it #includes it).
class Random;
}

namespace BrnGameState
{
// Forward decls: the accessors below hand out a GameMode / take a GameModeParams only by
// pointer, and each of those types' own headers forward-declares ModeManager, so pointer-only
// forward decls here keep the headers free of an include cycle.
class GameMode;
class GameModeParams;
class NetworkRoundManager;
// Forward decls for the by-pointer accessors added for the OnlineRaceMode TU. The owning ModeManager
// embeds the ScoringSystem by value and holds the GameStateModule by pointer; both accessors hand out
// a pointer, so a forward decl is enough here (the OnlineRaceMode .cpp #includes the real headers).
class ScoringSystem;
class GameStateModule;

// MERGED OWNING HEADER for the ModeManager of the game-mode hierarchy (consolidated from the
// CountdownState / IntroState / RaceMode worker contributions and the existing committed slice).
// This is the real home for BrnGameState::ModeManager; BrnGameMode.h forward-declares it and the
// game-mode / state .cpp files #include this header when they actually call into it.
//
// MINIMAL / BOUNDED reconstruction: only the methods the worked slices need are declared, gated
// on the X360 ledger / DWARF. The full ModeManager is large (~190 methods + dozens of by-value
// contained mode/state objects in the DecFIGS DWARF, BrnModeManager.h) and is reconstructed by
// its own TU; this header is then extended toward that shape. No member layout is reconstructed
// here because every caller only ever holds a ModeManager* and calls methods on it -- a
// pointer-only use needs no layout.
class ModeManager
{
public:
    // X360-attested (identity.json, 0x82329B68); PS3-only-absent from the DecFIGS DWARF (build
    // drift), so its shape is recovered from the X360 pseudocode: a non-static member whose body
    // returns the pointer from ScoringSystem::UpdateCumulativeResults; the only caller (SendEvent)
    // discards it, so it is declared `void` here to avoid forking the as-yet-unreconstructed
    // return type. Called by OnlineGameMode::SendEvent.
    void TellGuiToShowOnlineFinalStandings();

    // Pre-race countdown duration for the current mode. X360-attested (identity.json,
    // 0x82327D10), called by CountdownState::OnEnter. Body reads the current mode type and
    // indexes a per-mode time table (plus team-mode bonuses); reconstructed by the ModeManager
    // TU. Returns f32 (the body computes a double, every caller stores it single-precision).
    f32  GetCountdownTimeForMode() const;

    // --- Queries used by the nested GameMode states (IntroState) -------------------------
    // DWARF-attested public accessors, inlined in the X360 build (IntroState's pseudocode renders
    // them as inline field reads: meCurrentGameModeType, mpCurrentGameMode, mbModeDataIsLoading).
    // DEFINED INLINE, which is the faithful shape: the X360 emits NO symbol for either -- every
    // caller reads meCurrentGameModeType / mpCurrentGameMode directly. (IsWaitingForModeDataToLoad
    // stays declare-only; its mbModeDataIsLoading member is not modelled yet.)
    GameStateModuleIO::EGameModeType GetCurrentGameModeType() const { return meCurrentGameModeType; }
    const GameMode* GetCurrentGameMode() const                      { return mpCurrentGameMode; }
    bool            IsWaitingForModeDataToLoad() const;

    // ------------------------------------------------------------------------
    // ⭐ [tut-ticker] THE THREE MODE CLOCKS (2026-08-24). DWARF BrnModeManager.h:1025/1026/1027
    // (mfTimeInFreeBurn / mfTimeInMode / mfTimeInOnline); X360 ModeManager +0x951C/+0x9520/+0x9524
    // (== GameStateModule +42300/+42304/+42308, the raw reads TrainingManager::RequestTraining /
    // IsTipAllowedInGameMode / Update open-code). Identity PROVEN from the writers, not the DWARF
    // order alone: ModeManager::PreWorldUpdate @0x823537B8 accumulates +0x951C ONLY while no game
    // mode is running AND gsm->mCarSelectManager.mJunkyardId == 0 AND the TrainingManager's
    // picture-paradise byte is clear (else resets it) == "time spent free-burning"; +0x9520
    // accumulates whenever a mode IS running; +0x9524 accumulates only while the mode is online
    // (else reset). All three are zeroed by ModeManager::Construct @0x82340008, and the first two
    // by SetupGameMode @0x8234B158 / SendModeStopMessages @0x8234BEC0.
    //
    // ⛔ WHY THESE ARE NOT OPTIONAL FLAG-ZEROES: TrainingManager::Update's WAIT_FOR_MESSAGE arm
    // (state 4, the LEAVES_JUNKYARD tip) only advances to PENDING when GetTimeInFreeBurn() > 3.0
    // -- a permanently-zero stand-in silently kills the whole tutorial ticker (the placeholder-
    // identity-element trap: 0 is not this expression's identity).
    // ------------------------------------------------------------------------
    f32 GetTimeInFreeBurn() const { return mfTimeInFreeBurn; }   // DWARF :657
    f32 GetTimeInMode()     const { return mfTimeInMode; }       // DWARF :660

    // The extracted CLOCK leg of ModeManager::PreWorldUpdate @0x823537B8 (the rest of that
    // body -- scoring updates, network results, payback takedowns -- is not reconstructed).
    // [FLAG PC bring-up] the EXTRACTION is the deviation; the accumulate/reset conditions and
    // the "game timestep" source are the console's own (the console reads timerStatus base *
    // multiplier off the pre-world input buffer; the caller hands the same product in).
    void PreWorldUpdateClocksBringUp(f32 lfGameTimestep);

    // The extracted five inter-mode seed stores of ModeManager::Construct @0x82340008 (sole
    // caller: GameStateModule::Construct):
    //     *(+0xD94) = -1  (meCurrentGameModeType = E_MODE_NONE)
    //     *(+0xD98) = 0   (mpCurrentGameMode)
    //     *(+0x951C) = *(+0x9520) = *(+0x9524) = 0.0f   (the three clocks)
    // ⭐ THE FIRST STORE IS LOAD-BEARING AND WAS MISSING ON THIS BUILD: nothing ever wrote
    // meCurrentGameModeType, so between modes it read 0 == E_MODE_OFFLINE_RACE instead of the
    // console's -1 == E_MODE_NONE. Two measured consumers of that lie: TrainingManager::
    // IsTipAllowedInGameMode's mode switch (0 lands in the mode-running arm and rejects the
    // junkyard tips), and TranslateGameActionsToGuiEvents case 58 (mode 0 picks the BOOST-BAR
    // presentation, GUI event 218, where the console's free-roam -1 takes the unsigned-compare
    // default and posts the plain 217 -- see that arm's own banner).
    // Also stores the owning-module back-pointer (console `*(a1 + 27992) = a2`, the leading
    // store of the same Construct) -- the clock leg reads the junkyard/picture-paradise state
    // through it.
    void ConstructInterModeStateBringUp(GameStateModule* lpGameStateModule);

    // ADDITIVE GROW (declare-only) for the BrnMugshotManager TU. X360-inlined helper: true when the
    // current game mode is in one of its post-event states (the X360 reads the current mode pointer
    // at ModeManager+0x1DB8, then tests its state field == 3 / 5 / 4). MugshotManager gates camera/
    // mugshot capture on it (StartMugshotCapture / ProcessImageReceivedEvent). De-inlined to this
    // named accessor; body + the real mode-state wiring land with the ModeManager TU. FLAG: additive.
    bool IsInPostEvent() const;

    // The mode's live scoring system. X360-inlined: every game-mode body that needs it reaches the
    // ScoringSystem the ModeManager embeds by value (X360 ModeManager+0xDB0) directly, e.g.
    // OnlineRaceMode::PreWorldUpdate / GetOutroTimeout poke ScoringSystem methods off
    // *(mpModeManager+0xDB0). De-inlined to this named accessor (returns the address of the embedded
    // member). Body + real member land with the ModeManager TU; declared by-pointer here.
    ScoringSystem*       GetScoringSystem();
    const ScoringSystem* GetScoringSystem() const;

    // The owning GameStateModule (X360 mpGameStateModule @ ModeManager+0x6D58). X360-inlined with a
    // non-null assert ("mpGameStateModule", BrnModeManager.cpp:5700) at the call site, exactly the
    // shape used by OnlineRaceMode::GetOutroTimeout. De-inlined to this named accessor; body asserts
    // the pointer is set and returns it. Body + real member land with the ModeManager TU.
    GameStateModule* GetGameStateModule();

    // True when the current online mode (free-burn lobby or showtime) starts without a timed
    // intro -- the composite the X360 build inlines everywhere as
    //   (IsOnlineFreeBurnLobby() || IsShowtimeGameMode()) && <mbSplashFlag>
    // (ModeManager field at X360 +0x9508). IntroState::OnEnter uses it to collapse the intro
    // countdown to ~0. The underlying flag's exact member name is unconfirmed in this bounded
    // view; rename when ModeManager is fully reconstructed.
    bool IsOnlineModeWithInstantIntro() const;

    // X360: BrnGameState::ModeManager::SetStartingGrid (0x82328608), called by the offline modes'
    // Start() (e.g. RaceMode::Start) to place the cars on the grid. Declaration shape taken from
    // the DecFIGS DWARF (BrnModeManager.h:486): void SetStartingGrid(GameModeParams*, int32_t,
    // bool) const. The full body/layout is reconstructed by the ModeManager TU.
    void SetStartingGrid(GameModeParams* lpGameModeParams, s32 liCarCount, bool lbPushForwards) const;

    // The owning network round manager (DWARF BrnModeManager.h:482). The online modes' Start()
    // reach the cached StartNetworkGameEvent through it (NetworkRoundManager::GetNetworkGameEvent).
    // Returns by pointer; body + member land with the ModeManager TU.
    const NetworkRoundManager* GetNetworkRoundManager() const;

    // FLAG (declare-only): the live traffic-data resource for the current track. The X360 reaches
    // it through a streaming/world holder the ModeManager caches (the GetBestStartGridID chain reads
    // `(*(this+0x6D60)+0x640)->GetMemoryStructure()`), an indirection whose intermediate holder type
    // is NOT reconstructed in this tree. De-inlined to this named accessor so the online-mode bodies
    // that walk the traffic hulls (OnlineStuntRunMode::GetBestStartGridID) stay member-by-name; the
    // body + the real holder wiring land when that streaming object is reconstructed. DO NOT
    // fabricate the +0x6D60/+0x640 chain here.
    const BrnTraffic::TrafficData* GetTrafficData() const;

    // DWARF BrnModeManager.h:483. Places the online race cars on the grid from the start event.
    // Body + layout land with the ModeManager TU.
    void SetOnlineRaceCars(GameModeParams* lpGameModeParams,
                           const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent) const;

    // X360-attested via the online modes' Start() (e.g. OnlineStuntRunMode::Start @0x82339E70 calls
    // it as SetupOnlineStartingGrid(this, lpGameModeParams, miNumRaceCars, &mRandom, false)). Lays
    // out the online start grid, shuffling the slots with the supplied per-event random generator.
    // Body + layout land with the ModeManager TU. (CgsNumeric::Random by pointer; its owning header
    // is pulled in by the online-mode .cpp that calls this.)
    void SetupOnlineStartingGrid(GameModeParams* lpGameModeParams, s32 liNumRaceCars,
                                 CgsNumeric::Random* lpRandom, bool lbPushForwards) const;

    // X360: BrnGameState::ModeManager::GetCheckpointPosition (0x82327388); DWARF
    // BrnModeManager.h:663 -> `Vector3 GetCheckpointPosition(uint32_t) const`. Returns the
    // world position of the checkpoint with the given id (the body looks the id up through the
    // mode's checkpoint TriggerData; reconstructed by the ModeManager TU). Called by
    // ScoringSystem::UpdateRacePositions (0x8232A668) to compute each car's distance-to-finish:
    // in the X360 pseudocode the Vector3 is returned via the hidden sret pointer
    // (GetCheckpointPosition(&v69, this, *(v28+12))), matching the by-value DWARF return.
    Vector3 GetCheckpointPosition(u32 luCheckpointId) const;

    // The owning ProgressionManager. Inlined in the X360 build (OfflineGameMode::
    // SelectRandomDestinations reaches it via the magic-multiply ModeManager indirection); the
    // standalone accessor is declared here for the offline-mode callers. Body + real member land
    // with the ModeManager TU. NOTE: depends on BrnProgression::ProgressionManager whose owning
    // header is not yet reconstructed (forward-declared above, used by pointer only).
    BrnProgression::ProgressionManager* GetProgressionManager() const;

    // =====================================================================================
    // ADDITIVE GROW (the ModeManager TU): the 15 mode-management-core methods bodied by this
    // TU. Each is X360-attested (identity.json); the bodies live in BrnModeManager.cpp.
    // =====================================================================================

    // X360 0x82311410. True iff the current game mode is an online mode (reads the current mode's
    // cached online flag -- the de-inlined GameMode::IsOnline()). Returns false when there is no
    // current mode. Called by TriggerQueryManager::PostWorldUpdate.
    bool IsOnlineGameMode() const;

    // X360 0x82329890. The next (lowest-indexed) checkpoint the given car still has to reach, as a
    // LandmarkIndex (clamped to u8 by the X360). Delegates to the car's CarCheckpointData tracker.
    s32 GetNextLandmarkIndex(EGlobalRaceCarIndex leGlobalRaceCarIndex) const;

    // X360 0x8231E960. How many checkpoints the given car still has to reach (the popcount of its
    // "remaining" bit set). Asserts the car index is in [0, COUNT).
    u32 CountCheckpointsRemaining(EGlobalRaceCarIndex leGlobalRaceCarIndex) const;

    // X360 0x8231E800. Mark a checkpoint as reached by the given car (clears its remaining bit).
    // Asserts the checkpoint index is in [0, muNumLandmarks) and the car index is in range.
    void MarkCarHittingCheckpoint(u32 luCheckpointIndex, EGlobalRaceCarIndex leGlobalRaceCarIndex);

    // X360 0x8231E8D8. Re-arm the given car's checkpoint tracker for the next lap (re-mark all
    // muNumLandmarks checkpoints as remaining). Called by RaceCarFinishes.
    void ResetCheckpointDataForNextLap(EGlobalRaceCarIndex leGlobalRaceCarIndex);

    // X360 0x82329910. Process a landmark trigger for the given car: if luLandmarkId matches a mode
    // landmark and is the car's next-expected checkpoint, record the next landmark, mark the
    // checkpoint hit (and, for the online burning-home-run mode, bump the player's checkpoint
    // counters) and return true. Returns false otherwise. Called by RaceCarTriggersLandmark.
    bool HasRaceCarHitValidCheckpoint(s16 luLandmarkId, EGlobalRaceCarIndex leGlobalRaceCarIndex);

    // X360 0x823283E8. True iff the player has won: finishing position 1, or (offline-race only, when
    // mbWinIfSecond is set) position 2. Calls GetPlayersFinishPosition().
    bool HasPlayerWon();

    // X360 0x8231EB00 / 0x823120E8. Begin / end the stunt challenge: clear the online stunt scorer and
    // (Setup) activate it, then latch mbStuntChallengeActive. Reached through the ScoringSystem's
    // online stunt scorer.
    void SetupStuntChallenge();
    void EndStuntChallenge();

    // X360 0x82363540. Cache + forward a player's network stunt score: look up the player's CarData,
    // snapshot its prior online stunt score + active-car index into the network-stunt cache, then
    // forward to ScoringSystem::SetNetworkStuntScore.
    void SetNetworkStuntScore(BrnNetwork::NetworkPlayerID lNetworkPlayerID, s32 liScore);

    // ADDITIVE GROW (declare-only) for the BrnDriveThruManager TU.
    // HandleDriveThru's body-shop auto-repair path fires a mode-manager hook once the repair is
    // available (X360 the `(*(*(this+2348)+3480)->vtbl+0x64)()` indirect call). De-inlined to this
    // named method; body + the real dispatch land with the ModeManager TU. FLAG: the real name and
    // dispatch object are NOT in the exports -- named conservatively.
    void OnDriveThruRepairAvailable();

    // X360 0x82327388. World position of the checkpoint with the given mode-local id (looks the id up
    // through the mode's checkpoint TriggerData landmark table). Asserts luCheckpointId < muNumLandmarks.
    // Called by ScoringSystem::UpdateRacePositions. (Overload of the existing GetCheckpointPosition.)

    // X360 0x82337B70. Copy each active car's checkpoint-remaining bit set into the scoring output
    // interface, then delegate to ScoringSystem::WriteDataToOutput. Called by
    // GameStateModule::CopyScoringDataToOutput.
    void WriteDataToOutput(GameStateModuleIO::ScoringOutputInterface* lpOutput,
                           GameStateModuleIO::OnlineScoringOutputInterface* lpOnlineOutput,
                           bool lbOnline,
                           EActiveRaceCarIndex lePlayerRaceCarIndex);

    // X360 0x82327B98. Publish each active car's live distance-to-finish + the total race distance +
    // the active-car count into the supplied RaceCarRaceDistanceInterface. No-op when there is no
    // current mode. Called by GameStateModule::PreWorldUpdate. Reads the per-car distance from the
    // embedded ScoringSystem (GetCarData(i)->GetScoreData()->GetDistanceToFinishLive()).
    void FillInRaceDistanceInterface(GameStateModuleIO::RaceCarRaceDistanceInterface* lpRaceDistanceInterface);

    // X360 0x82343438. Pack the player's mode results (finish time, fastest lap, takedowns, distance,
    // eliminator, race position, eliminations + per-mode fields) into a results record and AddEvent it
    // onto the supplied per-frame output action queue. Called by FinishCurrentMode / UpdateCurrentMode.
    void SendModeResults(CgsModule::VariableEventQueue<13312, 16>* lpOutputQueue);

    // X360 0x82329B68. Tell the GUI to show the online final standings: refresh the cumulative results
    // through the ScoringSystem and latch mbOnlineFinalStandingsShown. Called by OnlineGameMode::SendEvent.
    // (Already declared above as void TellGuiToShowOnlineFinalStandings(); bodied by this TU.)

    // X360 0x823281E8. The player's finishing position (1-based). NOT one of this TU's 15; declared
    // here (additive) so HasPlayerWon can call it. Body lands with a sibling ModeManager partial.
    s32 GetPlayersFinishPosition();

    // ---- named accessors for the embedded sub-objects (de-inlined) -------------------------
    // GetScoringSystem() / GetGameStateModule() are declared above; bodied by this TU.
    const BrnTrigger::TriggerData* GetCheckpointTriggerData() const;   // resolves mpGameStateModule's checkpoint TriggerData resource

    // FLAG (declare-only, blocked on the GameStateModule minimal slice): the X360 reaches these three
    // through mpGameStateModule by raw offset (the global race-car output interface @+0x3C0D0 / the
    // active interface @+0x245968, the current online round @+0xBB24/+0xBB20). The minimal
    // GameStateModule slice (BrnGameStateModule.h) exposes no named accessor for them, and growing
    // GameStateModule is out of this TU's scope. De-inlined to these ModeManager helpers so the
    // bodied methods stay member-by-name; the helper bodies (which read through mpGameStateModule) land
    // when GameStateModule grows the matching accessors. DO NOT fabricate the offsets here.
    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*
        GetGlobalRaceCarOutputInterface() const;                       // X360 mpGameStateModule+0x3C0D0
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        GetActiveRaceCarOutputInterface() const;                       // X360 mpGameStateModule+0x245968
    s32 GetOnlineRoundIndex() const;                                   // X360 mpGameStateModule+0x32DAC
    s32 GetOnlineActiveCarCount() const;                               // X360 mpGameStateModule (+0xBB24)-(+0xBB20)

    // FLAG (declare-only): the X360 maps a global race-car index to its active slot through an
    // internal table on the active interface (`*(4*(global+525)+interface)`); the active interface
    // exposes only the active->global direction by name (GetGlobalRaceCarIndex). De-inlined here so
    // HasRaceCarHitValidCheckpoint stays member-by-name; body lands when the active interface grows the
    // inverse accessor. DO NOT fabricate the table walk.
    EActiveRaceCarIndex GlobalToActiveRaceCarIndex(EGlobalRaceCarIndex leGlobalRaceCarIndex) const;

    // Debug-tunable mode flags driven by the mode-manager debug menu (ModeManagerDebugComponent).
    // Declared as named members here (the real home) so the debug component accesses them by name
    // rather than by raw offset; the full ModeManager layout (the X360 places these deep in the
    // ~38KB object) is reconstructed by the ModeManager TU and these settle into their real slots.
    bool mbEndlessStuntRun;
    bool mbWinIfSecond;
    bool mbFinishCurrentEvent;
    s32  miFinishPosition;

private:
    // =====================================================================================
    // ADDITIVE GROW: named data members the bodied methods touch. RELATIVE ORDER + named
    // access for semantic parity -- NOT X360-faithful byte offsets (pointers are 8 bytes on the
    // PC gate, the ScoringSystem is embedded by value at a slice size, and the per-car checkpoint
    // array lives in the X360 deep in the ~38KB object). The X360 byte offsets each member stands
    // in for are documented inline. FLAG: provisional layout; settles when the full ModeManager
    // layout is reconstructed.
    // =====================================================================================

    GameStateModuleIO::EGameModeType meCurrentGameModeType;   // X360 +0xD94 -- the live mode type
    GameMode*                        mpCurrentGameMode;       // X360 +0xD98 -- current mode (null between modes)
    ScoringSystem                    mScoringSystem;          // X360 +0xDB0 -- embedded by value (online stunt scorer at +0x2620)
    GameStateModule*                 mpGameStateModule;       // X360 +0x6D58 -- owning module (asserted non-null)

    // The mode's checkpoint TriggerData (X360 reaches it through a CgsResourcePtr<TriggerData> nested
    // at mpGameStateModule's +0x6D60/+0x620 region; modelled here as the resolved main-memory resource
    // for named access). GetCheckpointPosition reads landmark positions through it.
    const BrnTrigger::TriggerData*   mpCheckpointTriggerData; // X360-resolved trigger data

    u32                              muNumLandmarks;          // X360 +0x801C -- checkpoint count for the current mode
    // Per-checkpoint mode-local landmark -> region-table index map (X360 u16[] at +0x7E20).
    u16                              maLandmarkRegionIndexes[GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE];
    // The next-expected landmark index recorded per car (X360 u8[] at +0x7FF8). One per global car.
    u8                               maNextLandmarkIndex[E_GLOBAL_RACE_CAR_INDEX_COUNT];

    // Per-car checkpoint trackers (X360 CarCheckpointData[35] at +0x7EE0). A set bit == checkpoint
    // not yet reached; indexed by EGlobalRaceCarIndex.
    GameStateModuleIO::CarCheckpointData maCarCheckpointData[E_GLOBAL_RACE_CAR_INDEX_COUNT];

    // Live race-distance bookkeeping (the source FillInRaceDistanceInterface publishes). The X360
    // reads the active-car count at +0x5C98 and the total race distance at +0x6A94 directly off the
    // ModeManager; modelled as named members here (provisional layout -- see the FLAG above).
    s32                              miNumActiveRaceCars;           // X360 +0x5C98 (active-car count)
    f32                              mfTotalRaceDistance;           // X360 +0x6A94 (total race distance)

    // Network stunt-score cache (X360 +0x6CD0/+0x6CD4/+0x6CD8 written by SetNetworkStuntScore).
    s32                              miNetworkStuntActiveCarIndex;  // X360 +0x6CD0 (CarData active-race-car index)
    s32                              miNetworkStuntScore;           // X360 +0x6CD4 (the new score)
    s32                              miNetworkStuntPreviousScore;   // X360 +0x6CD8 (the car's prior online stunt score)

    // ⭐ [tut-ticker] the three mode clocks -- see the accessor banner above for the full
    // identity proof. X360 +0x951C / +0x9520 / +0x9524.
    f32                              mfTimeInFreeBurn;              // DWARF h:1025 -- free-roam seconds (no mode, not junkyard, not picture-paradise)
    f32                              mfTimeInMode;                  // DWARF h:1026 -- seconds in the running game mode
    f32                              mfTimeInOnline;                // DWARF h:1027 -- seconds in an online mode (reset offline)

    // Latch flags (X360 single bytes deep in the object).
    bool                             mbOnlineFinalStandingsShown;   // X360 +0x94F8 (set by TellGuiToShowOnlineFinalStandings)
    bool                             mbStuntChallengeActive;        // X360 +0x950D (Setup=1 / End=0 StuntChallenge)
    bool                             mbResultsPositionValid;        // X360 +0x94F7 (SendModeResults: use the override position)
    bool                             mbResultsTeamWon;              // X360 +0x94FD (SendModeResults: per-mode flag byte)
    bool                             mbResultsEliminatorValid;      // X360 +0x9519/region byte read by SendModeResults
    s32                              miResultsOverridePosition;     // X360 +0x9518 (SendModeResults: explicit race position)
};
}
