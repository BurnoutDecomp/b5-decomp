#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                         // CgsID (CarSelect player-car-change grows)
#include "GameSource/BurnoutConstants.h"                            // EActiveRaceCarIndex
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"  // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<N,16> (output GUI event queue)
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CgsDev::Assert Begin/Fire/EndAssert
#include "GameSource/GameState/ModeManager/BrnModeManager.h"        // BrnGameState::ModeManager (mModeManager, by value)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"         // BrnNetwork::NetworkPlayerID (s32 typedef; GetActiveRaceCarIndex param)
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // GameStateModuleIO::GameActionQueue (real typedef)
#include "GameSource/GameState/BrnGameActions.h"             // GameStateModuleIO::CarSelectionChangedAction (by value, +0x38B80)
#include "GameSource/GameState/Progression/BrnProgressionManager.h" // BrnProgression::ProgressionManager (mProgressionManager, by value)
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // BrnGameState::TriggerQueryManager (mTriggerQueryManager, by value)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"         // CgsModule::EventReceiverQueue<3072,16> (mReceiverQueue)
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"      // BrnGameState::CarSelectManager (mCarSelectManager, by value)

// The module's cached read-only snapshot of the active race cars (mLastActiveRaceCarInterface,
// X360 this+0x397E0) is held BY VALUE exactly as the console holds it, so this is a full include
// rather than a forward declaration.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
// VehicleList is held by pointer (mpVehicleList, X360 this+0x456E8) -- forward declaration is
// enough here; the bodies that walk it include the owning header.
namespace BrnResource { struct VehicleList; struct VehicleListEntry; }
// The achievement manager the BurnoutSkillzManager grow returns (StuntModeScoring::AchievementManager
// is a typedef of this; pointer only here).
namespace BrnGameState { class AchievementManagerPS3; }
// For the DriveThruManager additive grow below (pointer-only).
namespace BrnProgression { class Profile; }
// The module's OutputBuffer is held by POINTER here so this header does not have to pull the
// whole (192 KB, ~40-member) BrnGameStateModuleIO.h; the owning .cpp includes it.
namespace BrnGameState  { namespace GameStateModuleIO { struct OutputBuffer; } }
// For the DeveloperChallengeManager additive grow below (pointer-only).
namespace BrnResource    { struct VehicleList; }
// For the StreetManager wave-C GetDeveloperChallengeManager grow below (pointer-only;
// the real class is BrnGameState::DeveloperChallengeManager, BrnDeveloperChallengeManager.h).
namespace BrnGameState   { class DeveloperChallengeManager; }
// For the ResetPlayerDebugComponent additive grows below (pointer-only).
namespace BrnResource    { class  WheelList; }
namespace BrnTrigger     { struct TriggerData; }
namespace CgsWorld       { struct WorldMap2D; }
// Prepare's 2nd/3rd arguments (pointer-only; the reconstructed stages do not dereference them).
namespace CgsModule      { struct IOBufferStack; }
namespace BrnResource    { namespace GameDataIO { class AllocatorList; } }

namespace BrnGameState
{
// Minimal slice (BrnPursuitMode.h GetName-only precedent): the full GameStateModule layout (~290KB,
// ~190 methods) is owned by the BrnGameStateModule.cpp TU. Only the members touched by the three
// reconstructed functions of this TU are declared here; exact member offsets are NOT modelled (the
// X360 attests mbIsUpdating at this+292289, mePlayerActiveRaceCarIndex at this+208304, mModeManager at
// the +0x1DB8 region) -- inter-member padding for the full class is out of scope. The base
// (CgsModule::ModuleSingleBuffered) is #included, not forked.
class GameStateModule : public CgsModule::ModuleSingleBuffered
{
public:
    // X360 @ 0x82311570 (BrnGameStateModule.h:949). Inline accessor for the player's active-race-car
    // slot index. Asserts the module is mid-update (mbIsUpdating) via CGS_ASSERT before handing back
    // the cached index.
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return mePlayerActiveRaceCarIndex;
    }

    // X360 @ 0x823116D0 (BrnGameStateModule.h:982). Out-of-line; defined in BrnGameStateModule.cpp.
    bool IsOnlineGameMode();

