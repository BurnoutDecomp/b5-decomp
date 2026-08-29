#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                         // CgsID (CarSelect player-car-change grows)
#include "GameSource/BurnoutConstants.h"                            // EActiveRaceCarIndex
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"  // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<N,16> (output GUI event queue)
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CgsDev::Assert Begin/Fire/EndAssert
#include "GameShared/GameClasses/Containers/CgsStack.h"             // CgsContainers::Stack<u16,8> (mShowtimePendingTrafficIndexStack, by value)
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h" // BrnPhysics::ContactSpy::ContactSpyInterface (ProcessContacts' one argument)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"        // BrnGameState::ModeManager (mModeManager, by value)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"         // BrnNetwork::NetworkPlayerID (s32 typedef; GetActiveRaceCarIndex param)
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // GameStateModuleIO::GameActionQueue (real typedef)
#include "GameSource/GameState/BrnGameActions.h"             // GameStateModuleIO::CarSelectionChangedAction (by value, +0x38B80)
#include "GameSource/GameState/Progression/BrnProgressionManager.h" // BrnProgression::ProgressionManager (mProgressionManager, by value)
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // BrnGameState::TriggerQueryManager (mTriggerQueryManager, by value)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"         // CgsModule::EventReceiverQueue<3072,16> (mReceiverQueue)
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"      // BrnGameState::CarSelectManager (mCarSelectManager, by value)
// [gateui] The two sub-objects the smash/billboard chain needs BY VALUE, both at their console
// positions (StuntManager this+183952, DeveloperChallengeManager this+185712). Neither header
// includes this one back, so there is no cycle (contrast mpTrainingManager below).
#include "GameSource/GameState/Offences/BrnStuntManager.h"           // BrnGameState::StuntManager (mStuntManager, by value)
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h" // DeveloperChallengeManager (by value)
// ---- Prepare2's two sub-objects, both held BY VALUE exactly as the console holds them ----
// X360 GameStateModule::Construct @0x82380388 constructs both in place:
//   AchievementManagerBase::Construct(a1 + 181680, a1 + 47920, a1 + 284520, a1 + 7632, a1)
//   StreetManager::Construct        (a1 + 284520, a1, a1 + 47920, a1 + 183592)
// and the module ctor @0x827E44B8 seeds the achievement manager's vptr in place
// (`*(a1 + 181680) = &off_820CE768`) -- an EMBEDDED subobject, not a pointer.
#include "GameSource/GameState/AchievementManager/X360/BrnGameStateAchievementManagerX360.h" // BrnGameState::AchievementManagerX360 (mAchievementManager, by value)
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"                       // BrnGameState::StreetManager (mStreetManager, by value)

// The module's cached read-only snapshot of the active race cars (mLastActiveRaceCarInterface,
// X360 this+0x397E0) is held BY VALUE exactly as the console holds it, so this is a full include
// rather than a forward declaration.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
// VehicleList is held by pointer (mpVehicleList, X360 this+0x456E8) -- forward declaration is
// enough here; the bodies that walk it include the owning header.
namespace BrnResource { struct VehicleList; struct VehicleListEntry; }
// (The achievement manager is no longer a forward-declared pointer: the module embeds the real
// AchievementManagerX360 subobject -- see the include above and mAchievementManager below.)
// For the DriveThruManager additive grow below (pointer-only).
namespace BrnProgression { class Profile; }
// The module's OutputBuffer is held by POINTER here so this header does not have to pull the
// whole (192 KB, ~40-member) BrnGameStateModuleIO.h; the owning .cpp includes it.
namespace BrnGameState  { namespace GameStateModuleIO { struct OutputBuffer; } }
// [D2 gesture-sink] The module's stand-in PreWorldInputBuffer is held by POINTER for the same
// reason mpOutputBuffer is -- so this header does not have to pull the whole BrnGameStateModuleIO.h.
// See GetPreWorldInputBuffer() below for the console attestation and the named PC deviation.
namespace BrnGameState  { namespace GameStateModuleIO { struct PreWorldInputBuffer; } }
// For the DeveloperChallengeManager additive grow below (pointer-only).
namespace BrnResource    { struct VehicleList; }
// For the StreetManager wave-C GetDeveloperChallengeManager grow below (pointer-only;
// the real class is BrnGameState::DeveloperChallengeManager, BrnDeveloperChallengeManager.h).
// (The DeveloperChallengeManager is no longer a forward-declared pointer: the module embeds the
// real subobject -- see the include above and mDeveloperChallengeManager below.)
// [gateui] TrainingManager stays a forward declaration ON PURPOSE -- BrnTrainingManager.h includes
// THIS header, so including it back is a cycle. See mpTrainingManager's FLAG at the member.
namespace BrnGameState   { class TrainingManager; }
// For the ResetPlayerDebugComponent additive grows below (pointer-only).
namespace BrnResource    { class  WheelList; }
namespace BrnTrigger     { struct TriggerData; }
namespace CgsWorld       { struct WorldMap2D; }
// Prepare's 2nd/3rd arguments (pointer-only; the reconstructed stages do not dereference them).
namespace CgsModule      { struct IOBufferStack; }
// [showtime S7b-b] ShouldStartShowtimeMode takes the output buffer's SIM timer request block by
// pointer only (DWARF BrnGameStateModule.h:781). Forward-declared rather than included to keep
// this very widely-included header off the timer chain; the .cpp includes the real header.
namespace CgsSystem      { struct TimerRequests; }
namespace BrnResource    { namespace GameDataIO { class AllocatorList; } }

namespace BrnGameState
{
class GameStateModule;
namespace GameStateModuleIO
{
    // [P1 sim-pause] X360 free function: returns the module's post-world input GameEventQueue
    // (VariableEventQueue<1536,16>) that BridgeGuiToGameState AddEvent's translated events
    // into. GameBridgeGUIToX.cpp already declares it (the FLAG'd un-homed collaborator); this
    // is now its single canonical declaration and the PC body (GameStateModule_gUI_00.cpp)
    // returns the CARRY QUEUE -- the named reduction: the console's PostWorldUpdate merges its
    // post-world input queue into the carry queue for the next PreWorldUpdate's ProcessGameEvents,
    // and on this build (no PostWorldInputBuffer exists) the carry queue IS that seam, with the
    // identical consume point (the pre-world pump) and lifetime (Cleared after the pump).
    CgsModule::VariableEventQueue<1536, 16>* PostWorldInput(GameStateModule* lpModule);
}

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

    // ------------------------------------------------------------------------
    // ⭐⭐ [D2 gesture-sink] THE PRE-WORLD CONTROLLER SINK -- the buffer whose ControllerInput
    // block carries the offline event-start gesture (accelerator + brake both past quarter
    // analogue travel, PreWorldInputBuffer +0x45 == ControllerInput::mbRaceModePressed).
    //
    // ⛔⛔ THE PREMISE THIS GROW WAS ASKED UNDER IS REFUTED, AND THE REFUTATION IS WHY THE
    // OWNERSHIP BELOW IS A NAMED DEVIATION RATHER THAN A RESTORATION. The tree (and the wave
    // brief) said "the module owns a PreWorldInputBuffer at gameStateModule + 0x2BE8, returned
    // by X360 sub_823B8EC0". MEASURED, sub_823B8EC0 is a method OF THE BUFFER, not of the
    // module: its body tests `(*a1 >> 3) & 1` -- the IOBuffer WRITE-LOCK bit at a1+0 -- fires
    // "Not locked for writing" at BrnGameStateModuleIO.h:144, and returns `a1 + 11240`. So
    // 0x2BE8 is an offset INSIDE PreWorldInputBuffer (its input BIND-result queue; the UNBIND
    // queue is the same accessor + 108, i.e. +0x2C54, and the resolved player-0 controller port
    // is stored at +0x2CC0 == that accessor + 216).
    //
    // ⭐ WHERE THE CONSOLE'S BUFFER ACTUALLY COMES FROM: BrnGameModule::DoUpdate_GameStatePreWorld
    // @0x823EE0E8 opens with
    //     CgsModule::IOBufferStack::CreateIOBuffer<GameStateModuleIO::PreWorldInputBuffer>(
    //         lpUpdateInputBufferStack, &lpPreWorldInput, "GameStatePreWorld");
    // fills it (BridgeNetworkToGameState, then BridgeControllerToGameState -- BOTH of which take
    // that BUFFER as their second argument, not the module), hands it to
    // `GameStateModule::PreWorldUpdate(this + 6722816, ..., lpPreWorldInput, ...)`, and
    // DestroyIOBuffer()s it at the tail. It is a PER-FRAME STACK BUFFER, owned by the frame.
    //
    // ⚠️ FLAG (PC bring-up seam) -- SAME CLASS OF DEVIATION AS mpOutputBuffer ABOVE, AND STATED
    // THE SAME WAY: nothing on PC runs DoUpdate_GameStatePreWorld, so the console's per-frame
    // CreateIOBuffer never happens and the buffer has no owner at all. Before this grow the only
    // "sink" in the tree was a FILE-STATIC throwaway inside GameBridgeControllerToX.cpp, so the
    // gesture byte was computed every frame and discarded -- no consumer could ever read it.
    // The module owns one instead: same buffer TYPE, same Construct, same accessors, same
    // write-lock contract; only the ALLOCATION SITE and the LIFETIME (per-frame -> per-module)
    // move. The single live producer (BridgeControllerToGameState) and the single live consumer
    // (ShouldStartSnapRaceMode) run in the same frame, so the longer lifetime is not observable.
    // DELETE-WHEN DoUpdate_GameStatePreWorld lands with a real IOBufferStack-staged buffer --
    // then this accessor, mpPreWorldInputBuffer, and their Construct/Destruct legs all retire
    // together and the bridges take the staged buffer directly, as the console's do.
    //
    // Returns 0 until Construct() has run.
    GameStateModuleIO::PreWorldInputBuffer* GetPreWorldInputBuffer() { return mpPreWorldInputBuffer; }
    const GameStateModuleIO::PreWorldInputBuffer* GetPreWorldInputBuffer() const { return mpPreWorldInputBuffer; }

    // ========================================================================================
    // ⭐⭐⭐ [stuntrace wave D, D3] THE FOUR OFFLINE EVENT-START FUNCTIONS.
    // Bodies in GameStateModule_gSR_00.cpp. Console call order, all inside
    // GameStateModule::PreWorldUpdate @0x823A5328:
    //
    //   CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418   -- "am I standing at an event?"
    //        traffic-light id -> JunctionLogicBox -> muEventJunctionID -> ProgressionData's
    //        EventJunction table -> RaceEventData, then POSTS GAME ACTION 201 (40 bytes,
    //        E_ACTION_EVENT_AT_JUNCTION_AVAILABLE). That action is THE VISIBLE ORACLE for this
    //        wave: the mounted GameBridgeGameStateToX_EventFlowGuiEvents.cpp arm turns it into
    //        GuiEventJunctionInfo (311) and the mounted JunctionInfoComponent draws the banner.
    //   DetectModeStarts                     @0x8239A428   -- the arm; calls the next two.
    //   ShouldStartSnapRaceMode              @0x82363700   -- the 0.35 s accel+brake hold gate.
    //   StartModeAtLights                    @0x82396CF8   -- builds StartGameModeParams and
    //        calls ModeManager::StartGameMode.
    //
    // ⓘ ARGUMENT SHAPES ARE ASM-RECOVERED, NOT TAKEN FROM THE HEX-RAYS PROTOTYPES (which drop
    // arguments on all four). Each body's banner cites the exact register loads.
    // ========================================================================================

    // ⭐ X360 0x82390418. The junction join + the action-201 post. Console arguments: r3 = this,
    // r4 = the pre-world input buffer (its ControllerInput's mbAcceleratePressed byte +0x12 is
    // read at 0x8239080C), r5 = the output buffer (spilled to arg_24 and passed to
    // OutputBuffer::GetGameActionQueue at every post site).
    void CheckIfPlayerIsAtJunctionWithAnEvent(
            const GameStateModuleIO::PreWorldInputBuffer* lpInput,
            GameStateModuleIO::OutputBuffer*              lpOutput);

    // ⭐ X360 0x8239A428. The start-detection arm. Console arguments: r3 = this, r4 = the
    // pre-world input buffer, r5 = the output buffer.
    // ⚠️ THE TIMESTEP IS A PARAMETER HERE AND IS NOT ON THE CONSOLE: 0x8239A518 loads it as
    // `lfs f1, 0(this + 292284)` -- the module's own cached game timestep, latched by
    // PreWorldUpdate @0x823A54D8 out of the PreWorldInputBuffer's TimerStatusInterface
    // (maEntries[0].mfValue04 * .mfValue08). That member is not modelled on this minimal slice,
    // and its producer (PreWorldUpdate's timer leg) is not reconstructed, so the value arrives
    // from the caller -- exactly as PreWorldUpdateStuntBringUp / PreWorldUpdateTrainingBringUp
    // above already take it. DELETE-WHEN the +292284 latch lands.
    void DetectModeStarts(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                          GameStateModuleIO::OutputBuffer*              lpOutput,
                          f32                                           lfGameTimestep);

    // ⭐ X360 0x82363700. The gesture gate. Console arguments, read off DetectModeStarts'
    // call site @0x8239A504..0x8239A51C: r3 = this, r4 = `lbz 0x45(lpInput)` (the buffer's
    // ControllerInput::mbRaceModePressed -- accelerator AND brake both past 0.25 analogue
    // travel, SetButtonPressed @0x823BA240), f1 = the game timestep, r6 = the out start
    // mechanism. r5 IS SKIPPED -- the PPC float argument consumes its GPR slot, which is why
    // the Hex-Rays prototype shows a phantom `int a4`.
    // Returns true exactly once, on the frame the 0.35 s hold expires.
    bool ShouldStartSnapRaceMode(bool                     lbRaceModePressed,
                                 f32                      lfGameTimestep,
                                 EGameModeStartMechanism* lpOutStartMechanism);

