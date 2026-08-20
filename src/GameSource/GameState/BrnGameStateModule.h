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
    void PostWorldUpdateStuntBringUp(
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                                                      lpActiveRaceCarOutputInterface,
        const CgsModule::VariableEventQueue<1536, 16>* lpWorldGameEventQueue);

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
    void PreWorldUpdateStuntBringUp(f32 lfGameTimestep, bool lbIsAGameModeActive);

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
    //
    // lbMayCompleteJunkyardEntry gates the SECOND leg (the extracted ProcessGameEvents case-78
    // arm below). It is the ordering stand-in for game event 78: the caller passes the GUI's own
    // first-boot signal, and the body explains what firing without it measurably does.
    void PreWorldUpdateSetupPlayerCarBringUp(bool lbMayCompleteJunkyardEntry);

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
    // [FLAG PC bring-up] the console reaches this arm from game event 78, which
    // BridgeGuiToGameState @0x823DDB78 translates out of GUI out-event 145
    // (BrnGui::InGame::OnEnter). Both ends of that bridge exist in this tree, but the bridge is
    // not plumbed (nothing creates a GameStateModuleIO::PostWorldInputBuffer, and PreWorldUpdate's
    // three-queue merge is not reconstructed) AND -- measured on this build -- InGame::OnEnter
    // runs ~40 log lines BEFORE the latch is armed, so a faithfully-plumbed bridge would deliver
    // the event to a latch that does not yet exist. The arm therefore runs off the latch alone,
    // in the same sub-step as the arming leg -- which IS the console's own body order inside
    // PreWorldUpdate (latch leg @0x823A5510, ProcessGameEvents @0x823A58B8).
    // DELETE-WHEN ProcessGameEvents + the post-world input buffer + BridgeGuiToGameState's caller
    // are real (then this goes and the GUI's own event drives it).
    void ProcessGameEventsReallyEnterJunkyardBringUp(GameStateModuleIO::GameActionQueue* lpActionQueue);

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
};
}