    // ------------------------------------------------------------------------
    // ⭐ THE MODULE'S OUTPUT BUFFER.
    //
    // On the console the GameState module is a CgsModule::ModuleSingleBuffered: its
    // GameStateModuleIO::OutputBuffer is the DataStructure the base allocates through
    // CreateOutputDataStructure() during Prepare(), and EVERY producer in the game-state
    // tree publishes through it -- CarSelectManager, DriveThruManager, StuntManager,
    // PaybackManager, the mode managers -- by posting onto its +0x04 game-action queue.
    // BrnGameModule::BridgeGameStateToDirector @0x823CD170 then bulk-Appends that queue
    // into the director input buffer's own <13312,16> queue once a frame. That Append is
    // the ONLY route by which a game action ever reaches MainDirector::ProcessInputQueue.
    //
    // ⚠️ FLAG (PC bring-up seam): nothing on PC calls this module's Prepare(), so the
    // console's allocation point never runs. Construct() below news the buffer instead and
    // Destruct() frees it. The buffer itself, its Construct and its accessors are the real
    // console ones; only the ALLOCATION SITE is moved. DELETE-WHEN the module's
    // Prepare()/CreateOutputDataStructure() path is reconstructed.
    GameStateModuleIO::OutputBuffer* GetOutputBuffer() { return mpOutputBuffer; }

    void Construct() override;
    void Destruct()  override;

    // ------------------------------------------------------------------------
    // ⭐ X360 0x8239E578 -- the module's FIRST-pass prepare (vtable +64). DWARF
    // BrnGameStateModule.h:510: `virtual bool Prepare(GameStateModuleIO::OutputBuffer*,
    // IOBufferStack*, const AllocatorList*)`. Its ONE caller is
    // BrnGameModule::GamePrepare @0x823EFBD0 stage 4, which pumps it until it returns true and
    // forwards the requests it stages onto the output buffer's resource-request interface into
    // the frame's GameData input (AppendRequestInterface<3072>).
    //
    // ⭐ THIS is where TRIGGERS.DAT is loaded (stage 3, E_PREPARESTAGE_LOAD_TRIGGER_DATA ->
    // TriggerQueryManager::Prepare @0x82398218). Nothing on PC ran it before 2026-08-01, which
    // is why `[WorldMap] LOADED -- traffic=1 trigger=0 ai=0` and why every consumer of
    // `mpTriggerQueryManager->GetTriggerData()` (the junkyard spawn-location walk above all)
    // was looking at a null resource pointer.
    //
    // ⚠️ SLICE: stage 3 is REAL; the other 24 stages log once and advance (each one names its
    // X360 call at the body). See the body for the full deferral list.
    //
    // NOTE: this hides the base `ModuleSingleBuffered::Prepare()` -- exactly as the console's
    // own override does. Stage 1 calls the base explicitly.
    bool Prepare(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                 CgsModule::IOBufferStack*        lpUpdateOutputBufferStack,
                 const BrnResource::GameDataIO::AllocatorList* lpAllocatorList);

    // The track's TriggerData / traffic-lane owner, and the spawn-location source the junkyard
    // car-select flow walks. DWARF BrnGameStateModule.h:201 (X360 this+42320).
    TriggerQueryManager*       GetTriggerQueryManager()       { return &mTriggerQueryManager; }
    const TriggerQueryManager* GetTriggerQueryManager() const { return &mTriggerQueryManager; }

    // ⭐ The junkyard car-select state machine (X360 this+183712 == 0x2CDA0). The console
    // NEVER names an accessor for it -- every one of the eight console call sites reaches the
    // embedded subobject by the inlined `this + 0x2CDA0` pointer adjust (GameStateModule::
    // Construct / Prepare / ProcessStreamingCompleteEvent / OnProfileLoaded /
    // SendSetupPlayerCarEvent / OnEnterOnline / ProcessGameEvents / PreWorldUpdate -- an
    // image-wide scan for that adjust returns exactly those eight). De-inlined to this named
    // accessor so no reconstructed body has to poke a byte offset; the member itself is the
    // console's, held BY VALUE exactly as the console holds it.
    CarSelectManager*       GetCarSelectManager()       { return &mCarSelectManager; }
    const CarSelectManager* GetCarSelectManager() const { return &mCarSelectManager; }

    // ⭐ X360 0x8236BAC8. The nearest junkyard's CgsID to lPosition -- the single input that turns
    // the loaded TriggerData into "which junkyard do I enter". Both start-of-game entries need it:
    // OnProfileLoaded @0x82397310 feeds it the Profile's saved position, SendSetupPlayerCarEvent
    // @0x8239A918 feeds it TriggerData::GetPlayerStartPosition().
    //
    // ⚠️⚠️ DROPPED-ARGUMENT TRAP (the SIXTH recorded incident, and the first VECTOR one). Hex-Rays
    // renders this `FindNearestJunkyardID()` -- ARITY ZERO. The asm opens `vmr128 v124, v1`, and
    // both call sites load v1 immediately before the branch (`lvx128 v1, r30, 48` in
    // OnProfileLoaded; `vmr128 v1, v127` in SendSetupPlayerCarEvent). It takes a Vector3 by value
    // in a VMX register. Recovered from the asm, not the prototype.
    CgsID FindNearestJunkyardID(Vector3 lPosition) const;