    // ⭐ X360 0x82396CF8. The actual start. Console arguments, read off the prologue
    // (@0x82396D0C/D10/D14) and DetectModeStarts' call site (@0x8239A550..0x8239A560):
    // r3 = this, r4 = lpInput, r5 = lpOutput, r6 = the start mechanism ShouldStartSnapRaceMode
    // wrote. ⚠️ IT EARLY-RETURNS unless the mechanism is E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_
    // AT_LIGHTS (2) -- `cmpwi cr6, r31, 2 / bne loc_82397300` @0x82396D64.
    void StartModeAtLights(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                           GameStateModuleIO::OutputBuffer*              lpOutput,
                           EGameModeStartMechanism                       leStartMechanism);

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

    // ------------------------------------------------------------------------
    // ⭐ X360 0x8239ED10 -- the module's SECOND-pass prepare. Its ONE caller is
    // LoadingScriptedState::LoadGameState2 @0x823EF4D8, i.e. scripted-load stage 3, which pumps
    // it until it returns true and forwards the requests it stages into the frame's GameData
    // input (AppendRequestInterface<3072>), exactly like GamePrepare stage 4 does for Prepare.
    //
    // ⭐ THIS is where PROGRESSION.DAT is loaded: case 0/1 calls
    // ProgressionManager::Prepare2 @0x8239DC98, whose LoadProgressionData @0x82399ED0 does
    // LoadBundle("Progression.dat", pool 5) -> acquire("ProgressionData") -> bind
    // mpProgressionData. Nothing on PC ran it before 2026-08-11, which is why
    // ProgressionManager::GetProgressionData() answered NULL and OnPlayerCarChange fired the
    // console's own "lpProgressionData != NULL" assert (BrnGameStateModule.cpp:4636) the moment
    // the junkyard handed a car over.
    //
    // ⚠️ SLICE: the progression leg is REAL; the StreetManager leg (case 2) logs once and
    // advances -- see the body.
    bool Prepare2(GameStateModuleIO::OutputBuffer* lpOutputBuffer);

    // The track's TriggerData / traffic-lane owner, and the spawn-location source the junkyard
    // car-select flow walks. DWARF BrnGameStateModule.h:201 (X360 this+42320).
    TriggerQueryManager*       GetTriggerQueryManager()       { return &mTriggerQueryManager; }
    const TriggerQueryManager* GetTriggerQueryManager() const { return &mTriggerQueryManager; }

    // [stuntrace 2026-08-26] The embedded progression manager (by value, :1171). Named accessor
    // added for ModeManager's back-pointer wiring (ConstructInterModeStateBringUp): the console
    // passes &mProgressionManager as ModeManager::Construct's argument at the sole call site;
    // the bring-up seam reaches it through this accessor instead of a raw offset.
    // [PC bring-up observer -- NOT an X360 method.] See mbPrepare2Complete's note: true once
    // Prepare2's progression AND street legs have both completed, i.e. once the two bundles the
    // GUI lane's WorldDataController::Prepare2 acquires by name are resident in pool 5.
    bool IsPrepare2Complete() const { return mbPrepare2Complete; }

    BrnProgression::ProgressionManager*       GetProgressionManager()       { return &mProgressionManager; }
    const BrnProgression::ProgressionManager* GetProgressionManager() const { return &mProgressionManager; }

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

    // ⭐ [gateui] The COLLECTIBLE bookkeeper (X360 this+183952 == 0x2CE50, 1568 bytes). Held BY
    // VALUE exactly as the console holds it -- attested five ways: Construct @0x82380388
    // (`StuntManager::Construct(a1+183952, a1+47920, a1+42320, a1+4128, a1+46640, a1)`),
    // Prepare @0x8239E578 stage 4, PreWorldUpdate @0x823A5328 (both `StuntManager::Update
    // (a1+183952, ...)` and the TriggerQueryManager::PreWorldUpdate argument),
    // ProcessGameEvents @0x823A0A18 case 111, and ProgressionManager::Construct's 5th argument.
    // The 1568-byte size is pinned by the next sub-object (GameStateImageManagerBase at
    // this+185520; 185520 - 183952 == 1568).
    //
    // Like mCarSelectManager, the console NEVER names an accessor for it -- every call site
    // reaches the sub-object through the inlined `this + 0x2CE50` pointer adjust. De-inlined to
    // this named accessor so no reconstructed body has to poke a byte offset.
    StuntManager*       GetStuntManager()       { return &mStuntManager; }
    const StuntManager* GetStuntManager() const { return &mStuntManager; }

    // [drive-thru wave 2026-08-27] Same de-inlining as GetStuntManager above: the console reaches
    // the drive-thru sub-object through an inlined `this + 44240` adjust and names no accessor, so
    // one is provided here rather than letting any body poke the byte offset.
    DriveThruManager*       GetDriveThruManager()       { return &mDriveThruManager; }
    const DriveThruManager* GetDriveThruManager() const { return &mDriveThruManager; }

    // ------------------------------------------------------------------------
    // ⭐⭐ [gateui] X360 PostWorldUpdate @0x8238F358 -- ITS TWO STUNT-CHAIN LEGS.
    //
    // The console body opens `LockForRead(lpPostWorldInput)` and then, among other copies:
    //     XMemCpy(this + 235488, lpInput->GetActiveRaceCarOutputInterface(), 10480);   // sub_8231D2C0
    //     VariableEventQueue<1536,16>::Append<1536,16>(this + 248384,
    //                                                  lpInput->GetGameEventQueue());  // 0x8231D0C8
    // i.e. it refreshes mLastActiveRaceCarInterface from the world's published snapshot and folds
    // the world's per-frame game-event queue into the module's CARRY queue, which PreWorldUpdate
    // then merges and hands to ProcessGameEvents on the NEXT frame.
    //
    // ⛔ WHY BOTH LEGS ARE LOAD-BEARING FOR THIS WAVE, and why the first one is the bigger of the
    // two: mLastActiveRaceCarInterface had NO WRITER ANYWHERE IN THE TREE (its FLAG at the member
    // said so), so it read as the Clear()ed "no valid player car" state on every frame. That is
    // not a cosmetic gap -- TriggerQueryManager::UpdateTriggers gates its ENTIRE active-set
    // rebuild on `lpActiveRaceCarInterface->IsPlayerCarActive()`, so with a dead interface
    // maActiveTriggers stays EMPTY for ever and StuntManager::OnPropHit iterates nothing and
    // latches nothing. StuntManager::Update has the same dependency (an inactive player car makes
    // it drop the latch instead of processing it).
    //
    // [FLAG PC bring-up] THE ARGUMENTS ARE THE DEVIATION, NOT THE BODY. The console reads both
    // out of a GameStateModuleIO::PostWorldInputBuffer that BridgeWorldToGameState @0x823E5368
    // fills; nothing on PC creates that buffer (its accessors exist, its producer does not), so
    // this entry point takes the two values DIRECTLY -- and the world module's OutputBuffer hands
    // out exactly these two types (BrnWorldModuleIO.h: `GetActiveRaceCarOutputInterface() const`
    // and `GetGameEventQueue() const`, the latter typedef'd to VariableEventQueue<1536,16>), so
    // the caller passes them through unchanged. The interface copy is done BY ASSIGNMENT, never
    // as the console's 10480-byte XMemCpy: the host object is a different size.
    // DELETE-WHEN PostWorldUpdate lands with a real PostWorldInputBuffer.
    // ⭐ [D4 stuntrace WAVE D] THE THIRD ARGUMENT IS NEW: the frame delta the console's
    // GameStateModule::PostWorldUpdate keeps in f1 and forwards to ModeManager::PostWorldUpdate
    // (@0x8238F358 `bl` #19). It feeds LEG 3 below -- the extracted stunt-scorer fork, i.e. the
    // call that actually drives StuntModeScoring::Update. See the body for why the leg is
    // extracted rather than a straight `mModeManager.PostWorldUpdate(...)` (there is no
    // PostWorldInputBuffer on this build, and a synthesised one would hand ModeManager an
    // X360-sized opaque blob where a host RCEntityActiveRaceCarOutputInterface must be).
    // ⭐ [showtime score wave 2026-08-29] THE FOURTH ARGUMENT IS THE FRAME'S CONTACT SPY, and it
    // feeds the new LEG 5 -- GameStateModule::ProcessContacts, the console's own `bl` #25 of the
    // same PostWorldUpdate. Same deviation as the first two arguments and for the same reason:
    // the console reads it out of the PostWorldInputBuffer (+0x6E30, BrnGameStateModuleIO.h:204)
    // and nothing on this build fills one, while the world module's UpdateOutputBuffer publishes
    // exactly this type through its const GetContactSpyInterface().
    void PostWorldUpdateStuntBringUp(
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                                                      lpActiveRaceCarOutputInterface,
        const CgsModule::VariableEventQueue<1536, 16>* lpWorldGameEventQueue,
        f32                                           lfDelta,
        const BrnPhysics::ContactSpy::ContactSpyInterface* lpContactSpyInterface);

    // ==========================================================================================
    // ⭐⭐⭐ [showtime score wave 2026-08-29] ProcessContacts -- X360 0x8236BC68, DWARF :853.
    //
    // THE PRODUCER CrashModeScoring::DealWithHitTrafficCar AND ::DealWithHitProp HAVE NEVER HAD.
    // The console's sole caller is GameStateModule::PostWorldUpdate @0x8238F358 (`bl` #25,
    // bracketed by PerfMonCpu Start/StopMonitor(*(this+292352) == miProcessContactsPM)), which
    // itself has no call site on this build -- so on the X360 this function runs every showtime /
    // stunt-attack frame and here it ran never.
    //
    // [FLAG PC bring-up] THE ARGUMENT IS THE DEVIATION, NOT THE BODY -- and here the reduction is
    // total rather than partial: the console takes `const PostWorldInputBuffer*` and the ONLY
    // thing it ever reads from it is `GetContactSpyInterface()` (sub_82362988, +0x6E30), called
    // three times. Every other value in the body comes from `this`. So the parameter is that
    // interface, and the body below is the console's, statement for statement.
    // DELETE-WHEN a real PostWorldInputBuffer exists: change the parameter back and add the one
    // accessor call at the top -- nothing else in the body moves.
    // ==========================================================================================
    void ProcessContacts(const BrnPhysics::ContactSpy::ContactSpyInterface* lpContactSpyInterface);

    // ⭐⭐ [gateui] X360 PreWorldUpdate @0x823A5328 -- ITS THREE STUNT-CHAIN LEGS, IN THE
    // CONSOLE'S OWN ORDER (which matters, see below):
    //     239-245  build a LOCAL VariableEventQueue<1536,16> and Append THREE sources into it
    //              (the carry queue this+248384, the PreWorldInputBuffer's queue, and the
    //              InviteManager's this+2032), then Clear the carry queue
    //     252      ProcessGameEvents(this, thatQueue, actionQueue, preWorldInput, output)
    //                                            <- case 111 -> StuntManager::OnPropHit LATCHES
    //     310      TriggerQueryManager::PreWorldUpdate(this+42320, in, out, this+183952,
    //                                                  this+44240, this+235488, mpVehicleList)
    //                                            <- UpdateTriggers ARMS the trigger set
    //     332      StuntManager::Update(this+183952, out->GetGameActionQueue(), this+235488,
    //                                   f1 == the game timestep, isGameModeActive)
    //                                            <- CONSUMES the latch
    //
    // ⚠️⚠️ OnPropHit RUNS BEFORE THE TRIGGER SET IS REFRESHED -- it deliberately walks the
    // PREVIOUS frame's armed set. That is the console's own order; DO NOT "fix" it.
    //
    // [FLAG PC bring-up] two documented reductions, both stated rather than hidden:
    //   * the three-source merge collapses to the carry queue alone, because the other two
    //     sources do not exist on this build (nothing creates a PreWorldInputBuffer, and the
    //     InviteManager's queue has no producer). The Clear of the carry queue is the console's.
    //   * TriggerQueryManager::PreWorldUpdate @0x8239F5C8 is reduced to its UpdateTriggers leg --
    //     the one that writes maActiveTriggers, which is the only thing OnPropHit reads. Its
    //     other legs (SubmitTriggerQueries, the per-player-trigger fan-out that posts action 109
    //     and calls ProcessPlayerTriggers, the killzone drain) walk arrays this tree's
    //     TriggerQueryManager slice does not model. Named as a park; nothing is fabricated.
    // ⭐⭐⭐ [D4 stuntrace WAVE D -- THE PUMP] THE SAME ENTRY POINT, GROWN INTO THE FULL
    // PreWorldUpdate SPINE. Everything above still holds; what wave D adds is the rest of the
    // console's own body order between those legs. The `bl` sequence of X360
    // GameStateModule::PreWorldUpdate @0x823A5328, numbered by its position in the call stream
    // (the numbering is this wave's, the ORDER is the binary's):
    //
    //     #68   GameStateModule::ProcessGameEvents            <- the arms below, then the Clear
    //     #86   GameStateModule::EmmPreWorldUpdate @0x8238EF50
    //                -> ModeManager::PreWorldUpdate @0x823537B8      (gsm+4128 == 0x1020)
    //     #93   TriggerQueryManager::PreWorldUpdate            <- the two legs already here
    //     #95   ProgressionManager::PreWorldUpdate             (not staged -- see the body)
    //     #96   GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418
    //     #97   GameStateModule::SendSetUpAllDriveThrusMessage (gated on a latch; not staged)
    //     #98   GameStateModule::DetectModeStarts @0x8239A428
    //     #103  StuntManager::Update                           <- the leg already here
    //
    // ⚠️ THE TIMER ARGUMENT IS NEW AND IT IS LOAD-BEARING. ModeManager::PreWorldUpdate takes a
    // `const CgsSystem::TimerStatusInterface&` and reads its SIM sub-status for the frame's
    // timestep (BrnModeManager_WorldTick.cpp: `GetSimTimerStatus()->GetCurrentTimeStep()`), which
    // is what drives the mode clocks, the countdown and the mode timer. The console fills it by
    // copying the PreWorldInputBuffer's own 48-byte timer block into the module at gsm+208328 --
    // EmmPreWorldUpdate @0x8238EF50 does exactly twelve word copies out of
    // `PreWorldInputBuffer::GetTimerStatusInterface()` and then passes `a1 + 208328` as the third
    // ModeManager argument. On PC NOTHING fills the module-owned buffer's timer block (the one
    // live producer, BridgeControllerToGameState, writes only the controller sub-object), so a
    // copy of it would hand ModeManager an all-zero interface and every mode clock would stand
    // still. BrnGameModule::mTimerStatusInterface IS filled, every sub-step, by the console's own
    // `TimerStatusInterface::StoreTimers(&mGameTimer, &mSimTimer)` (BrnGameModule.cpp:1411) -- the
    // same data by the same route, one copy earlier -- so the caller hands that in by const
    // reference. [FLAG PC bring-up] the SOURCE of the interface is the deviation, not its content.
    // DELETE-WHEN DoUpdate_GameStatePreWorld lands and stages a real PreWorldInputBuffer whose
    // timer block is filled: this parameter then goes and the body reads it off the buffer.
    void PreWorldUpdateStuntBringUp(f32 lfGameTimestep, bool lbIsAGameModeActive,
                                    const CgsSystem::TimerStatusInterface& lrTimerStatusInterface);