    // ⭐ X360 0x8239A918 -- SendSetupPlayerCarEvent. THE START-OF-GAME JUNKYARD ENTRY.
    // Cache the track's authored player-start pose, pick vehicle 0 and its default wheel set,
    // find the nearest junkyard, and hand all of it to CarSelectManager::EnterJunkyardAtStartOfGame
    // -- which posts the ResetPlayerCarAction that places the player's car at
    // maSpawnLocations[1]. Its two console call sites are ProcessGameEvents case 110 and
    // PreWorldUpdate @0x823A5328 (behind the one-shot latch below).
    void SendSetupPlayerCarEvent(GameStateModuleIO::GameActionQueue* lpActionQueue);

    // X360 0x82363450 -- the player-scoring slot currently mapped to leActiveRaceCarIndex.
    // Linear scan of the scoring module's eight per-player records (stride 344 bytes) for the one
    // whose active-race-car index matches; returns E_PLAYER_SCORING_INDEX_0 when none does (the
    // console's `result = 0` miss arm, NOT an invalid sentinel).
    GameStateModuleIO::EPlayerScoringIndex FindPlayerScoringIndexForActiveRaceCar(
            ::EActiveRaceCarIndex leActiveRaceCarIndex) const;

    // ⭐ X360 PreWorldUpdate @0x823A5328, the one-shot leg at 0x823A5510..0x823A5540:
    //     if (mbSendSetupPlayerCarPending) { SendSetupPlayerCarEvent(actionQueue);
    //                                       SendSetUpAllEventStartsMessage(out);
    //                                       mbSendSetupPlayerCarPending = false; }
    // Extracted here because the rest of PreWorldUpdate (1300 lines of dossier) is not
    // reconstructed. [FLAG PC bring-up] the EXTRACTION is the deviation -- the latch, its
    // one-shot semantics and the call it makes are the console's.
    // DELETE-WHEN PreWorldUpdate lands.
    void PreWorldUpdateSetupPlayerCarBringUp();

    // ---- bodies already reconstructed in BrnGameStateModule.cpp -------------
    // X360 @ 0x82311620. The player's GLOBAL race-car index (its slot in the full world
    // race-car table). Asserts mbIsUpdating.
    s32  GetPlayerGlobalRaceCarIndex();
    // X360 @ 0x82356870. Is the active race car in this slot currently crashing?
    bool IsRaceCarCrashing(::EActiveRaceCarIndex leRaceCarIndex);
    // X360 @ 0x823567A8. Is the current game mode a showtime mode (offline or online)?
    bool IsShowtimeGameMode();
    // X360 @ 0x82356978. Is the simulation currently paused (see the mask notes at the body)?
    bool IsSimPaused(bool lbCheckGameMode, bool lbStrictMask) const;