    // ⭐⭐⭐ [A9 scoring-feed wave 2026-08-27] CopyScoringDataToOutput -- X360 0x8236CDC0, REAL,
    // whole. Not an extracted leg: this is the console function, one for one.
    //
    // WHY IT IS LOAD-BEARING. It is the SOLE caller of ModeManager::WriteDataToOutput
    // @0x82337B70 (bodied + mounted, BrnModeManager.cpp:285) -> ScoringSystem::WriteDataToOutput
    // @0x8232AE98 (bodied + mounted, BrnScoringSystem_Lifecycle.cpp:385) -- both of which had NO
    // caller at all -- and it itself writes the twelve ScoringOutputInterface scalars the
    // GameState->Gui bridge gates on, meGameModeType (+0xA3C) and mbTimerActive (+0xAA8) among
    // them. Without it the bridge posts GUI id 424 every frame with an all-zero payload (and a
    // clear mbTimerActive, which FREEZES mfEventTime rather than zeroing it) and never posts id
    // 428 at all, because that build gate reads the scoring interface's meGameModeType.
    //
    // CONSOLE CALL SITE, exact: GameStateModule::PreWorldUpdate @0x823A5328's ONLY xref-to,
    // between `RumbleManager::UpdatePauseState` and the `(a6 & 8)` TriggerQueryManager block,
    // inside the `LockForWrite(lpOutput)` bracket, monitored by the +292348 PerfMon slot.
    //
    // [FLAG PC bring-up] ONE deviation, the same one PreWorldUpdateStuntBringUp above carries
    // and for the same measured reason: the frame's Time comes from a TimerStatusInterface
    // handed in by the caller instead of from the module's own 48-byte copy of the
    // PreWorldInputBuffer's timer block (the console reads gsm+208368, which is that copy's
    // mSimTimerStatus.mTime -- EmmPreWorldUpdate @0x8238EF50 fills gsm+208328..+208372 with the
    // twelve-word copy). Nothing on PC fills that block, so the console route would hand this
    // function an all-zero "now" and every published mode time would be garbage. Same data, one
    // copy earlier. DELETE-WHEN DoUpdate_GameStatePreWorld stages a real PreWorldInputBuffer.
    void CopyScoringDataToOutput(GameStateModuleIO::OutputBuffer* lpOutput,
                                 const CgsSystem::TimerStatusInterface& lrTimerStatusInterface);