    // ADDITIVE GROW (declare-only) for the BrnChallengeManager wave-C TUs (NetworkPlayerRemoved
    // @0x8234E420 / SetRemotePlayersChallengeCompleted @0x82323DF8 call it through mpGameStateModule
    // @+0xE48). DWARF BrnGameStateModule.h:618 declares
    //   `EActiveRaceCarIndex GetActiveRaceCarIndex(RoadRulesRecvData::NetworkPlayerID)`;
    // the param type is spelled BrnNetwork::NetworkPlayerID here, matching the committed
    // BrnBurnoutSkillzManager.h precedent for the identical s32 typedef. Maps a network player id
    // to that player's active-race-car slot, or E_ACTIVE_RACE_CAR_INDEX_INVALID (-1) when the
    // player has no active car. The callee body is ledgered under the BrnScoringSystem.h TU
    // (X360 0x82363978); declare-only suffices for the per-TU `cl /c` gate.
    ::EActiveRaceCarIndex GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID lPlayerID);

    // The loaded vehicle list (X360 reads the VehicleList* at this+284392 == 0x456E8). BODIED --
    // OnSpecialEventPlayerCarChange / ApplyCarStats / GetOriginalCarId all resolve their vehicle
    // records through it.
    BrnResource::VehicleList* GetVehicleList();

    // ADDITIVE GROW (declare-only) for the StreetManager wave-C TU. StreetManager::
    // ProcessNewRoadScore (X360 0x823496C8) reaches the module's EMBEDDED DeveloperChallengeManager
    // subobject as the inlined `mpGameStateModule + 185712` (0x2D570) pointer adjust (asserted as
    // "mpGameStateModule->GetDeveloperChallengeManager()" at BrnGameStateStreetManager.cpp:2615,
    // then DeveloperChallengeManager::OnSetRoadRule is called on it). De-inlined to this named
    // accessor; body + the real embedded-manager member land with the GameStateModule TU.
    // FLAG: declare-only additive grow on the minimal GameStateModule slice.
    DeveloperChallengeManager* GetDeveloperChallengeManager();

    // ADDITIVE GROW (declare-only) for the BrnPaybackManager TU. X360 @ 0x823566F8. Hands back the
    // module's per-frame output GUI event queue (a CgsModule::VariableEventQueue<18432,16>) the
    // PaybackManager publishes its countdown / state-change HUD events onto via AddEvent(&ev,type,
    // size). Body + the real embedded-queue member land with the GameStateModule TU; declare-only
    // suffices for the per-TU `cl /c` gate.
    CgsModule::VariableEventQueue<18432, 16>* GetOutputGuiEventQueue();

    // ADDITIVE GROW (declare-only) for the BrnPaybackManager TU. X360 PaybackManager::
    // HandleHavingPayback resolves the active race car for a slot index from the module's active-car
    // table (this+7632) and treats a null entry OR a dead car-id (@+328 == -1) as "the car has left
    // the game". De-inlined to this named predicate (true == the slot still holds a live race car);
    // body + the real table walk land with the GameStateModule TU.
    bool IsActiveRaceCarStillPresent(::EActiveRaceCarIndex leActiveRaceCarIndex) const;

    // ADDITIVE GROW (declare-only) for the BrnMugshotManager TU. The X360 build inlines the access
    // to the owning module's embedded ModeManager (MugshotManager reads through *(this+0x1DB8) to
    // test the current mode's post-event state via ModeManager::IsInPostEvent). De-inlined to this
    // named accessor; body + the real embedded-ModeManager wiring land with the GameStateModule TU.
    // FLAG: declare-only additive grow on the minimal GameStateModule slice.
    ModeManager* GetModeManager();

    // ADDITIVE GROW (declare-only) for the AchievementManagerBase TU. FLAG: the cached
    // current-game-mode-type scalar AchievementManagerBase::OnTakedown (X360 0x8235AAE0)
    // reads through mpGameStateModule (raw read at this+7604, just below mModeManager) and
    // compares == E_MODE_ROAD_RAGE. Body lands with the GameStateModule TU; declare-only
    // suffices for the compile gate.
    GameStateModuleIO::EGameModeType GetCurrentGameModeType() const;

    // DWARF BrnGameStateModule.h:1300/651. X360 inlines it (sets mbToggleShowtimeBehaviour=true at
    // offset 284512); declared-only here, used by GameStateDebugComponent::ToggleShowtimeCallback.
    void ToggleShowtimeBehaviour();

    // ADDITIVE GROW (declare-only) for the BrnBurnoutSkillzManager TU. FLAG: the X360
    // BurnoutSkillzManager::Construct (0x82332688) reaches the embedded achievement manager
    // through the owning GameStateModule (the inlined `*(modeManager->mpGameStateModule) +
    // 181680` pointer adjust to the achievement-manager subobject). De-inlined to this named
    // accessor; body + the real embedded-AchievementManager wiring land with the GameStateModule
    // TU. Returns the StuntModeScoring::AchievementManager (== AchievementManagerPS3) the manager
    // caches as mpAchievementManager.
    AchievementManagerPS3* GetAchievementManager();

    // ADDITIVE GROW (declare-only) for the BrnTrainingManager TU.
    // X360 BrnGameState::GameStateModule::RequestPause -- TrainingManager::TriggerAnyFollowOnTrainingTips
    // (0x823889C8) calls it as RequestPause(this, 64, lpGameActionQueue, 0, 0) to pause the world while a
    // training voiceover plays. The leading s32 is the pause-reason bitflag (the X360 immediate 0x40 ==
    // "training" reason); the GameActionQueue* is the OutputBuffer's action queue the pause request is
    // broadcast through; the trailing two s32s are 0 at this call site. Body + the real pause-stack
    // wiring land with the GameStateModule TU. FLAG: declare-only additive grow on the minimal slice.
    void RequestPause(s32 liPauseReasonFlags,
                      GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                      s32 liArg3, s32 liArg4);

    // ⭐ CORRECTED 2026-08-01 (was: "the exact member name is unconfirmed"). The s32 at
    // this+232288 (0x38B60) that TrainingManager::RequestTraining @0x82365B20,
    // TriggerAnyFollowOnTrainingTips @0x823889C8 and CarSelectManager::Update @0x8239C218 all
    // nonzero-test is **miSimPauseFlags** -- the pause-reason bitfield. PROVEN two ways:
    //   IsSimPaused    @0x82356978 reads `*(this + 232288)` and returns `!= 0` (masked when online);
    //   RequestUnpause @0x82382138 does `*(this + 232288) &= ~leUnpauseModule`.
    // So this predicate is EXACTLY `IsSimPaused(false, false)` -- "the simulation is already paused
    // for some reason", which is why a training tip does not request its own pause and why the
    // car-select tick short-circuits. Kept as its own named predicate because the three call sites
    // read the raw word inline rather than calling IsSimPaused; the name now says what it reads.
    bool IsTrainingPauseSuppressed() const;

    // X360 GetLastActiveRaceCarInterface -- the read-only snapshot of the player's + rivals' active
    // race cars the GameStateModule cached at the end of the last world update (the X360 reaches it
    // as the interface EMBEDDED BY VALUE at this+0x397E0; it is held by value here too).
    // ⚠️ FLAG: nothing on PC copies the world's published interface into it yet, so it reads as the
    // Clear()ed state -- which is the console's own "no valid last player car" answer, and is what
    // CarSelectManager::SetupSpawnLocations' two geometry helpers already assume.
    // DELETE-WHEN GameStateModule::Update's world-snapshot leg lands.
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        GetLastActiveRaceCarInterface() const;

    // ADDITIVE GROW (declare-only) for the BrnTrainingManager TU. FLAG: the X360 reads an f32 at
    // this+42300 (0xA53C) in TrainingManager::RequestTraining / IsTipAllowedInGameMode and compares it
    // against fixed thresholds (5.0 == KF_MAX_TIMED_PLAYED_FOR_NOOB_TRAINING_TIPS, 30.0 for the
    // DISCOVERS_EVENT / boost gates). Semantically the elapsed time the player has spent in the current
    // timed game mode. Body + real member land with the GameStateModule TU.
    f32 GetTimePlayedInTimedMode() const;

    // ADDITIVE GROW (declare-only) for the BrnDriveThruManager TU (X360 0x82382460).
    // UnlockCarChallengeForCar calls it after flipping a newly-found event so the module can check
    // whether ALL events are now found (and fire the matching unlock/achievement). Declare-only.
    // NOTE: globally-qualified ::InputBuffer to avoid the BrnGameState::InputBuffer nested forward
    // decl (BrnScoringSystem.h) shadowing the global queue type when this header is parsed inside
    // namespace BrnGameState.
    void CheckForAllEventsBeingFound(BrnProgression::Profile* lpProfile,
                                     GameStateModuleIO::GameActionQueue* lpQueue);

    // ------------------------------------------------------------------------
    // The BrnGameState::CarSelectManager (junkyard car-select) hooks. The junkyard FSM routes the
    // player-car snapshot / streaming / swap-broadcast / unpause hooks through the owning
    // GameStateModule. Every signature below was RE-RECOVERED FROM THE X360 ASM this wave (the
    // register-pair rendering in the Hex-Rays prototypes drops arguments -- see the bodies).
    // ------------------------------------------------------------------------

    // X360 read at GameStateModule+0x456D8 -- the active player car's CgsID, compared against the
    // desired/current car id to detect a swap completing. Written by OnSpecialEventPlayerCarChange.
    CgsID GetActivePlayerCarId() const;

    // X360 0x82382550. Kick the world to start streaming the vehicle-selection carousel around
    // lCarId (posts the 88-byte "cars to stream" action 69).
    void RequestStreamingForVehicleSelection(CgsID lCarId);

    // X360 0x8238FB40. Broadcast a special-event player-car change (the unlock-display / change-car
    // / start-of-game path uses it). ARG SHAPE RECOVERED FROM ASM: r3=this, r4=car id, r5=wheel id,
    // r6=queue, r7=the trailing bool (forwarded to ProgressionManager::OnPlayerCarChange).
    void OnSpecialEventPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                                       GameStateModuleIO::GameActionQueue* lpQueue, bool lbUpdateProfile);

    // X360 0x82396B88. Broadcast a (non-special-event) player-car change on junkyard exit (offline).
    // Same arg shape; it forwards straight to OnSpecialEventPlayerCarChange and then publishes the
    // new car's opponent set.
    void OnPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                           GameStateModuleIO::GameActionQueue* lpQueue, bool lbUpdateProfile);

    // X360 0x82363698. Mark car lCarId as already-shown in the unlock sequence (so it is not shown again).
    void SetCarUnlockAlreadyShown(CgsID lCarId);

    // X360 0x82382010 / 0x82382138. Request / release a pause on the simulation.
    // ⚠️ SIGNATURE CORRECTION (2026-08-01): the committed declaration had `RequestUnpause(bool, ...)`.
    // The X360 second argument is NOT a bool -- it is the pause-reason BITFLAG the call clears:
    //   0x82382138: `CGS_ASSERT(leUnpauseModule != E_PAUSE_NONE)` then `miSimPauseFlags &= ~a2`.
    // CarSelectManager::UpdateExitState @0x82398C20 passes `li r4, 1` (bit 0, the car-select reason);
    // TrainingManager::TriggerAnyFollowOnTrainingTips passes 0x40 to RequestPause. The old
    // `RequestUnpause(true, q)` happened to produce the same bit only by accident.
    void RequestUnpause(s32 leUnpauseModule, GameStateModuleIO::GameActionQueue* lpQueue);

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnGameState::ResetPlayerDebugComponent TU. The
    // "Reset Player Car" debug menu reads the loaded vehicle/wheel resources, the active track's
    // trigger data and the district map off the owning GameStateModule, and publishes its
    // teleport / change-car requests onto the module's output GUI event queue. Signatures +
    // semantics are X360-asm-attested (offsets noted); bodies land with the GameStateModule TU.
    // Declare-only suffices for the per-TU `cl /c` gate.
    // ------------------------------------------------------------------------

    // X360 read at GameStateModule+0x456EC (284396). The loaded wheel-list resource (the runtime
    // aggregation of wheel records the change-wheel menu enumerates).
    BrnResource::WheelList* GetWheelList();

    // X360 read at GameStateModule+0xAB70 (43888). The currently-loaded track's trigger data (the
    // landmark / generic-region table the teleport-location menu is built from). The X360 reaches
    // it through the embedded resource pointer at this offset; de-inlined to a named accessor.
    BrnTrigger::TriggerData* GetTrackTriggerData();

    // X360 read at GameStateModule+0x3C090 (245904). The world district map (sampled to label each
    // teleport region with its county/district name).
    CgsWorld::WorldMap2D* GetDistrictMap();

    // The output GUI event queue the debug menu's teleport/change-car requests are published onto
    // (X360 reaches it at GameStateModule+0x3CA40 / 248384 -- the same per-frame output queue
    // GetOutputGuiEventQueue() above returns; exposed here for the debug component's AddEvent calls).

    // X360 read at GameStateModule+0x456E0 (284384). The player's currently-equipped wheel id; the
    // change-wheel menu pre-selects the option whose wheel-record id matches it. (GetActivePlayerCarId()
    // above exposes the sibling active-car id at +0x456D8.)
    CgsID GetActivePlayerWheelId() const;

    // X360 read at GameStateModule+0x1DB8 (7608) -- the leading flag of the embedded mModeManager.
    // The change-car debug action gates on it: it only publishes a change-car request when this is
    // false (the X360 `if ( !*(mpGameStateModule + 7608) ) { AddEvent(...); }`). Semantically "a mode
    // change / mode-data load is in progress", so a live car swap is deferred while it is true. FLAG:
    // the exact member name is unconfirmed (it is the first byte of the not-yet-named mModeManager
    // prefix); the predicate name reflects the asm-attested gate semantics. Body lands with the
    // GameStateModule TU.
    bool IsModeChangeInProgress() const;

private:
    // ------------------------------------------------------------------------
    // Private helpers the CarSelect hooks above call. Both are real X360 symbols with exactly one
    // caller each inside this class, so they stay private here.
    // ------------------------------------------------------------------------

    // X360 0x82381188. Publish the newly-selected car's handling/boost stats (the 24-byte action
    // 198) from its VehicleListEntry. Sole caller: OnSpecialEventPlayerCarChange.
    void  ApplyCarStats(CgsID lCarId, GameStateModuleIO::GameActionQueue* lpQueue);

    // The shared body of Prepare's "receive a resident data list" stages (vehicle reply 52,
    // wheel reply 59). The console writes each one out longhand; they differ only in the reply
    // id and their two baked assert lines. Returns false while the reply has not arrived.
    bool ReceiveListResource(s32 liExpectedReplyId, s32 liAssertLineType,
                             s32 liAssertLineEventId, void** lppOutResource);

    // X360 0x823758E8. Walk lCarId up its VehicleListEntry parent chain (at most two levels, as the
    // console does) to the base/"original" car a livery variant derives from. Used by
    // OnPlayerCarChange to look up the opponent set, and by ModeManager::SetupOpponentData /
    // StartModeAtLights (not reconstructed) on the console.
    CgsID GetOriginalCarId(CgsID lCarId);

    // DWARF BrnGameStateModule.h:771. The by-value ModeManager that owns the current game mode.
    ModeManager         mModeManager;
    // DWARF BrnGameStateModule.h:794 (X360 this+208304).
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;
    // DWARF BrnGameStateModule.h:882 (X360 this+292289) -- set true only while the module is updating.
    bool                mbIsUpdating;

    // ------------------------------------------------------------------------
    // Members the ALREADY-RECONSTRUCTED bodies in BrnGameStateModule.cpp bind to.
    //
    // ⚠️⚠️ THOSE BODIES DID NOT COMPILE. BrnGameStateModule.cpp carries seven finished
    // X360 reconstructions (IsOnlineGameMode, GetPlayerGlobalRaceCarIndex, IsRaceCarCrashing,
    // IsShowtimeGameMode, IsSimPaused, GetOutputGuiEventQueue, SetCarUnlockAlreadyShown) that
    // were written against a RICHER version of this header; the header was later reduced to
    // the "minimal slice" and the TU was left unmounted, so nothing ever noticed that
    // `cl /c` on it fails with 15 errors. Declaring the members it uses is what makes those
    // bodies real code again. (Same defect family as a silent-drop stub: finished work that
    // the link never reaches.)
    // ------------------------------------------------------------------------

    // X360 read in GetPlayerGlobalRaceCarIndex @0x82311620 -- the player's slot in the FULL
    // world race-car table (distinct from mePlayerActiveRaceCarIndex, the active-car slot).
    s32                 miPlayerGlobalRaceCarIndex;
    // X360 read in IsRaceCarCrashing @0x82356870 -- per-active-slot "this car is crashing" cache.
    bool                maRaceCarCrashing[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    // X360 this+232288 (0x38B60) -- bitfield of active pause reasons (0 == running). Read by
    // IsSimPaused @0x82356978 and IsTrainingPauseSuppressed; cleared by RequestUnpause @0x82382138.
    s32                 miSimPauseFlags;

    // ---- the CarSelect / player-car block (X360 this+0x456D8 .. +0x456E8, contiguous there) ----
    // X360 +0x456D8 (284376). The active player car's CgsID. WRITTEN by OnSpecialEventPlayerCarChange
    // (`stdx r4, this, 0x456D8`), read by CarSelectManager at four sites.
    CgsID               mActivePlayerCarId;
    // X360 +0x456E0 (284384). The active player car's wheel-set CgsID (`stdx r5, this, 0x456E0`).
    CgsID               mActivePlayerWheelId;
    // X360 +0x456E8 (284392). The loaded vehicle list (`lwzx r29, this, 0x456E8`).
    // ✅ THE FLAG IS RETIRED (2026-08-01): Prepare's stage 7/8 (E_PREPARESTAGE_REQUEST/RECEIVE_
    // VEHICLE_LIST) is real now and installs it from the GameData reply, exactly as the console
    // does. (What is still deferred at that stage is the pair of ApplyVehicleList republish
    // hooks, not the pointer.)
    BrnResource::VehicleList* mpVehicleList = 0;
    // X360 +0x456EC (284396). The loaded wheel list -- Prepare's stage 9/10, same shape,
    // reply id 59. GetWheelList() hands it back.
    BrnResource::WheelList*   mpWheelList = 0;

    // X360 +0x32DC4 (208324). The one-shot latch PreWorldUpdate @0x823A5510 tests before running
    // SendSetupPlayerCarEvent + SendSetUpAllEventStartsMessage, and clears immediately after
    // (`stb r17, 0(r28)` with r17 == 0). The console ARMS it from an event handler this slice does
    // not reconstruct; on PC it is armed at the end of Prepare's terminal stage, which is the first
    // moment its three data preconditions (vehicle list, wheel list, TriggerData) are all satisfied.
    bool mbSendSetupPlayerCarPending = false;

    // X360 +0x38B80 (232320). The CarSelectionChangedAction SendSetupPlayerCarEvent hands
    // EnterJunkyardAtStartOfGame as its sixth argument (the console passes `this + 0x38B80`); the
    // junkyard's id + spawn transform + "pos is left" bit are written into it and read back later
    // by the car-select flow. Held by value exactly as the console holds it.
    GameStateModuleIO::CarSelectionChangedAction mCarSelectionChangedAction;

    // X360 +0x397E0. The read-only active-race-car snapshot the module caches at the end of the last
    // world update, held BY VALUE as the console holds it (see GetLastActiveRaceCarInterface).
    BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface mLastActiveRaceCarInterface;
    // X360 @0x823566F8 hands this back (GetOutputGuiEventQueue); the PaybackManager and the
    // other managers publish their HUD/GUI events onto it.
    CgsModule::VariableEventQueue<18432, 16> mOutputGuiEventQueue;
    // X360 SetCarUnlockAlreadyShown @0x82363698 resolves the player Profile through it.
    BrnProgression::ProgressionManager       mProgressionManager;

    // The module's own output buffer -- see GetOutputBuffer() above for the ownership FLAG.
    // Zero-initialised in-class: BrnGameModule embeds this module by value and does NOT list
    // it in its ctor init list, so without this the Construct() guard would read garbage.
    GameStateModuleIO::OutputBuffer*         mpOutputBuffer = 0;

    // ---- the Prepare slice's own members (X360 offsets from Prepare @0x8239E578) ----

    // DWARF BrnGameStateModule.h:696. The 27-stage first-pass prepare machine's stage set.
    // Enumerators + values are DWARF-authoritative.
    //
    // ⛔ CORRECTION 2026-08-01: the committed line "the X360 jump table has exactly these" is
    // WRONG -- MEASURED, the ARTIST jump table at 0x8239E630 has TWENTY-EIGHT cases (0..27), one
    // more than this DWARF enum. The extra one sits between RUMBLE_MANAGER and DONE:
    //     X360 case 26  zero-inits the embedded DeveloperChallengeManager (this+185712)
    //     X360 case 27  DriveThruManager::Prepare + the car-select list publish + the DONE tail
    // The DecFIGS DWARF is the PS3 INTERNAL build and has no DeveloperChallengeManager stage at
    // all -- straight version drift (references/DecFIGS/README.md's own warning). The names stay
    // DWARF-authoritative; what shifts is only which console case each does the work of, so
    // E_PREPARESTAGE_DONE below carries the X360's case-27 body (which is the terminal one, and
    // whose `stage = 1; stage2 = 0` tail this machine already reproduced).
    enum EPrepareStage
    {
        E_PREPARESTAGE_START                    = 0,
        E_PREPARESTAGE_MANAGER                  = 1,
        E_PREPARESTAGE_MODE_DATA_ACQUIRING      = 2,
        E_PREPARESTAGE_LOAD_TRIGGER_DATA        = 3,
        E_PREPARESTAGE_STUNT_MANAGER            = 4,
        E_PREPARESTAGE_REQUEST_CHALLENGE_LIST   = 5,
        E_PREPARESTAGE_RECEIVE_CHALLENGE_LIST   = 6,
        E_PREPARESTAGE_REQUEST_VEHICLE_LIST     = 7,
        E_PREPARESTAGE_RECEIVE_VEHICLE_LIST     = 8,
        E_PREPARESTAGE_REQUEST_WHEEL_LIST       = 9,
        E_PREPARESTAGE_RECEIVE_WHEEL_LIST       = 10,
        E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS = 11,
        E_PREPARESTAGE_RECEIVE_PLAYERCARCOLOURS = 12,
        E_PREPARESTAGE_MODEMANAGER              = 13,
        E_PREPARESTAGE_TAKEDOWNMANAGER          = 14,
        E_PREPARESTAGE_MUGSHOTMANAGER           = 15,
        E_PREPARESTAGE_PAYBACKMANAGER           = 16,
        E_PREPARESTAGE_INVITEMANAGER            = 17,
        E_PREPARESTAGE_FLYBYMANAGER             = 18,
        E_PREPARESTAGE_NETWORKROUNDMANAGER      = 19,
        E_PREPARESTAGE_PROGRESSION              = 20,
        E_PREPARESTAGE_RICH_PRESENCE            = 21,
        E_PREPARESTAGE_ACHIEVEMENT_MANAGER      = 22,
        E_PREPARESTAGE_STREET_MANAGER           = 23,
        E_PREPARESTAGE_IMAGE_MANAGER            = 24,
        E_PREPARESTAGE_RUMBLE_MANAGER           = 25,
        E_PREPARESTAGE_DONE                     = 26,
    };

    // DWARF BrnGameStateModule.h:180 (X360 this+552). ⚠️ This NAME-SHADOWS the private
    // `mePrepareStage` of CgsModule::ModuleSingleBuffered -- exactly as the console's two
    // classes do (they are different words at different offsets, +8 on the base vs +552 here).
    // The base's copy is private, so there is no ambiguity; nothing reads it through this class.
    EPrepareStage mePrepareStage = E_PREPARESTAGE_START;

    // DWARF BrnGameStateModule.h:201 (X360 this+42320). The track-trigger dispatcher that owns
    // the loaded TriggerData / traffic-lane resources. Held BY VALUE as the console holds it.
    TriggerQueryManager mTriggerQueryManager;

    // DWARF BrnGameStateModule.h:317 (X360 this+232384). The ONE reply queue every prepare
    // stage names as the reply target for its resource request.
    CgsModule::EventReceiverQueue<3072, 16> mReceiverQueue;
    bool                                    mbReceiverQueueConstructed = false;

    // ⭐ DWARF BrnGameStateModule.h:280 (X360 this+183712 == 0x2CDA0). The junkyard car-select
    // state machine, held BY VALUE as the console holds it. Constructed from this module's
    // Construct() with the console's own three arguments (&mTriggerQueryManager, this,
    // &mProgressionManager -- X360 0x82380388: `CarSelectManager::Construct(a1 + 183712,
    // a1 + 42320, a1, a1 + 47920)`), and handed the loaded vehicle/wheel lists by Prepare's
    // terminal stage. GameStateModule::Construct is the console's ONLY caller of
    // CarSelectManager::Construct.
    CarSelectManager                        mCarSelectManager;
};
}