    // ⭐⭐ [D4 stuntrace WAVE D] X360 ProcessGameEvents @0x823A0A18, THE CASE-20 ARM
    // (E_EVENT_PLAYER_ACCEPTED_MODE; asm 0x823A2680..0x823A2718, source BrnGameStateModule.cpp:2456
    // per the DWARF unity dump). Same one-arm-at-a-time extraction as the 78 / 94 / 111 / 113 / 115
    // arms. The console arm, verbatim:
    //
    //     0x823A2680  lwz  r11, 0x1DB8(r31)     ; gsm+7608 == mModeManager.mpCurrentGameMode
    //     0x823A2688  bne  -> skip              ; the arm runs ONLY when no mode is running
    //     0x823A268C  _vector_constructor_iterator_(&params.maCheckpointDataArray, 44, 16, ...)
    //     0x823A26B4  sub_823102F0(&tmp, gsm + 235488)   ; ActiveRaceCarOutputInterface::
    //                                                    ;   GetPlayerPosition (asserts
    //                                                    ;   IsPlayerCarActive; reads car+1360)
    //     0x823A26BC  li r5, 0 / li r4, 0
    //     0x823A26CC  StartGameModeParams::Construct(&params, r4, r5, v1)
    //     0x823A26D0  lbz  r11, 0x4C(r25)       ; event->muNumLandmarks
    //     0x823A26E0  r30 = r25 + 8             ; &event->mauLandmarkSectionIds[0]
    //     0x823A26E8  lhz  r5, 0x24(r30)        ; event + 0x2C + 2i
    //     0x823A26EC  lhz  r4, 0(r30)           ; event + 0x08 + 2i
    //     0x823A26F0  StartGameModeParams::AddCheckpoint(&params, r4, r5)
    //     0x823A2714  ModeManager::StartGameMode(gsm + 0x1020, r27 /*OutputBuffer*/, &params)
    //
    // ⛔⛔ CORRECTION TO THE WAVE PREMISE, PROVEN FROM THE ASM ABOVE -- READ BEFORE PLANNING ON
    // THIS ARM. `li r4, 0` / `li r5, 0` are the Construct arguments, and Construct @0x8231C1F8
    // stores r4 to +0x2D0 (meGameModeType) and r5 to +0x310 (meStartMechanism). So case 20
    // ALWAYS starts E_MODE_OFFLINE_RACE (0) with E_GAMEMODESTARTMECHANISM_DEFAULT (0). It never
    // reads the event's own meModeType (+0x48) and never reads mRaceId (+0x00): the ONLY field it
    // consumes is the landmark/section checkpoint list. Case 20 is therefore NOT the stunt-race
    // start -- the offline stunt start is GameStateModule::StartModeAtLights @0x82396CF8, which
    // sets mechanism 2 and resolves the runtime mode through ProgressionManager::GetEvent. Do not
    // "fix" the hard-coded 0 here; it is the binary's.
    //
    // Argument shape: the console reads the event out of the merged pre-world queue and takes the
    // OutputBuffer from PreWorldUpdate's own local (r27), so this arm takes both.
    void ProcessGameEventsStartGameModeBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::OutputBuffer*               lpOutputBuffer);

    // ⭐⭐ [D4 stuntrace WAVE D] X360 ProcessGameEvents @0x823A0A18, THE INTRO/RESULTS EXIT ARMS
    // (cases 24 / 25 / 26 / 27; asm 0x823A272C for 26, pseudocode lines 1121-1134):
    //     case 24: ModeManager::FinishedMapPan(gsm + 4128)
    //     case 25: ModeManager::FinishOfflineModeIntro(gsm + 4128)
    //     case 26: ModeManager::ResultsAccept(gsm + 4128); *(gsm + 181413) = 1
    //     case 27: ModeManager::UserCancelCurrentMode(gsm + 4128);
    //              TakedownManager::ClearRaceCarData(gsm + 568)
    // The event ids match the DWARF EGameEventType exactly in this range (24 FINISHED_MAP_PAN,
    // 25 GUI_FINISHED_OFFLINE_PRE_EVENT, 26 RESULTS_FINISHED, 27 POST_EVENT_LEAVE) -- verified by
    // the callee on each arm, not assumed.
    //
    // ⚠️ ONLY CASE 25 IS ARMED TODAY. ModeManager::FinishOfflineModeIntro @0x823119B0 is bodied
    // (BrnModeManager_IntroPlay.cpp:540); FinishedMapPan / ResultsAccept / UserCancelCurrentMode
    // are neither declared nor bodied on this tree, so their arms are PARKED IN THE BODY with the
    // console call written out. Nothing is fabricated. DELETE-WHEN those three land.
    void ProcessGameEventsModeIntroBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue);

    // ================================================================================
    // (i) [D4 PUMP SEAM] CheckIfPlayerIsAtJunctionWithAnEvent (X360 0x82390418) and
    // DetectModeStarts (X360 0x8239A428) are the two functions THIS lane stages, at console
    // positions #96 and #98 of PreWorldUpdate @0x823A5328. Their declarations and bodies are
    // agent D3's and live in the start-function block further down this header -- D4 landed a
    // duplicate block here during the parallel wave and removed it once D3's arrived. If a
    // future landing re-adds declarations for either, check that block first.
    // ================================================================================

    // ⭐ [D4 stuntrace WAVE D] HARNESS-ONLY, NOT IN THE X360 BINARY. Env-gated (BRN_START_EVENT=1)
    // one-shot that substitutes for the 0.35 s analogue accelerator+brake HOLD -- and for nothing
    // else: it calls StartModeAtLights @0x82396CF8 with the mechanism ShouldStartSnapRaceMode
    // would have written at a junction (E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS == 2, the
    // value StartModeAtLights early-returns without), so every hop downstream is the console's.
    // Gated additionally on TriggerQueryManager::IsPlayerInTrafficLightRegion() and on no mode
    // already running; logs loudly; fires at most once per process. Agent D5 wires the flow_run
    // switch that sets the env var. Full note at the body.
    void HarnessInjectEventStartBringUp(GameStateModuleIO::OutputBuffer* lpOutputBuffer);

    // ==============================================================================================
    // ⭐⭐⭐ [showtime S7b-a wave, 2026-08-27] THE SHOWTIME START. Both bodies live in
    // GameStateModule_Showtime.cpp.
    // ==============================================================================================

    // ⭐ X360 0x8236B580 (80 insns; asserts baked at BrnGameStateModule.cpp:5579/5580/5582).
    // Build a StartGameModeParams at the player's position for E_MODE_OFFLINE_SHOWTIME (2) or
    // E_MODE_ONLINE_SHOWTIME (16) and hand it to ModeManager::StartGameMode. Console arguments,
    // from the prologue and the two `bl` sites: r3 = this, r4 = lpInput, r5 = lpOutput.
    // ⚠️ lpInput IS ASSERTED AND THEN NEVER READ -- r4 is reloaded with `this + 235488` two
    // instructions later. The parameter is the console's; do not drop it.
    // ⭐ THIS IS THE FUNCTION THE WHOLE SHOWTIME CHAIN BOTTOMS OUT IN: it is what eventually makes
    // ModeManager::PrepareForMode post action 23 with KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR, which
    // is the ONLY console road into VehicleManager::SetPlayerCarToShowtimeMode @0x8259C108.
    void StartCrashMode(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                        GameStateModuleIO::OutputBuffer*              lpOutput);

    // ⭐⭐⭐ X360 0x82356B18 (166 insns) -- THE SHOWTIME GATE STACK. Returns true on the single
    // frame the crash-start hold expires with every condition met; every refusal re-arms that hold.
    //
    // SIGNATURE IS THE DWARF's (BrnGameStateModule.h:781
    // `bool ShouldStartShowtimeMode(float32_t, bool, TimerRequests*)`) AND THE ASM AGREES, which is
    // what settles the phantom-argument trap: Hex-Rays renders this
    // `(int a1, double a2, int a3, char a4, unsigned int* a5)` -- FIVE arguments -- because the f32
    // rides f1 and CONSUMES the r4 GPR slot. r4 is never read in the body; the bool is r5
    // (`clrlwi r11, r5, 24` @0x82356C18) and the pointer is r6 (`lwz r11, 0(r6)` @0x82356BF0).
    // Three arguments, in the DWARF's order.
    //
    // The third argument is the OUTPUT buffer's SIM timer request block --
    // `OutputBuffer::GetTimerRequest() + 8` at the call sites, i.e.
    // GetTimerRequestInterface()->GetSimTimerRequests(); the gate refuses while any of its three
    // request bits is already set this frame.
    bool ShouldStartShowtimeMode(f32                       lfGameTimestep,
                                 bool                      lbCrashModePressed,
                                 CgsSystem::TimerRequests*  lpSimTimerRequests);

    // ⭐ X360 0x82356A60 (11 insns) -- `mfShowtimeIntroTimeLeft > 0.0f`. The showtime intro is the
    // 0.5 s window DetectModeStarts' else arm opens between the gesture completing and
    // StartCrashMode firing; while it is open the controller bridge overrides the player's steering
    // with GetShowtimeIntroSteering() (see GameBridgeControllerToX.cpp's FLAG on that leg).
    bool IsInShowtimeIntro() const;

    // ⭐ X360 0x82356A90 (33 insns) -- returns mfShowtimeIntroSteering, asserting IsInShowtimeIntro()
    // first (BrnGameStateModule.cpp:5050). The value is +/-1.0f, latched by the else arm from the
    // sign of the player car's angular velocity Y at the moment the intro opens.
    f32 GetShowtimeIntroSteering() const;

    // ⭐ [D4 stuntrace WAVE D] TEMPORARY, NOT IN THE X360 BINARY. The offline event intro has NO
    // timer by design (IntroState::OnEnter raises mbUseCountdown only for online modes and offline
    // Showtime), so its ONLY console exit is GUI command 163 -> game event 25 ->
    // ModeManager::FinishOfflineModeIntro. The pre-event GUI that sends 163 is not reconstructed,
    // so a started offline mode would sit in E_GMS_INTRO for ever. This posts event 25 into the
    // carry queue once the intro has run for GameMode::GetIntroDurationSeconds() (StuntAttackMode
    // returns 6.0f, X360 0x827E2538 -> lfs [0x82021240]), which is the duration the console's own
    // pre-event screen is scheduled against. Posting the EVENT rather than calling
    // FinishOfflineModeIntro directly is deliberate: it exercises the real case-25 arm.
    // ⛔ DELETE-WHEN BrnGui's offline pre-event state sends GUI command 163 (the producer half,
    // BridgeGuiToGameState case 163 -> event 25, is ALREADY live and mounted at
    // GameBridgeGUIToX_GameState.cpp:153) -- then this function and its call go, in one edit.
    void HarnessOfflineIntroSelfTriggerBringUp(f32 lfGameTimestep);

    // ⭐⭐ [gateui] X360 ProcessGameEvents @0x823A0A18, THE CASE-111 ARM (0x823A1684..0x823A1698).
    // The console's dispatcher is a ~180-case jump table this tree extracts one arm at a time
    // (the precedent: ProcessGameEventsReallyEnterJunkyardBringUp / ...ActivateCarSelectBringUp
    // above). This arm, verbatim from the asm:
    //     0x823A1684  addis r3, r31, 3          ; \
    //     0x823A1690  addi  r3, r3, -0x3170     ; / r3 = this + 183952  == &mStuntManager
    //     0x823A1688  lvx128 v1, r0, r25        ; v1 = event->mPosition   (event +0x00, Vector3)
    //     0x823A1694  lhz   r4, 0x10(r25)       ; r4 = event->muZoneId    (event +0x10, u16)
    //     0x823A168C  lhz   r5, 0x12(r25)       ; r5 = event->muPropId    (event +0x12, u16)
    //     0x823A1698  bl    StuntManager::OnPropHit
    // (the Vector3 rides v1 and consumes NO GPR slot -- the PPC float-arg rule in reverse, which
    //  is why the committed OnPropHit(u16, u16, Vector3) signature is the right one).
    // The event is GameStateModuleIO::RecordPropHitEvent, game EVENT id 111 -- and game event ids
    // are NOT subject to the +5 action-id shift (see BrnGameActions.h): 111 matches the X360 jump
    // table exactly.
    void ProcessGameEventsPropHitBringUp(const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue);

    // ⭐⭐ [tut-ticker] X360 ProcessGameEvents @0x823A0A18, THE CASE-113 ARM -- "a world system
    // asks for a training tip". Same extraction precedent as the case-111 arm above. The console
    // arm is one call: `BrnGameState::TrainingManager::RequestTraining(this + 46640, *payload)`
    // with the payload's leading s32 being the BrnProgression::ETrainingType. Producers of game
    // event 113 (all world-side, each drains through RaceCarEntityModule::SendGameEvents or its
    // siblings): the junkyard-exit request (action 149 -> HandleGameActions case 149), the
    // car-type tip (action 77 -> HandleCarTypeTrainingMessage), AI buzz-by (16), roll/spin (49),
    // boost-strategy (50).
    void ProcessGameEventsTrainingRequestBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue);

    // ⭐ [H1 district wave 2026-08-25] X360 ProcessGameEvents @0x823A0A18, THE CASE-115 ARM --
    // "the player crossed into a new district". Same extraction precedent as the case-111/113
    // arms above. The console arm is three statements (h1_dump3.txt):
    //     GameStateImageManagerBase::HandleWorldRegionChangeEvent(this+185520, payload);
    //     AddEvent(actionQueue, {county,district}, /*action*/112, 8);
    //     *(this+181512) = payload->meDistrict;
    // Reproduced: the ACTION POST (the load-bearing hop -- the bridge turns action 112 into
    // GUI event 169, the HUD district marker's feed). FLAG'd deferrals: the image-manager
    // handler (the GameStateImageManagerBase sub-object is not a PC member yet -- its Prepare
    // is the stage-24 deferral) and the +181512 store (member un-homed; not fabricated).
    // Producer: RaceCarEntityModule::UpdateCurrentWorldRegion (event 115, world side).
    void ProcessGameEventsWorldRegionBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue* lpActionQueue);

    // ⭐ [P1 sim-pause] X360 ProcessGameEvents @0x823A0A18, THE PAUSE FAMILY -- the four
    // arms that route pause-state events into RequestPause/RequestUnpause (same extraction
    // precedent as the case-111/113/115 arms above). Console arms, verbatim (p1_dump.txt):
    //   case 33 (PLAYER_PAUSE_STATE_CHANGED): payload {b0 pause?, b1, b2};
    //       b0 ? RequestPause(2, q, b1, b2) : RequestUnpause(2, q)
    //   case 35 (ENTER_REPLAY):  RequestPause(16, q, 0, 0)
    //   case 36 (LEAVE_REPLAY):  RequestUnpause(16, q)
    //   case 93 (CRASHNAV_STATE_CHANGED): payload {b0};
    //       b0 ? RequestPause(4, q, 0, 0) : RequestUnpause(4, q)
    // ⚠️ THE INVERTED SEMANTIC IS CONSOLE TRUTH: BridgeGuiToGameState's GUI-191 arm posts
    // 93 payload 1 when the crash-nav DEACTIVATES (payload = (guiWord0==0)), so ACTIVATING
    // the map UNpauses and the deactivate pauses. Do not "fix" it.
    void ProcessGameEventsPauseBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue* lpActionQueue);

    // ⭐⭐ [driver-details pause wave 2026-08-28] X360 ProcessGameEvents @0x823A0A18, THE CASE-80
    // ARM -- "the GUI asks for the player's rank progress". Same extraction precedent as the
    // case-111/113/115 and the pause-family arms above. The console arm, verbatim from the asm
    // @0x823A2D54..0x823A2E60 (Hex-Rays renders the same nine statements):
    //     data = mProgressionManager.GetProgressionData();          // ResourcePtr, null-guarded
    //     rankCount = data->muProgressionRankCount;                 // lwz 0x14(data)
    //     m8 = GetProgressionRankForGameMode(E_MODE_MARKED_MAN);    // li r4, 8
    //     s7 = GetProgressionRankForGameMode(E_MODE_STUNT_ATTACK);  // li r4, 7
    //     r3 = GetProgressionRankForGameMode(E_MODE_ROAD_RAGE);     // li r4, 3
    //     o0 = GetProgressionRankForGameMode(E_MODE_OFFLINE_RACE);  // li r4, 0
    //     record.SetProgressionRanks(GetProgressionRank(), rankCount, o0, r3, s7, m8);
    //     record.SetProgressionRankEventWins(<the four maiRankWinsPerOfflineGameMode reads>);
    //     if (PlayerHasFinishedLastRank()) record.miPlayerRank = -1;   // `li r11,-1; stw r11,+0x00`
    //     AddEvent(actionQueue, &record, /*action*/181, /*size*/0x24);
    //
    // ⭐ THE FOUR WIN COUNTS ARE ONE NAMED ARRAY, not four members. The console reads them as
    // four raw module offsets (`lwzx r4..r7` from +0xBE9C/+0xBEA8/+0xBEB8/+0xBEBC, i.e.
    // ProgressionManager +0x36C/+0x378/+0x388/+0x38C) because it INLINED the accessor:
    // Profile::GetNumRankWinsForGameMode @0x8230FA40 is literally `*(4 * (mode + 127) + this)`,
    // i.e. maiRankWinsPerOfflineGameMode[mode] at Profile+0x1FC, and the embedded Profile sits at
    // ProgressionManager+0x170 -- 0x170 + 0x1FC + 4*{0,3,7,8} == exactly those four offsets. So
    // the arm is four indexed reads through the committed accessor, and the modes are the SAME
    // four, in the same order, that SetProgressionRanks takes.
    //
    // PRODUCER of the event: BridgeGuiToGameState's case 437 (GameBridgeGUIToX_GameState.cpp),
    // fed by CrashNavDriverDetails::UpdateInitSetup's GuiEventRankProgressRequest.
    // CONSUMER of the action: TranslateGameActionsToGuiEvents case 181 -> GUI event 438, which is
    // what CrashNavDriverDetails::UpdateSetupLicense is parked waiting for.
    void ProcessGameEventsRankInfoRequestBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue* lpActionQueue);

    // ⭐⭐⭐ [pause-stats wave 2026-08-29] X360 ProcessGameEvents @0x823A0A18, THE CASE-79 ARM --
    // "the GUI asks for the player's game stats". The immediate neighbour of the case-80 arm
    // above, extracted the same way, and it is SEVEN instructions long in the console because
    // both halves of the work are calls:
    //     0x823A2D18  addi r3, r31, 0x7E20   ; &mChallengeManager
    //     0x823A2D1C  bl   ChallengeManager::CountCompletedChallenges     ; -> r3
    //     0x823A2D20  mr   r6, r3                                        ; the THIRD argument
    //     0x823A2D2C  addi r5, r31, 0x2CE90  ; &mStuntManager
    //     0x823A2D30  addi r4, r1, var_E30   ; a stack-local GameStats
    //     0x823A2D34  addi r3, r31, 0xBB30   ; &mProgressionManager
    //     0x823A2D38  bl   ProgressionManager::GetGameStats
    //     0x823A2D3C  li   r6, 0x160         ; 352 == sizeof(GameStats)
    //     0x823A2D40  li   r5, 0xB4          ; action 180
    //     0x823A2D48  mr   r3, r22           ; the action queue
    //     0x823A2D4C  bl   VariableEventQueue<13312,16>::AddEvent
    //
    // ⭐ ARGUMENT ORDER IS THE ASM's, AND IT MATTERS: CountCompletedChallenges runs FIRST, into
    // r6, so the challenge count is already in hand when GetGameStats is entered -- it is a
    // plain third parameter, not something GetGameStats fetches. The PS3 DWARF's two-parameter
    // `GetGameStats(GameStats*, StuntManager*) const` would have had nowhere to put it.
    //
    // PRODUCER of the event: BridgeGuiToGameState's case 435 (GameBridgeGUIToX_GameState.cpp),
    // fed by CrashNavDriverDetails::UpdateInitSetup's GuiEventStatsRequest -- posted in the SAME
    // cache latch that posts the 437 the case-80 arm answers.
    // CONSUMER of the action: TranslateGameActionsToGuiEvents case 180 -> GUI event 436, which
    // CrashNavDriverDetails::HandleStatData (and BrnGui::CrashNavStats) read.
    void ProcessGameEventsGameStatsRequestBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue* lpActionQueue);

    // ⭐⭐ [tut-ticker] X360 PreWorldUpdate @0x823A5328, THE TRAINING LEG (@0x823A57A4..0x823A57C8):
    //     bl ShouldAllowTimedTutorialTips     ; r3 = this
    //     mr r8, r3                           ; -> Update's trailing bool
    //     TrainingManager::Update(this+46640 /*r3*/, lpPreWorldInput /*r4*/,
    //                             actionQueue /*r5 == r16*/, this+235488 /*r6, the embedded
    //                             mLastActiveRaceCarInterface*/, f1 = the game timestep, r8)
    // [FLAG PC bring-up] the PreWorldInputBuffer argument has no PC producer (same deviation the
    // sibling legs carry); Update's one console read of it (the controller interface's user index
    // for XUserGetSigninState) is taken with user 0 against the PC stub, which reports signed-out
    // either way. The extraction is the deviation; the call, its arguments and its position after
    // the stunt leg are the console's.
    void PreWorldUpdateTrainingBringUp(f32 lfGameTimestep);

    // ⭐ [tut-ticker] X360 0x82356DB0 -- ShouldAllowTimedTutorialTips. True only when the player
    // car is live and nothing suppresses ambient tips. Console reads, in order:
    //   * the embedded interface (this+235488): mePlayerActiveRaceCarIndex != -1 (with the
    //     interface's own bounds assert) and mbIsPlayerCarActive,
    //   * a byte at this+245952 -- see the body FLAG (no writer exists in the export set),
    //   * miSimPauseFlags (this+232288) == 0,
    //   * mCarSelectManager.mJunkyardId low word (lwz at this+183748) == 0,
    //   * mModeManager.mpCurrentGameMode (this+7608) == 0.
    bool ShouldAllowTimedTutorialTips();

    // [tut-ticker] the heap TrainingManager (see mpTrainingManager's FLAG at the member). The
    // console embeds it by value at this+46640; ModeManager::PreWorldUpdateClocksBringUp and the
    // two extracted arms above reach it through this named accessor.
    TrainingManager*       GetTrainingManager()       { return mpTrainingManager; }
    const TrainingManager* GetTrainingManager() const { return mpTrainingManager; }

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

    // ⭐⭐ X360 0x823759D0 -- SendSetUpAllEventStartsMessage. THE EVENT-START TABLE PRODUCER, and
    // the only path in the image to SetUpAllEventStartsInterface::AddEventStart @0x82361398.
    // Walks every light trigger of every TrafficData hull, keeps the junctions that carry BOTH
    // start grids, and publishes {junction position, light-trigger id, junction id, event junction
    // id, county, nearest AI section} into the output buffer's SetUpAllEventStartsInterface, then
    // raises its valid flag for BridgeGameStateToGui to turn into GUI event 203.
    // Its two console call sites are PreWorldUpdate @0x823A5328 (the one-shot latch below, which
    // is where this build calls it) and ProcessGameEvents @0x823A0A18.
    // Body: GameStateModule_SendSetUpAllEventStarts.cpp (read its banner before touching this).
    void SendSetUpAllEventStartsMessage(GameStateModuleIO::OutputBuffer* lpOutput);

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
    //
    // ⭐⭐⭐ [returning-player wave 2026-08-28] THE bool IS GONE AND THE STAND-IN WITH IT.
    // The second leg (the extracted ProcessGameEvents case-78 arm below) used to be gated on an
    // ORDERING STAND-IN the caller supplied -- MainDirector::IsNewProfileIntroActive(), a
    // NEW-PROFILE-ONLY signal. That made the start-of-game junkyard entry impossible to complete
    // on a boot that finds a Profile.sav, which is the whole "a returning player cannot drive"
    // defect (see the case-78 banner below for the measurement and the retraction). It now drains
    // the console's own game event 78 out of mGameEventCarryQueue, which BridgeGuiToGameState
    // fills -- so the trigger is the GUI's, on both paths, exactly as on the console.
    void PreWorldUpdateSetupPlayerCarBringUp();

    // ⭐⭐ X360 ProcessGameEvents @0x823A0A18, THE CASE-78 ARM (0x823A4590..0x823A45F8) --
    // "the GUI says the player is really in the junkyard now, finish the entry".
    //
    // The console's dispatcher is
    //     void ProcessGameEvents(const GameEventQueue*, InputBuffer::GameActionQueue*,
    //                            const PreWorldInputBuffer*, OutputBuffer*)
    // (DWARF BrnGameStateModule.h:695; the asm consumes exactly those four in r4..r7). It is a
    // ~180-case jump table over a merged event queue that PreWorldUpdate builds on the stack from
    // three sources, and it is not reconstructed. Only arm 78 is extracted here, with the arm's
    // own gate (mbWaitingToPutPlayerInJunkyard) intact -- so what runs is console code, and the
    // deviation is the TRIGGER, not the body.
    //
    // ✅ [returning-player wave 2026-08-28] THE DELETE-WHEN IS PAID: THE GUI'S OWN EVENT DRIVES IT.
    // The console reaches this arm from game event 78, which BridgeGuiToGameState @0x823DDB78
    // translates out of GUI out-event 145 (BrnGui::InGame::OnEnter @0x824D0498). Every rung of
    // that bridge now exists AND is plumbed: InGame::OnEnter posts 145 (BrnInGame.cpp:388),
    // BrnGameModule::DoUpdate calls BridgeGuiToGameState every in-game sub-step, and its sink --
    // GameStateModuleIO::PostWorldInput -- is mGameEventCarryQueue, the very queue the other
    // extracted ProcessGameEvents arms already walk. So the trigger below is the console's.
    //
    // ⛔⛔ THE NOTE THAT USED TO STAND HERE WAS WRONG, AND IT COST A WEEK.
    // It read: "measured on this build -- InGame::OnEnter runs ~40 log lines BEFORE the latch is
    // armed, so a faithfully-plumbed bridge would deliver the event to a latch that does not yet
    // exist", and on that basis the arm was driven off the latch alone, gated by
    // MainDirector::IsNewProfileIntroActive(). ⭐ THE MEASUREMENT CONFUSED TWO DIFFERENT
    // "InGame::OnEnter"s. The line it read is BrnGameMainFlowInGameState::OnEnter ("InGame:
    // OnEnter -> GUI FSM stage 5"), the FLOW-CONTROLLER state -- not BrnGui::InGame::OnEnter, the
    // GUI SCREEN state that actually posts command 145. The screen state's own observable is the
    // command-65 line ("in-game screen entered (65)"), and it lands ~200 lines AFTER the latch:
    //     fresh      (scratch/flow_run/eng_d1_freshreg) SendSetupPlayerCarEvent :1052  65 :1249
    //     returning  (scratch/flow_run/eng_b2_probe)    SendSetupPlayerCarEvent :1032  65 :1229
    // The producer therefore fires comfortably AFTER the latch on BOTH paths, and the real event
    // is a strictly better trigger than the stand-in -- on the fresh path it arrives 20 lines
    // EARLIER than IsNewProfileIntroActive() used to fire it (:1249 vs :1270, same sub-step
    // neighbourhood), and on the returning path it arrives at all, which the stand-in never did.
    void ProcessGameEventsReallyEnterJunkyardBringUp(GameStateModuleIO::GameActionQueue* lpActionQueue);

    // ⭐⭐ [returning-player wave 2026-08-28] The QUEUE WALK for the arm above -- the same shape
    // as ProcessGameEventsPropHitBringUp / ...WorldRegionBringUp / ...PauseBringUp: the console's
    // dispatcher makes one pass over the merged queue and this tree extracts one arm per
    // function. Reads mGameEventCarryQueue WITHOUT clearing it (PreWorldUpdateStuntBringUp owns
    // the console's Clear, later in the same sub-step), so the other arms still see the frame.
    // ⓘ POSITION IS THE CONSOLE'S: PreWorldUpdate runs the latch leg (@0x823A5510), then
    // ProcessGameEvents (@0x823A58B8), then the CarSelectManager tick (@0x823A5904) -- so this
    // walk lives in PreWorldUpdateSetupPlayerCarBringUp, ahead of PreWorldUpdateCarSelectBringUp,
    // not in the later PreWorldUpdateStuntBringUp pass (which would cost a sub-step).
    void ProcessGameEventsGuiStartedGameBringUp(
            const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
            GameStateModuleIO::GameActionQueue* lpActionQueue);

    // ⭐⭐ X360 PreWorldUpdate @0x823A5328, the CAR-SELECT leg at 0x823A5904..0x823A5958:
    //     PerfMonCpu::StartMonitor(mCpuMonitors.<car-select>);
    //     if (mCarSelectManager.mJunkyardId != kCGSID_NULL)          // `ld r11,0(this+0x2CDC0)`
    //         mCarSelectManager.Update(lpActionQueue,
    //                                  lpInput->GetControllerInput(),
    //                                  lfGameTimestep);
    //     PerfMonCpu::StopMonitor(...);
    // (the console's `f31` is `TimerStatusInterface::maEntries[0].mfValue04 *
    //  .mfValue08` -- the GAME timer's rate * scale, latched at 0x823A54D8.)
    //
    // ⭐ WHY THIS LEG MATTERS: it is the ONLY caller of CarSelectManager::Update in the whole
    // image, and CarSelectManager::Update is what ENDS the junkyard transition-in. Without it
    // meState stays E_STATE_TRANSITION_IN for ever, EndTransitionInState never posts its
    // action 73 {0, hasCars}, and MainDirector::ProcessInputQueue never moves
    // GameState::meJunkyardState off E_JY_INTRO_NO_CARS -- which is exactly what this build
    // did (measured: `jy 2` on every frame to the end of the run).
    //
    // [FLAG PC bring-up] the EXTRACTION is the deviation, as with the two legs above: the gate,
    // the call and its arguments are the console's. The controller-input argument is passed as
    // NULL because nothing on this build creates a GameStateModuleIO::PreWorldInputBuffer --
    // CarSelectManager::Update never dereferences it (it threads it to the deeper state
    // updaters, none of which take it in the reconstruction).
    // DELETE-WHEN PreWorldUpdate lands with its real input buffer.
    void PreWorldUpdateCarSelectBringUp(f32 lfGameTimestep);

    // ⭐⭐ X360 ProcessGameEvents @0x823A0A18, THE CASE-94 JUNKYARD ARM (the switch at
    // 0x823A4EE0-ish; pseudocode `case 94:` -> `if (v236 == 1) { assert IsInJunkyard();
    // switch (*_R25) { 0: StartCarSelectState  1: EnterModification  4: ExitJunkyard } }`).
    //
    // ⭐ THE TWO PAYLOAD WORDS ARE (ACTION, TYPE), IN THAT ORDER. The dispatcher reads the
    // SECOND word (`_R25[1]`) as the car-select TYPE (1 == junkyard, 2 == online event) and the
    // FIRST word (`*_R25`) as the ACTION. Both GUI producers write the pair as ONE big-endian
    // `std`, so the value that lands in word0 is the __int64's HIGH dword:
    //   CarSelectVehicle::Update @0x824DCBF0  `v13 = meCarSelectType`            -> {0, type}
    //   CarSelectLivery::Update  @0x824DFCD0  `LODWORD=type; HIDWORD=1`          -> {1, type}
    //   CarSelectMain::ExitCarSelection @0x824C8CB8  record {8,192,12,4,1}       -> {4, 1}
    // i.e. entering the vehicle screen starts car-select, entering the livery screen enters
    // modification, and accepting on the livery screen exits the junkyard.
    //
    // [FLAG PC bring-up] the EXTRACTION is the deviation -- the arm's own gate, its three calls
    // and its default assert are the console's. The console reaches it from game event 94, which
    // BridgeGuiToGameState @0x823DDB78 case 192 translates out of GUI out-event 192; that
    // translation IS reconstructed (GameBridgeGUIToX.cpp) but the bridge has no caller and its
    // sink (GameStateModuleIO::PostWorldInput) has no definition, so BrnGameModule's
    // BridgeGuiToGame walk calls this directly with the SAME decode.
    // DELETE-WHEN ProcessGameEvents + the post-world input buffer + BridgeGuiToGameState's
    // caller are real.
    void ProcessGameEventsActivateCarSelectBringUp(s32 liAction, s32 liCarSelectType);

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

    // DWARF BrnGameStateModule.h:106 (references/DecFIGS/.../BrnGameStateModule.h:106-111),
    // names and values verbatim. Every value is attested by a console WRITER of the member:
    //   0 NOT_IN_GAME                ModeManager::SendModeStopMessages @0x8234BEC0, ClearData
    //                                @0x8236B3A8, Destruct @0x82375420 -- "no game mode is
    //                                running", i.e. free roam
    //   1 CAR_SELECT                 ProcessGameEvents @0x823A0A18 case 16
    //   2 INACTIVE_GAME_MODE_STATE   ModeManager::PlayerFinishedMode @0x823280D8, and the
    //                                case-106 `else` arm
    //   3 ACTIVE_GAME_MODE_STATE     ModeManager::UpdateCurrentMode @0x82350EC8 (mode start),
    //                                and the case-106 `if` arm
    // Declared here, ahead of the member block, because a nested type must precede the member
    // that names it.
    enum EControllerState
    {
        E_CONTROLLERSTATE_NOT_IN_GAME              = 0,
        E_CONTROLLERSTATE_CAR_SELECT               = 1,
        E_CONTROLLERSTATE_INACTIVE_GAME_MODE_STATE = 2,
        E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE   = 3
    };

    // ⭐⭐ DWARF BrnGameStateModule.h:1173 `bool IsControllerActive() const`. The console
    // INLINES it into PreWorldUpdate @0x823A5328, where it reads verbatim:
    //     v61 = *(a1 + 232292);                        // meControllerState
    //     if ( v61 == 3 || (v63 = v61 != 0, v62 = 0, !v63) ) v62 = 1;
    //     GameStateModuleIO::OutputBuffer::SetControllerActive(a5, v62);
    // i.e. TRUE for exactly E_CONTROLLERSTATE_NOT_IN_GAME (free roam -- no game mode is
    // running, the player drives Paradise City) and E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE
    // (an event is actually under way). FALSE while the car-select screen owns the pad and
    // while a mode is in its inactive phase (countdown / results).
    bool IsControllerActive() const;

    // [stuntrace waveB fix round, 2026-08-26] The controller-state WRITERS. meControllerState is
    // private with only the reader declared, yet its own banner already names "the four ModeManager
    // / ProcessGameEvents sites" as its writers -- two of which are in this wave and were parked
    // for want of a mutator: ModeManager::UpdateCurrentMode writes value 3 and
    // ModeManager::PlayerFinishedMode writes value 2 (the enum comment at :481 already records
    // both), and SendModeStopMessages @0x8234BEC0 writes 0 (`stwx r20(0), r11, 0x38B64`).
    // X360-INLINED at every site (a plain word store), so inline bodies are the faithful form.
    void SetControllerState(EControllerState leControllerState) { meControllerState = leControllerState; }
    void SetActiveGameModeState()   { meControllerState = E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE; }
    void SetInActiveGameModeState() { meControllerState = E_CONTROLLERSTATE_INACTIVE_GAME_MODE_STATE; }

    // [stuntrace waveB fix round, 2026-08-26] X360 this+232296 (0x38B68). See the member banner for
    // the two asm attestations and for why the requested 0x38BE8 spelling is refuted.
    BrnNetwork::NetworkPlayerID GetLocalPlayerNetworkID() const { return mLocalPlayerNetworkID; }

    // ⭐⭐ THE EXTRACTED CONTROLLER-ACTIVE PUBLISH of PreWorldUpdate @0x823A5328 (the store
    // immediately after the IsOnlineGameMode / +536 / +537 / +532 block).
    //
    // ⛔ WITHOUT IT NOTHING CAN DRIVE THE CAR -- pad, keyboard OR harness. The flag reaches the
    // physics as BrnGameModule.cpp:1022 `lpWorldInput->SetControllerActive(...)` ->
    // WorldBridgeInputToEntityModules.cpp:250 -> RaceCarEntityModule::ProcessPlayerVehicleInput,
    // whose `lbControllerActive` false arm ZERO-FILLS BrnPlayerDriverControls (gas, brake,
    // handbrake AND steering). GameStateModuleIO::OutputBuffer's ctor leaves mbControllerActive
    // false and 0x82363040 has exactly ONE caller in the whole image -- PreWorldUpdate -- which
    // is not reconstructed, so on this build the flag was false for the entire run. MEASURED
    // 2026-08-12: `ctrlactive 0` on every frame, `gas 0.000` with the throttle held.
    //
    // [FLAG PC bring-up] the EXTRACTION is the deviation, as with the two PreWorldUpdate legs
    // above: the condition, the store and its position relative to the rest of the body are the
    // console's. DELETE-WHEN PreWorldUpdate lands whole.
    void PreWorldUpdatePublishControllerActiveBringUp();

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

    // [stuntrace waveB fix round, 2026-08-26] MOVED FROM private -- ACCESS ONLY, no text change to
    // the declaration itself. A cross-class console caller cannot call a private method, and there
    // is one: ModeManager::SetupOpponentData @0x82329348 does
    // `bl BrnGameState__GameStateModule__GetOriginalCarId` at 0x8232940C with r3 = the module and
    // r4 = the player car id read from gsm+0x456D8. The DWARF dump also lists it in the public run
    // (BrnGameStateModule.h:534, between OnSpecialEventPlayerCarChange and ApplyCarStats), and this
    // declaration's own comment already named that caller while sitting in the private block.
    // X360 0x823758E8. Walk lCarId up its VehicleListEntry parent chain (at most two levels, as the
    // console does) to the base/"original" car a livery variant derives from. Used by
    // OnPlayerCarChange to look up the opponent set, and by ModeManager::SetupOpponentData /
    // StartModeAtLights (not reconstructed) on the console.
    CgsID GetOriginalCarId(CgsID lCarId);

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

    // ⭐ REAL (2026-08-11). The X360 BurnoutSkillzManager::Construct (0x82332688) reaches the
    // embedded achievement manager through the owning GameStateModule (the inlined
    // `*(modeManager->mpGameStateModule) + 181680` pointer adjust to the achievement-manager
    // subobject). De-inlined to this named accessor; it now hands back the REAL embedded
    // mAchievementManager below.
    //
    // ⚠️ RETURN TYPE CORRECTED, and the X360 asm is the reason: the DWARF spells this
    // `StuntModeScoring::AchievementManager*`, a PLATFORM TYPEDEF that resolves to the SKU's
    // concrete leaf -- AchievementManagerPS3 on the PS3 DecFIGS build we took the name from, but
    // **AchievementManagerX360** on the ARTIST spine we are reconstructing. Attested four ways in
    // the X360: GameStateModule::Prepare @0x8239E578 calls AchievementManagerX360::Prepare
    // (this+181680), Release @0x823756A8 calls AchievementManagerX360::Release, PreWorldUpdate
    // @0x823A5328 calls AchievementManagerX360::Update, and the ctor @0x827E44B8 stores that
    // class's vtable there. Declared as the shared base (the type ProgressionManager already
    // holds it as -- BrnProgressionManager.h:350 `AchievementManagerBase* mpAchievementManager`),
    // because every consumer calls base hooks through it.
    AchievementManagerBase* GetAchievementManager();

    // ADDITIVE GROW (declare-only) for the BrnTrainingManager TU.
    // X360 BrnGameState::GameStateModule::RequestPause -- TrainingManager::TriggerAnyFollowOnTrainingTips
    // (0x823889C8) calls it as RequestPause(this, 64, lpGameActionQueue, 0, 0) to pause the world while a
    // training voiceover plays. The leading s32 is the pause-reason bitflag (the X360 immediate 0x40 ==
    // "training" reason); the GameActionQueue* is the OutputBuffer's action queue the pause request is
    // broadcast through; the trailing two s32s are 0 at this call site.
    // ⭐ [tut-ticker] BODIED 2026-08-24 (X360 0x82382010) -- see the .cpp: OR the reason bit into
    // miSimPauseFlags and, when the strict/loose IsSimPaused answer actually changed, broadcast
    // the 1-byte pause action (86 on the strict transition, 88 on the loose one). The two trailing
    // s32s are the bools forwarded to the two IsSimPaused probes.
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

    // ⭐ ACCESSOR GROW (stuntrace wave B, agent 9 -- header_grow_spec section 6 item (1)).
    // X360 GetLastGlobalRaceCarInterface (DWARF BrnGameStateModule.h:624 / member :323) -- the
    // read-only GLOBAL race-car snapshot embedded BY VALUE at this+245968 (0x3C0D0), immediately
    // after mLastActiveRaceCarInterface (235488 + 10480 == 245968 exactly).
    // THREE independent X360 attestations of that seat, all recovered this wave:
    //   * GameStateModule::PostWorldUpdate @0x8238F358: `XMemCpy(this + 245968, <the post-world
    //     input's global interface>, 2416)` on the line after the 10480-byte active-interface copy;
    //   * GameStateModule::ClearData @0x8236B3A8:
    //     `RCEntityGlobalRaceCarOutputInterface::Clear(this + 245968)` immediately after
    //     `RCEntityActiveRaceCarOutputInterface::Clear(this + 235488)`;
    //   * four ModeManager bodies read it: WriteDataToOutput @0x82337B70 and
    //     HasRaceCarHitValidCheckpoint @0x82329910 (whose `*(4*(global+525)+iface)` is the
    //     interface's OWN maeActiveRaceCarIndices @+0x834 == +2100, i.e. an inlined
    //     GetActiveRaceCarIndex), plus TransmitAndIncrementCheckPointsReached @0x82342098 and
    //     TransmitAndIncrementFinishReached @0x823424D0.
    // ⛔ CORRECTION TO header_grow_spec section 6: items (1) and (2) there are the SAME offset
    // written two ways -- 0x3C0D0 == 245968 -- and it is the GLOBAL interface, not a second ACTIVE
    // one. There is NO live-active interface member on GameStateModule at all: the live one arrives
    // through the world module's input buffer and is only ever COPIED into the two snapshots above.
    // ⚠️ FLAG: exactly like its active sibling, nothing on PC refreshes it (PostWorldUpdate's
    // world-snapshot leg is not reconstructed), so it reads as the Clear()ed state -- every
    // GetActiveRaceCarIndex answers E_ACTIVE_RACE_CAR_INDEX_INVALID. That is the console's own
    // "no data yet" answer and it fires the console's own asserts at the readers, which is the
    // wanted behaviour, not a silent wrong index.
    // DELETE-WHEN GameStateModule::PostWorldUpdate's snapshot leg lands (same commit as the active
    // interface's refresh -- the console does both copies back to back).
    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*
        GetLastGlobalRaceCarInterface() const;

    // ⭐ ACCESSOR GROW (stuntrace wave B, agent 9 -- header_grow_spec section 6 item (3)).
    // X360 GetNetworkRandomSeed (DWARF BrnGameStateModule.h:594) -- the u32 at this+208300
    // (0x32DAC), the word immediately below mePlayerActiveRaceCarIndex (208304, pinned by
    // GetPlayerActiveRaceCarIndex @0x82311570) and immediately above mafRivalTailingTimes[7]
    // (208272..208299, ClearData's four 64-bit zero stores). DWARF declaration order puts
    // muNetworkGameRandomSeed in exactly that 4-byte gap (:257 f32[7], :260 u32, :263 the index).
    // WRITER: ProcessGameEvents @0x823A0A18 stores the incoming StartNetworkGameEvent's word[3]
    // (`*(v23 + 208300) = _R25[3]`) on the line before NetworkRoundManager::NetworkGameStarted.
    // READER (this wave): ModeManager::TellGuiToShowOnlineFinalStandings @0x82329B68 hands it to
    // ScoringSystem::UpdateCumulativeResults as its first argument.
    u32 GetNetworkRandomSeed() const;

    // ⭐ [tut-ticker] RETIRED 2026-08-24: `GetTimePlayedInTimedMode` (declare-only, no callers).
    // The f32 the X360 reads at this+42300 (0xA53C) is IDENTIFIED: it is
    // mModeManager.mfTimeInFreeBurn (ModeManager +0x951C; DWARF BrnModeManager.h:1025), and the
    // sibling read at +42304 is mfTimeInMode (+0x9520, DWARF :1026) -- proven from the writers
    // (ModeManager::PreWorldUpdate @0x823537B8 accumulate/reset conditions; see the accessor
    // banner in BrnModeManager.h). Callers reach them as
    // GetModeManager()->GetTimeInFreeBurn() / GetTimeInMode().

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
    // [P1 sim-pause] the PostWorldInput free function returns the private carry queue (see its
    // declaration above the class) -- friendship, not a fabricated public accessor, per the
    // GuiCache exposure rule.
    friend CgsModule::VariableEventQueue<1536, 16>* GameStateModuleIO::PostWorldInput(GameStateModule*);

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

    // DWARF BrnGameStateModule.h:771. The by-value ModeManager that owns the current game mode.
    ModeManager         mModeManager;
    // DWARF BrnGameStateModule.h:793 (X360 this+208300) -- the seed word for the online session's
    // shared RNG, declared immediately ABOVE mePlayerActiveRaceCarIndex by the DWARF and landing in
    // exactly the 4-byte gap the X360 leaves there. See GetNetworkRandomSeed() for the writer and
    // the reader.
    // ⚠️ FLAG (initialisation site only, same precedent as mpCurrentCarData / mpOutputBuffer): the
    // console seeds it to -1 in GameStateModule::ClearData @0x8236B3A8 (`*(a1 + 208300) = -1`,
    // NOT 0), and ClearData is not reconstructed on this build. GameStateModule is a by-value
    // sub-object of BrnGameModule and is not in its ctor init list, so without this initialiser the
    // word is indeterminate. The value IS the console's -- it is image-cited from ClearData, not a
    // placeholder zero. DELETE-WHEN ClearData lands.
    u32                 muNetworkGameRandomSeed = 0xFFFFFFFFu;
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
    // ⭐ X360 this+232292 (0x38B64), the word immediately after the pause flags -- and DWARF
    // BrnGameStateModule.h:806 puts `EControllerState meControllerState` immediately after
    // `EPauseFlags mePauseFlags` (:805), so the name and the neighbour agree independently.
    // Read by IsControllerActive(); written by ClearData @0x8236B3A8 and Destruct @0x82375420
    // (both to 0) and by the four ModeManager / ProcessGameEvents sites listed on the enum.
    // ⓘ Nothing on this build writes it yet -- ModeManager's mode machine and ProcessGameEvents'
    // case 16/106 arms are not reconstructed -- so it holds its zero-init
    // E_CONTROLLERSTATE_NOT_IN_GAME, which is the CORRECT state for the free-roam the junkyard
    // handover leaves the player in, and which IsControllerActive() reports as active.
    EControllerState    meControllerState = E_CONTROLLERSTATE_NOT_IN_GAME;
    // [stuntrace waveB fix round, 2026-08-26] X360 this+232296 (0x38B68) -- the word immediately
    // after meControllerState, requested by three wave-B partfiles. OFFSET ARBITRATED FROM THE ASM
    // (two implementers filed different numbers): it is 0x38B68, NOT 0x38BE8.
    //   * BrnGameState::OnlineFlybyManager::GetLocalPlayerNetworkID @0x82358720 is the whole proof:
    //     after asserting "mpGameStateModule" it does `lis r10,3 / ori r10,r10,0x8B68 /
    //     lwzx r3, r11, r10` on its cached module pointer -- a 4-byte load at gsm+0x38B68 returned
    //     AS the local player network id, and the console's own symbol name supplies the accessor
    //     name used here.
    //   * ModeManager::PrepareForMode @0x82342930 materialises the SAME address
    //     (`lis r11,3 / ori r14,r11,0x8B68` @0x82342B78/0x82342B7C) for its online disconnect
    //     guard, immediately after loading the assert literal
    //     "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID".
    // No export forms gsm+0x38BE8 at all; that filing was a transcription slip and is REFUTED.
    BrnNetwork::NetworkPlayerID mLocalPlayerNetworkID;

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

    // ⭐ X360 +0x38B72 (232306) -- THE SECOND HALF OF THE START-OF-GAME JUNKYARD HANDSHAKE.
    // SendSetupPlayerCarEvent @0x8239A918 sets it; ProcessGameEvents @0x823A0A18 case 78 tests it
    // (`addis r29,r31,4; addi r29,r29,-0x748E; lbz r11,0(r29)`), runs
    // CarSelectManager::ReallyEnterJunkyardAtStartOfGame and clears it (`stb r18,0(r29)`, r18==0).
    // NAME IS THE CONSOLE'S: DWARF BrnGameStateModule.h:811 `bool mbWaitingToPutPlayerInJunkyard`.
    // The DWARF declaration order pins it exactly at the asm-attested byte -- :809/:810 are the two
    // streaming bools at 232304/232305, :812 is an s32 at 232308, :813 a bool[4] at 232312, and
    // :816 (the 16-aligned action below) lands on 232320, which is the OTHER offset this arm's asm
    // attests. Both ends of the DWARF span agree with the asm, so the byte is not a guess.
    bool mbWaitingToPutPlayerInJunkyard = false;

    // X360 +0x38B80 (232320). The CarSelectionChangedAction SendSetupPlayerCarEvent hands
    // EnterJunkyardAtStartOfGame as its sixth argument (the console passes `this + 0x38B80`); the
    // junkyard's id + spawn transform + "pos is left" bit are written into it, and ProcessGameEvents
    // case 78 posts the whole 64-byte record as game action 64 once the junkyard entry completes.
    // ⭐ NAME CORRECTED 2026-08-01: the repo called this `mCarSelectionChangedAction`, which was
    // this repo's invention. The console name is `mCachedCarSelectChangedAction` -- DWARF
    // BrnGameStateModule.h:816, independently attested by the case-78 assert literal
    // ("mCachedCarSelectChangedAction.mJunkyard..." @ BrnGameStateModule.cpp:4099).
    // Held by value exactly as the console holds it.
    GameStateModuleIO::CarSelectionChangedAction mCachedCarSelectChangedAction;

    // X360 +0x397E0. The read-only active-race-car snapshot the module caches at the end of the last
    // world update, held BY VALUE as the console holds it (see GetLastActiveRaceCarInterface).
    BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface mLastActiveRaceCarInterface;
    // X360 +0x3C0D0 (245968) == 235488 + 10480, i.e. flush against the member above. The read-only
    // GLOBAL race-car snapshot, held BY VALUE as the console holds it. See
    // GetLastGlobalRaceCarInterface() for the three attestations (PostWorldUpdate's 2416-byte
    // XMemCpy, ClearData's Clear, and the four ModeManager readers).
    // [!] DWARF BrnGameStateModule.h:323 declares it in exactly this position, one line after :320
    // mLastActiveRaceCarInterface -- DWARF order and X360 offsets agree here.
    BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface mLastGlobalRaceCarInterface;
    // X360 @0x823566F8 hands this back (GetOutputGuiEventQueue); the PaybackManager and the
    // other managers publish their HUD/GUI events onto it.
    CgsModule::VariableEventQueue<18432, 16> mOutputGuiEventQueue;
    // X360 SetCarUnlockAlreadyShown @0x82363698 resolves the player Profile through it.
    BrnProgression::ProgressionManager       mProgressionManager;

    // ⭐ DWARF BrnGameStateModule.h:226 -- `StuntModeScoring::AchievementManager mAchievementManager;`
    // declared immediately after mProgressionManager (:216) and before mRoadRulesManager (:229).
    // X360 this+181680 (0x2C5B0), embedded BY VALUE: the module ctor @0x827E44B8 writes the leaf's
    // vptr straight into it (`*(a1 + 181680) = &off_820CE768`) and Construct @0x82380388 runs
    // `AchievementManagerBase::Construct(a1 + 181680, a1 + 47920, a1 + 284520, a1 + 7632, a1)` --
    // i.e. (&mProgressionManager, &mStreetManager, mModeManager.GetScoringSystem(), this).
    // The concrete type is the X360 leaf (see GetAchievementManager above for the four attestations);
    // the PC build reuses it, the same precedent as CgsNetwork::BuddyManagerX360 standing in as the
    // platform buddy leaf (BrnNetworkBuddyManagerBase.h:57) and CgsSaveLoadX360.cpp being mounted in
    // the PC exe -- there is no AchievementManagerPC leaf in any build.
    AchievementManagerX360                   mAchievementManager;

    // (DWARF BrnGameStateModule.h:229 also declares `RoadRulesManager mRoadRulesManager;` between
    // these two -- X360 this+183592. NOT modelled yet: its only consumer here would be
    // StreetManager::Construct's third argument, and that Construct is deferred for the measured
    // link reason spelled out in BrnGameStateModule.cpp's Construct. Add it with that call.)

    // ⭐ DWARF BrnGameStateModule.h:425 -- `StreetManager mStreetManager;` (X360 this+284520,
    // 0x457E8). Embedded BY VALUE: GameStateModule::Construct @0x82380388 runs
    // `StreetManager::Construct(a1 + 284520, a1, a1 + 47920, a1 + 183592)` and FIVE other
    // subobjects take its address there (ProgressionManager / RoadRulesManager /
    // AchievementManagerBase / DeveloperChallengeManager Constructs all receive a1 + 284520).
    // Prepare2's case 2 (`StreetManager::Prepare2(a1 + 284520, a2, a1 + 232384, a1 + 42320)`)
    // pumps it.
    StreetManager                            mStreetManager;

    // The module's own output buffer -- see GetOutputBuffer() above for the ownership FLAG.
    // Zero-initialised in-class: BrnGameModule embeds this module by value and does NOT list
    // it in its ctor init list, so without this the Construct() guard would read garbage.
    GameStateModuleIO::OutputBuffer*         mpOutputBuffer = 0;

    // [D2 gesture-sink] The module-owned stand-in for the console's per-frame
    // "GameStatePreWorld" IOBufferStack buffer -- see GetPreWorldInputBuffer() above for the
    // full attestation, the refuted +0x2BE8 premise, and the DELETE-WHEN. Zero-initialised
    // in-class for the same reason mpOutputBuffer is: BrnGameModule embeds this module by value
    // and does not list it in its ctor init list.
    GameStateModuleIO::PreWorldInputBuffer*  mpPreWorldInputBuffer = 0;

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

    // ✅ [gateui] THE meDistrictsBundleStage STAND-IN LATCH IS RETIRED (2026-08-20).
    // It carried the LoadBundle("Districts.dat", pool 5) request/response half of
    // StuntManager::LoadDistrictMap @0x82399458 at the stage that calls it, because this module
    // had no StuntManager sub-object and BrnStuntManager.cpp's loader was an inert deferral. Both
    // of those are now false: mStuntManager below is the real embedded sub-object and its
    // LoadDistrictMap drives the console's full LoadBundle -> acquire("Districts") -> handle-bind
    // machine, so stage 4 is `mStuntManager.Prepare(lpOutputBuffer)` verbatim.
    // ⓘ The reason the latch's DONE state had to be STICKY still applies to the real machine and
    // is satisfied by it: Prepare's terminal stage re-arms mePrepareStage at MANAGER for a later
    // re-prepare pass, and StuntManager::meDistrictMapLoadStage's E_DISTRICT_MAP_DONE case
    // returns true without re-requesting -- so a second Prepare pass walks straight through
    // stage 4 instead of stalling on a spent request.
    // ⛔ [gateui] ROUND-3 CORRECTION (verify_r2_fixgsm F5). The round-2 wording of the line above
    // continued "...and only re-runs the census", which is FALSE on one path: if the FIRST pass
    // spent the PC bring-up acquire-retry budget and declared the district map unavailable, that
    // give-up is PROCESS-FINAL. StuntManager's give-up latches are file statics and
    // meDistrictMapLoadStage is already E_DISTRICT_MAP_DONE, whose arm never re-issues -- so every
    // later Prepare pass takes the give-up arm immediately and the census is NEVER re-run, even if
    // Districts.dat became resident in between. Accurately: a second pass walks through stage 4
    // and re-runs the census ONLY IF the first pass bound the map successfully; after a give-up it
    // walks through with the tally left at zero. The latches are reset in exactly one place
    // (LoadDistrictMap's E_DISTRICT_MAP_LOAD_REQUEST arm), so this self-corrects the moment
    // anything re-arms meDistrictMapLoadStage. See BrnStuntManager.cpp :: Prepare's give-up arm.

    // X360 this+556 (0x22C) -- Prepare2's OWN stage word, four bytes past mePrepareStage. Proven
    // straight off 0x8239ED10 (`lwz r11, 0x22C(r31)` / three `stw ..., 0x22C(r31)`). It is NOT
    // the word Prepare's tail clears: that one is this+560 (0x230), a different flag.
    enum EPrepare2Stage
    {
        E_PREPARE2STAGE_START          = 0,   // cases 0 and 1 share the progression leg
        E_PREPARE2STAGE_PROGRESSION    = 1,
        E_PREPARE2STAGE_STREET_MANAGER = 2,
        E_PREPARE2STAGE_DONE           = 3,
    };
    EPrepare2Stage mePrepare2Stage = E_PREPARE2STAGE_START;

    // [PC bring-up observer -- NOT an X360 member.] True once Prepare2 has completed BOTH legs,
    // i.e. once "Progression.dat" and "STREETDATA.DAT" are resident in pool 5. It cannot be
    // derived from mePrepare2Stage: the console never writes 3 into that word (case 3 is only
    // `li r28, 1`), so the stage word stops at 2. Read by BrnGameModule::ResourceUpdateThread
    // through IsPrepare2Complete() to hold the GUI lane's WorldDataController::Prepare2 until
    // the two resources it acquires by name actually exist. See the latch site in Prepare2.
    bool mbPrepare2Complete = false;

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

    // ⭐ [gateui] X360 this+183952 (0x2CE50). The Super-Jump / Super-Smash / Billboard collectible
    // bookkeeper, embedded BY VALUE as the console embeds it. See GetStuntManager() above for the
    // five console attestations of the offset and for why the console never names an accessor.
    StuntManager                            mStuntManager;

    // ⭐ [drive-thru wave 2026-08-27] X360 this+44240 (0x ACD0). The junk-yard / gas-station /
    // body-shop / paint-shop / car-park drive-thru bookkeeper, embedded BY VALUE as the console
    // embeds it. Console attestations for the offset:
    //   * GameStateModule::Prepare @0x8239E578 stage 27 calls DriveThruManager::Prepare(this+44240,
    //     the TriggerQueryManager's TriggerData, the CarColours palette) immediately BEFORE the
    //     car-select list publish this tree already reproduces in E_PREPARESTAGE_DONE.
    //   * PreWorldUpdate @0x823A5328 passes `a1 + 44240` as TriggerQueryManager::
    //     ProcessPlayerTriggers' lpDriveThruManager argument (the drive-thru fan-out leg).
    // The member is NOT laid out at the console byte offset (semantic parity by named members,
    // AGENTS.md) -- the offset is the identity proof. The type is already complete here:
    // BrnGameActions.h (included above) includes BrnDriveThruManager.h for GenericRegion::Type,
    // and BrnDriveThruManager.h forward-declares GameStateModule rather than including this
    // header, so there is no cycle.
    DriveThruManager                        mDriveThruManager;

    // ⭐ [gateui] X360 this+185712 (0x2D570). The developer-challenge tracker, embedded BY VALUE:
    // Construct @0x82380388 runs `DeveloperChallengeManager::Construct(a1 + 185712, a1 + 47920,
    // a1 + 284520, a1 + 7632, a1)` in place, and StuntManager::ProcessStuntElement @0x8239CDB0
    // reaches it as the inlined `mpGameStateModule + 185712` adjust (asserted there as
    // "mpGameStateModule->GetDeveloperChallengeManager()", BrnStuntManager.cpp:695). It was
    // already declared as an accessor (GetDeveloperChallengeManager, added for the StreetManager
    // wave) with no member behind it; the member lands here and the body with it.
    // ⚠️ NOT Construct()ed yet -- see the named deferral in GameStateModule::Construct.
    DeveloperChallengeManager               mDeveloperChallengeManager;

    // ⭐ [gateui] X360 this+248384 (0x3CA40). THE GAME-EVENT CARRY QUEUE -- the one-frame buffer
    // between the world and ProcessGameEvents. PostWorldUpdate @0x8238F358 Appends the world's
    // per-frame game-event queue into it; PreWorldUpdate @0x823A5328 merges it (with two other
    // sources) into a local queue, hands that to ProcessGameEvents, and Clears it.
    // ⓘ THE never-Constructed-queue TRAP IS PAID: the console's own Construct @0x82380388 carries
    // `CgsModule::VariableEventQueue<1536,16>::Construct(a1 + 248384)`, and this module's
    // Construct() reproduces that call. (A VariableEventQueue that is only Clear()ed still has no
    // buffer bound -- the exact trap that has bitten this tree four times.)
    // ⚠️ CORRECTION while adding it: the dangling comment further up this class (under the
    // ResetPlayerDebugComponent block) claimed this offset is "the same per-frame output queue
    // GetOutputGuiEventQueue() returns". It is not -- that one is a VariableEventQueue<18432,16>
    // at a different offset (its accessor is X360 0x823566F8); +248384 is this <1536,16> queue,
    // as both Append<1536,16> call sites prove. That comment had no member behind it either.
    CgsModule::VariableEventQueue<1536, 16>  mGameEventCarryQueue;

    // ⚠️ [FLAG PC bring-up] X360 this+46640. The console embeds TrainingManager BY VALUE here
    // (PreWorldUpdate @0x823A5328 calls `TrainingManager::Update(a1 + 46640, ...)`, and four
    // Constructs -- ProgressionManager's, RoadRulesManager's, DriveThruManager's and
    // StuntManager's -- are each handed `a1 + 46640` as an owner pointer).
    // Embedding it here is blocked by a GENUINE INCLUDE CYCLE, not by a missing reconstruction:
    // BrnTrainingManager.h `#include`s THIS header (its bodies call GameStateModule::RequestPause
    // / GetLastActiveRaceCarInterface / the timed-mode accessors), so this header cannot include
    // it back, and a by-value member needs the complete type. Held by pointer and allocated in
    // Construct() instead -- the same shape, and the same class of reason, as mpOutputBuffer
    // above: the ALLOCATION SITE moves, nothing else does. The pointer's VALUE is what every
    // consumer sees, and it is stable for the module's whole life.
    // DELETE-WHEN the cycle is broken (BrnTrainingManager.h forward-declaring GameStateModule and
    // moving its inline bodies out would do it) -- then this becomes `TrainingManager mTrainingManager;`.
    TrainingManager*                        mpTrainingManager = 0;

    // ========================================================================================
    // ⭐⭐ [stuntrace wave D, D3] THE JUNCTION CACHE + THE HOLD TIMER (X360 +0x456C8..+0x456D2
    // and +0x45714). Six members, every one pinned by a STORE this wave's four functions emit;
    // the offsets are quoted per member. As with the rest of this minimal slice the members are
    // NOT laid out at the console offsets -- the offsets are the identity proof, not a layout.
    //
    // The first five are a contiguous console run and CheckIfPlayerIsAtJunctionWithAnEvent
    // @0x82390418 is the only reader/writer of all five; ShouldStartSnapRaceMode @0x82363700
    // owns the sixth.
    // ========================================================================================

    // X360 +284360 (0x456C8). The traffic-light trigger id of the junction the last posted
    // "you are at a junction" action described. WRITTEN `stwx r30, r27, r9` @0x82390974 with
    // r9 == 0x456C8 and r30 == TriggerQueryManager::GetPlayerCurrentTrafficLightId(); RESET to
    // -1 by the departure post (`li r11,-1 / stw r11, 0(r29)` @0x82390EC0/EC4, r29 == this +
    // 0x456C8). The departure arm's own validity test reads it as the packed handle --
    // `rlwinm r9, r11, 0,8,23 / cmplw 0xFFFF00` (hull, bits 8..23) and `clrlwi r11,r11,24 /
    // cmplwi 0xFF` (light index, bits 0..7) @0x82390DEC..0x82390E0C -- which is the same
    // LightTriggerId::IsValid() TrafficData::GetJunctionLogicBoxForTrafficLight asserts.
    // ⚠️ FLAG (initialisation site only, the muNetworkGameRandomSeed precedent): the console
    // seeds it in the un-reconstructed ClearData/Construct pair; the value here is the one the
    // departure arm itself restores, so it is image-cited rather than a placeholder.
    u32  muCachedJunctionLightTriggerId = 0xFFFFFFFFu;

    // X360 +284364 (0x456CC). JunctionLogicBox::muID of that same junction (`lwz r9, 0(r31)` off
    // the box, `stw r9, 0(r11)` @0x82390998 with r11 == this + 0x456CC), re-read immediately as
    // the action's +0x00 field. The PREVIOUS value is latched one instruction earlier
    // (`lwz r7, 0(r11)` @0x82390994) and the difference drives the "the junction changed" bit
    // (`subf r9,r7,r8 / cntlzw / extrwi / xori` @0x823909A0..0x823909BC). Also reset to -1 by
    // the departure post (`stw r11, 0(r31)` @0x82390EC8).
    u32  muCachedJunctionLogicBoxId     = 0xFFFFFFFFu;

    // X360 +284368 (0x456D0). "This junction was discovered on THIS visit" -- the one-shot the
    // discovery arm sets (`*(result + 284368) = 1`, 0x823906xx) and every action-201 post
    // clears on its way out (`stb r20(0), 0(r17)` @0x82390DDC, `stb r28(0), 0(r26)`
    // @0x82390ED0). It feeds the action's mbIsNewlyDiscovered (+0x22) and forces the banner
    // even at speed. ⓘ Reads false for the whole run on this build -- see the PARKED discovery
    // arm in GameStateModule_gSR_00.cpp.
    bool mbJunctionNewlyDiscovered      = false;

    // X360 +284369 (0x456D1). "The player may start the event standing here" -- no game mode is
    // running, no blocking training tip is up, and the car is at or below 30 mph
    // (`stb r10, 0(r24)` @0x82390984, r24 == this + 0x456D1). Read back by the event arm's
    // LABEL_118 gate and by the departure post, and cleared by the departure post
    // (`stb r28(0), 0(r30)` @0x82390ECC).
    bool mbCanEnterEventAtJunction      = false;

    // X360 +284370 (0x456D2). "An arrival action has been posted for the junction I am in" --
    // set by the arrival post (`stbx r21(1), r27, r22` @0x8239096C, r22 == 0x456D2) and cleared
    // the moment the player is no longer in ANY traffic-light region (`stbx r28(0), r27, r22`
    // @0x823907EC). It is the second half of the post gate, which is why the console keeps
    // re-posting the arrival record every frame while the player idles in the junction.
    bool mbAtJunctionWithEvent          = false;

    // X360 +284436 (0x45714). The accelerator+brake HOLD countdown, in seconds.
    // ShouldStartSnapRaceMode decrements it by the frame timestep and fires when it crosses 0;
    // every bail arm re-arms it to 0.35 (`li`-free: the console stores the literal 0x3EB33333
    // == 0.35f, rendered by Hex-Rays both as `0.34999999` and as the raw `1051931443`).
    // ⓘ [showtime S7b-b] THE CONSOLE NAME FOR THIS MEMBER IS `mfTimeSpentDoingStartRaceAction`
    // (DWARF BrnGameStateModule.h:851) -- it is the immediate neighbour of, and the exact twin of,
    // mfTimeSpentDoingCrashStartAction below (:852, +284440). Not renamed here because
    // ShouldStartSnapRaceMode and its call sites are another lane's live work this session; the
    // DWARF name is recorded so a later pass is a rename, not a re-derivation.
    f32  mfSnapRaceStartHoldSeconds     = 0.35f;

    // =========================================================================================
    // ⭐⭐⭐ [showtime S7b-b, 2026-08-27] THE SEVEN SHOWTIME MEMBERS ShouldStartShowtimeMode
    // @0x82356B18 and the DetectModeStarts @0x8239A428 `else` arm bind to. Every NAME is the
    // DecFIGS DWARF's (BrnGameStateModule.h:852..860) and every OFFSET is a store or a load in
    // one of those two bodies or in a THIRD function that independently pins it -- cited per
    // member. As elsewhere on this minimal slice the members are NOT laid out at the console
    // offsets; the offsets are the identity proof, not a layout.
    //
    // ⚠️ THE DWARF DECLARATION ORDER AND THE X360 LAYOUT DISAGREE in this run (the DWARF puts the
    // two bools :857/:858 between the Vector3 and the enum; the X360 asm puts the enum at +284480,
    // immediately after the Vector3, and a bool at +284510). Names come from rung 2, offsets from
    // rung 1 -- so each member below is matched to its offset by BEHAVIOUR, not by counting
    // declarations. The three that carry an independent witness are marked ✅.
    //
    // ⓘ INITIALISERS. GameStateModule::Construct @0x82380388 seeds four of these
    // (`*(a1+284448) = 0.0`, `*(a1+284440) = 0.0099999998`, `*(a1+284444) = 2.0`,
    // `*(a1+284480) = 2`) and this tree's Construct() does not reproduce that block, so the values
    // are carried as member initialisers -- the same FLAG shape muNetworkGameRandomSeed uses. Each
    // value IS the console's, image-cited from Construct, not a placeholder.
    // =========================================================================================

    // X360 +284440 (0x45718), DWARF :852. The BOTH-BUMPERS hold countdown -- the crash-start twin
    // of mfSnapRaceStartHoldSeconds. ShouldStartShowtimeMode subtracts the frame timestep and
    // fires when it crosses 0; EVERY refusal arm re-arms it to the console's 0.0099999998f
    // (flt_82029F24, image-read: 0x3C23D70A). Ten milliseconds -- i.e. ONE frame of holding both
    // bumpers with every gate satisfied.
    f32                mfTimeSpentDoingCrashStartAction = 0.0099999998f;

    // X360 +284444 (0x4571C), DWARF :853. ✅ The post-mode LOCKOUT, and it counts DOWN despite the
    // name. Construct @0x82380388 and OnModeEnd @0x823767E0 both store 2.0f into it; while it is
    // above zero ShouldStartShowtimeMode decrements it, re-arms the hold and refuses. So showtime
    // cannot be re-entered for two seconds after any mode ends -- and cannot be entered for the
    // first two seconds of a session.
    f32                mfTimeSinceLastCrashMode         = 2.0f;

    // X360 +284448 (0x45720), DWARF :854. ✅ THE 0.5 s INTRO WINDOW. Two independent witnesses:
    // IsInShowtimeIntro @0x82356A60 is nothing but `*(this+284448) > 0.0f`, and
    // GetShowtimeIntroSteering @0x82356A90 asserts on exactly that expression. Opened to
    // flt_82CDB8D0 (0.5f, image-read) by the else arm, decremented every frame, and cleared to 0
    // when the arm takes its decision.
    f32                mfShowtimeIntroTimeLeft          = 0.0f;

    // X360 +284452 (0x45724), DWARF :855. ✅ The forced steering the intro applies, +/-1.0f
    // (flt_82001C98 / flt_820037C8, both image-read). GetShowtimeIntroSteering @0x82356A90 returns
    // exactly this word. Latched by the else arm from the SIGN of the player car's angular
    // velocity Y, so the car keeps spinning the way it was already turning.
    // ⓘ Construct does NOT seed it (the console only ever reads it behind IsInShowtimeIntro(), and
    // the arm always writes it before opening the window); zeroed here for determinism.
    f32                mfShowtimeIntroSteering          = 0.0f;

    // X360 +284464 (0x45730), DWARF :856. The heading the car had when the intro opened, cached as
    // a whole 16-byte vector (`stvx128 v0, r30, r10` with r10 == 0x45730, @0x8239A6C4) and
    // re-loaded each frame (`lvx128 v127, r30, r10`, @0x8239A7A8) to be dotted against the current
    // heading. When that dot goes non-positive -- the car has turned more than 90 degrees -- the
    // arm stops waiting and decides.
    Vector3            mShowtimeIntroOriginalDirection;

    // X360 +284480 (0x45740), DWARF :859. ✅ Which showtime bounce behaviour is selected.
    // UpdateShowtimeMode @0x82380EF8 cycles it `(x + 1) % 3` and posts the new value as the 4-byte
    // payload of game action 138 (E_ACTION_TOGGLE_SHOWTIME_BEHAVIOUR), whose consumer
    // VehicleManager::SetShowtimeBehaviour asserts `< 3` -- which is what pins both the type and
    // E_SHOWTIME_MODE_COUNT. ShouldStartShowtimeMode refuses while it is E_SHOWTIME_MODE_OFF.
    EShowtimeBehaviour meShowtimeBehaviour              = E_SHOWTIME_MODE_ON_SIXAXIS;   // Construct: 2

    // X360 +284510 (0x4575E), DWARF :857. "The car has been on the ground at some point during the
    // intro window." Cleared when the window opens (`stbx r28(0), r30, r9`, r9 == 0x4575E,
    // @0x8239A6C8) and OR-ed every frame with `!(mfTimeInAir > 0.0f)` (@0x8239A7A0..0x8239A7BC).
    // ⭐ IT IS THE LAST GATE BEFORE StartCrashMode: when the window closes with this byte still
    // clear the arm does NOT start showtime -- it re-arms the window to 9.99999975e-06f
    // (flt_82004884, image-read) and retries next frame. A car that is airborne for the whole
    // window waits until it lands.
    bool               mbShowtimeIntroHasTouchedGround  = false;

    // ⭐⭐⭐ X360 +284513 (0x45761). [bounce wave] THE SHOWTIME RISING-EDGE LATCH, and the reason
    // ~1900 instructions of showtime physics had never executed on this build.
    // UpdateRoadRulesManager @0x82381258 reads it (`lbzx r11, r31, r9`, r9 == 0x45761, @0x82381484)
    // and immediately re-stores the CURRENT in-showtime truth into it (`stbx r11, r31, r9`
    // @0x823814B8). The post of game action 42 (E_ACTION_IMPACT_TIME_START) fires on the frame the
    // mode type FIRST becomes E_MODE_OFFLINE_SHOWTIME/E_MODE_ONLINE_SHOWTIME and on no other frame.
    // ⭐ The neighbouring byte at +284512 is mbToggleShowtimeBehaviour (see the accessor's banner
    // above) -- a showtime bool pair, which is independent corroboration of the offset's meaning.
    // Starts false: the console's own initial state is "not in showtime", so the first entry into
    // showtime IS a rising edge, which is exactly what makes the first post happen.
    bool mbWasInShowtimeGameMode        = false;

    // =========================================================================================
    // ⭐⭐⭐ [showtime score wave 2026-08-29] THE SHOWTIME CRASH-CHAIN HAND-OFF, three members.
    // Names and types are the DecFIGS DWARF's verbatim (BrnGameStateModule.h:862/:863/:864);
    // every offset is a store or a load in ProcessContacts @0x8236BC68 or UpdateShowtimeMode
    // @0x82380EF8. As with the seven showtime members above, the members are NOT laid out at
    // the console offsets on this minimal slice -- the offsets are the identity proof.
    //
    // ⛔ THE STACK IS A HAND-OFF BETWEEN THE TWO HALVES OF THE FRAME, and that is the whole
    // reason "Cars Crashed" is a seven-hop chain rather than a one-hop one:
    //   POST-world  ProcessContacts       pushes the index of each newly-crashed traffic car
    //   PRE-world   UpdateShowtimeMode    pops ONE index every 2 frames (miShowtimePendingFrameDelay),
    //                                     posts game action 116 E_ACTION_TRAFFIC_TYPE_REQUEST with
    //                                     it, and parks it in muShowtimeRequestedTrafficIndex until
    //                                     the traffic module answers with a TrafficTypeResponse --
    //                                     only THEN does DealWithScoreForVehicleClass run and
    //                                     maiNumCarsCrashed move.
    // ⚠️ ONLY THE POST-WORLD HALF EXISTS TODAY. UpdateShowtimeMode is not reconstructed, so
    // nothing pops this stack yet: it fills to KI_MAX_SIMULTANEOUS_SHOWTIME_CRASHES and then
    // ProcessContacts' own IsFull() guard stops pushing -- which is the console's own behaviour
    // when the consumer is starved, not a leak.
    // =========================================================================================

    // DWARF BrnGameStateModule.h:10 -- `const int32_t KI_MAX_SIMULTANEOUS_SHOWTIME_CRASHES = 8`,
    // which is the `short_8_` template argument the X360's mangled Stack symbols carry.
    static const s32 KI_MAX_SIMULTANEOUS_SHOWTIME_CRASHES = 8;

    // X360 +284488 (0x45748), DWARF :862. ProcessContacts reaches it as `addis r28, r25, 4 ;
    // addi r28, r28, 0x5748` and calls Stack<u16,8>::IsFull / ::Push on it; UpdateShowtimeMode
    // calls ::Peek / ::Pop. Its "Stack used before Construct/Clear was called" assert (CgsStack.h
    // :177) is what identifies the container as CgsContainers::Stack and not a Set.
    CgsContainers::Stack<u16, KI_MAX_SIMULTANEOUS_SHOWTIME_CRASHES> mShowtimePendingTrafficIndexStack;

    // X360 +284484 (0x45744), DWARF :863. UpdateShowtimeMode's inter-request countdown: it
    // decrements this every frame the stack is non-empty and only pops when it reaches 0, then
    // re-seeds it to 2. Declared here with the stack it paces; its only writer lands with
    // UpdateShowtimeMode.
    s32 miShowtimePendingFrameDelay = 0;

    // X360 +284508 (0x4575C), DWARF :864. The index whose traffic-type answer is outstanding;
    // K_INVALID_VEHICLE_INDEX (0xFFFF) when none is. Declared here for the same reason.
    u16 muShowtimeRequestedTrafficIndex = 0xFFFFu;

public:
    // ⭐⭐⭐ [bounce wave] ONE ARM of X360 GameStateModule::UpdateRoadRulesManager @0x82381258,
    // staged at the console's own PreWorldUpdate position. See the body for the full arm
    // inventory and for which arms are deferred and why. Takes the action queue the caller
    // already holds the output buffer's write lock for, the same way every other ...BringUp leg
    // in GameStateModule_gUI_00.cpp does.
    void UpdateRoadRulesManagerImpactTimeBringUp(GameStateModuleIO::GameActionQueue* lpActionQueue);
};
}
