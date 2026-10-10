// b5-decomp/src/GameSource/GameState/GameStateModule_gUI_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the rest
// of the module's committed bodies are in BrnGameStateModule.cpp).
//
// THE gateui WAVE'S GameState-SIDE PLUMBING -- the bodies that carry a broken prop from the
// world module all the way to a game action the GUI bridge can translate:
//
//     world OutputBuffer::GetGameEventQueue()            (produced by PropEntityModule::
//                                                         ProcessContacts -> the bridge legs)
//        -> GameStateModule::PostWorldUpdate             (GameStateModule_wW_01.cpp: refreshes
//                                                         mLastActiveRaceCarInterface, appends
//                                                         into mGameEventCarryQueue, runs the
//                                                         mode manager's scorers and HUD pump)
//        -> PreWorldUpdateStuntBringUp                   (X360 PreWorldUpdate @0x823A5328)
//             * ProcessGameEvents                        (GameStateModule_ProcessGameEvents.cpp,
//                                                         case 111 -> StuntManager::OnPropHit)
//             * TriggerQueryManager::UpdateTriggers      (arms maActiveTriggers for NEXT frame)
//             * StuntManager::Update                     (consumes the latch ->
//                                                         ProcessStuntElement -> action 58)
//
// Each function's console attestation, and each deliberate deviation, is written out at its
// declaration in BrnGameStateModule.h and again at its body below. Nothing here is fabricated:
// every reduction is named as a reduction.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnGameStateModule.h"

#include <stdlib.h>                                                     // getenv ([UI-gate] diag)
#include <string.h>                                                     // memcpy / memset (the case-20 payload view)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<1536,16>

#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer (lock + GetGameActionQueue)
#include "GameSource/GameState/BrnGameStateTakedownCache.h"             // mpTakedownCache->mTakedownEventQueue (the post-world scoring leg)
#include "GameSource/GameState/BrnGameEvents.h"                         // RecordPropHitEvent / E_EVENT_RECORD_PROP_HIT / E_EVENT_CHANGE_WORLD_REGION
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h" // WorldRegionChangeEvent (the case-115 payload)
#include "GameSource/GameState/Offences/BrnStuntManager.h"              // StuntManager::OnPropHit / Update
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // UpdateTriggers / GetActiveTrigger*
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h" // the accessor's return type
#include "GameSource/GameState/BrnGameActions.h"                        // RankInfoResponseAction (the case-80 record)
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"            // GameStats (the case-79 record)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::GetGameStats
#include "SharedClasses/Progression/BrnProgressionData.h"                // ProgressionData::GetProgressionRankCount
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::GetNumRankWinsForGameMode
#include "GameSource/GameState/Progression/BrnDerivedCars.h"

#include "SharedClasses/Trigger/BrnTriggerData.h"                       // TriggerData::GetRegion
#include "SharedClasses/Trigger/BrnTriggerBase.h"                       // TriggerRegion::GetType
#include "SharedClasses/Trigger/BrnGenericRegion.h"                     // GenericRegion::Type (SMASH / OVERDRIVE_BOOST)

// ---- [D4 stuntrace WAVE D -- THE PUMP] ------------------------------------------------------
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::Pre/PostWorldUpdate, StartGameMode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // GameMode::GetCurrentState / GetIntroDurationSeconds
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // StartGameModeParams (the case-20 local)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"  // CgsSystem::TimerStatusInterface (the pump's new argument)
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsIDCompress ([car] BRN_DEBUG_PLAYER_CAR)
#include "SharedClasses/DataLists/VehicleList.h"                        // VehicleList::GetVehicleCount/GetVehicleData ([car])
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // VehicleListEntry::GetId/GetName/GetDefaultWheelName ([car])
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"          // CgsResource::ID::HashString ([car-audio] audit)
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"   // CarData::GetId ([car-audio] junkyard pick)
#include "SharedClasses/DataLists/WheelList.h"                          // WheelList::FindWheelIndexFromName/GetWheelData ([car])
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h" // [FX-GS2 G10-D11] GameStateToGuiInterface::AddOnTailEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // [FX-FLOW NEW-EMMTAIL] GameStateToNetworkInterface::SetActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // [FX-GS2 G10-D11] GetUsedCarsBitArray / GetRaceCar
#include "rw/math/vpu/vector3_operation.h"                              // [FX-GS2 G10-D11] Magnitude (the player's speed)
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h" // the case-17/18 arms
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // TrainingManager::ForceUnpause (case 17)
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"     // TakedownManager::ClearAllTakedowns (case 18)
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"       // MugshotManager::OnRoundStart (case 18)
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"       // PaybackManager::OnRoundStart (case 18)
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"            // [net] witness lines (PC harness)

namespace BrnGameState
{

// ARTIST 0x82397568: update progression, reset the model at the current location,
// then apply the selected car's saved palette and colour.
void GameStateModule::HandleChangePlayerCarEvent(
    const GameStateModuleIO::ChangePlayerCarEvent* lpEvent,
    GameStateModuleIO::GameActionQueue* lpActions)
{
    OnPlayerCarChange(lpEvent->mCarModelId, lpEvent->mWheelModelId, lpActions, true);
    GameStateModuleIO::ResetPlayerCarAction lReset = {};
    lReset.mCarModelId = lpEvent->mCarModelId;
    lReset.mWheelModelId = lpEvent->mWheelModelId;
    lReset.mePlayerScoringIndex = GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
    lReset.muReserved0x42 = lpEvent->mbResetPlayerCamera;
    lReset.mbKeepResetSection = lpEvent->mbKeepResetSection;
    lReset.mfDeformationAmount = mProgressionManager.GetProfile()->GetPlayerBaseDeformAmount(lpEvent->mCarModelId);
    lReset.miBaseDeformationType = lReset.mfDeformationAmount > 0.0f ? 1 : -1;
    lpActions->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReset),
                       GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR, sizeof(lReset));
    GameStateModuleIO::CarSelectChangeColourAction lColour;
    s32 liColour, liPalette;
    mProgressionManager.GetCarColourAndPalette(lpEvent->mCarModelId, &liColour, &liPalette);
    lColour.muPaletteIndex = liPalette;
    lColour.muColourIndex = liColour;
    lpActions->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lColour),
                       GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR, sizeof(lColour));
}

// ============================================================================
// â­ [gateui] GetDeveloperChallengeManager -- the body behind the declaration the StreetManager
// wave added with no member behind it.
//
// The console never emits an accessor for this subobject: every call site reaches it through the
// inlined `mpGameStateModule + 185712` pointer adjust, and both of them assert it non-null with
// the accessor spelled out --
//   StreetManager::ProcessNewRoadScore      @0x823496C8 ("mpGameStateModule->GetDeveloperChallengeManager()")
//   StuntManager::ProcessStuntElement       @0x8239CDB0 (same string, BrnStuntManager.cpp)
// De-inlined here over the real embedded member (BrnGameStateModule.h,
// mDeveloperChallengeManager), so no reconstructed body has to poke a byte offset.
// ============================================================================
DeveloperChallengeManager* GameStateModule::GetDeveloperChallengeManager()
{
    return &mDeveloperChallengeManager;
}

// ============================================================================
// â­ [P1 sim-pause] PostWorldInput -- the free-function accessor BridgeGuiToGameState posts
// through (console: returns the module's post-world input GameEventQueue). PC body: the CARRY
// QUEUE, the named reduction spelled out at the declaration (BrnGameStateModule.h).
// ============================================================================
namespace GameStateModuleIO
{
    CgsModule::VariableEventQueue<1536, 16>* PostWorldInput(GameStateModule* lpModule)
    {
        return &lpModule->mGameEventCarryQueue;
    }
}

// ============================================================================
// â­â­ [gateui] PreWorldUpdateStuntBringUp -- the three stunt-chain legs of the console's
// PreWorldUpdate @0x823A5328, IN THE CONSOLE'S OWN ORDER. The header carries the line-by-line map
// of the source function and both named reductions; the body annotates each leg again.
// ============================================================================
void GameStateModule::PreWorldUpdateStuntBringUp(
        f32 lfGameTimestep, bool lbIsAGameModeActive,
        const CgsSystem::TimerStatusInterface& lrTimerStatusInterface)
{
    if (mpOutputBuffer == 0)
    {
        return;
    }

    // The console holds the output buffer's write lock across this whole span of PreWorldUpdate:
    // TriggerQueryManager::UpdateTriggers publishes add/remove-trigger events onto the buffer's
    // trigger-management input interface, and StuntManager::Update AddEvents onto its game-action
    // queue. Same bracket here -- the idiom the other extracted PreWorldUpdate legs in
    // BrnGameStateModule.cpp already use.
    mpOutputBuffer->LockForWrite();
    GameStateModuleIO::GameActionQueue* lpActionQueue = mpOutputBuffer->GetGameActionQueue();
    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue != NULL");   // BrnGameStateModule.cpp:1149
    mbIsUpdating = true;

    // The console's sim-step latch, near the top of PreWorldUpdate: mfSimTimeStep = 0, then the
    // pre-world input buffer's sim timer step (base * multiplier). [FLAG PC bring-up] read from
    // the frame's timer interface, the same named deviation leg 1b carries (nothing on PC fills
    // the input buffer's timer block). Readers: UpdateRoadRulesManager, StreetManager::Update.
    mfSimTimeStep = lrTimerStatusInterface.GetSimTimerStatus()->GetCurrentTimeStep();

    // ---- -1) THE ONLINE CRASH APPEND (console 0x823A5544..0x823A55A0) -------------------------
    // ⭐ [FX-GS2 2026-09-23, crash-parity G11-D5] Straight after the setup-player-car one-shot
    // (0x823A5510..0x823A5540, PreWorldUpdateSetupPlayerCarBringUp here) and before the drive-thru
    // tick below (0x823A56C0), the console does, in ANY frame:
    //     lwz  r11, 0xD98(ModeManager)  ; mpCurrentGameMode, NULL -> skip
    //     lbz  r11, 0xAC(r11)           ; GameMode::mbIsOnline, 0 -> skip
    //     bl   GetPlayerActiveRaceCarIndex
    //     bl   0x8231D8A8 (write-locked GetGameStateToGuiInterface) ; stw r28, 0(r11)
    //                                   ; the inlined SetPlayerRaceCarIndex
    //     bl   0x8231D8A8 ; addi r4 = gsm + 0x3D1A0 (the cached post-world crash queue)
    //     bl   GameStateToGuiInterface::AppendRaceCarCrashes @0x82379980
    // Online-only, and write-only: nothing in the image reads the interface's crash queue or its
    // player index back (TranslateGuiInterfaceToGuiEvents walks +4..+0x160 only), so this is a
    // completeness leg with no observable effect, on the console as here.
    // [FLAG PC] gsm+0x3D1A0 is the takedown lane's heap cache on this build (see the rumble leg
    // below): a missing cache skips the append rather than handing the callee a NULL it would
    // dereference -- the same guard CheckForTailingRivals' call site stands behind.
    {
        const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        if (lpCurrentGameMode != 0 && lpCurrentGameMode->IsOnline() && mpTakedownCache != 0)
        {
            const s32 liPlayerRaceCarIndex = static_cast<s32>(GetPlayerActiveRaceCarIndex());
            mpOutputBuffer->GetGameStateToGuiInterface()->SetPlayerRaceCarIndex(liPlayerRaceCarIndex);
            mpOutputBuffer->GetGameStateToGuiInterface()->AppendRaceCarCrashes(
                &mpTakedownCache->mRaceCarCrashEventQueue);
        }
    }

    // ---- 0) DRIVE-THRU TICK (console #? -- PreWorldUpdate @0x823A5328 pseudocode line 220) ---
    // ⭐⭐⭐ [drive-thru wave 2026-08-27] DriveThruManager::Update @0x8239EEF0. This is the leg that
    // turns a latched drive-thru into its game action: HandleDriveThru (leg 2b below) only CACHES
    // the region type in meDriveThruCache; Update is what dispatches it through ProcessDriveThru
    // and posts action 100 / 97 / 98. It also ages the 46 activation timers.
    //
    // ⚠️ POSITION IS THE CONSOLE'S AND IT IS DELIBERATELY *BEFORE* THE TRIGGER FAN-OUT. The
    // console's own body order is
    //     220  DriveThruManager::Update            <-- HERE
    //     252  ProcessGameEvents
    //     305  CopyScoringDataToOutput
    //     310  TriggerQueryManager::PreWorldUpdate  <-- calls HandleDriveThru (leg 2b)
    //     332  StuntManager::Update
    // so a region entered on frame N is PROCESSED ON FRAME N+1 -- the same one-frame deferral the
    // game-event carry queue uses. Moving this call after leg 2b would "fix" a latency that is the
    // console's own and is what gives the presentation timer a frame to arm. Do not reorder.
    //
    // ARGUMENTS FROM THE ASM (0x823A56C0..0x823A5708), not from the pseudocode's numbering: the
    // f32 timestep goes in f1 and consumes the r6 slot, so the integer args are
    // r4,r5,[r6 skipped],r7,r8,r9,r10 then stack. Pinned by the last stack slot, which the console
    // loads from r31+0x456E8 == the module's mpVehicleList == this call's trailing argument.
    //
    // [FLAG PC bring-up] TWO arguments are PC derivations, named rather than hidden:
    //   * lbIsFreeburn -- the console reads it off the current game mode; no accessor for it
    //     exists in this tree. Derived as "no game mode is running", which is what freeburn IS on
    //     this build. It gates ONLY mbPlayerCanUseJunkyards, not the shop path.
    //   * lbIsInJunkyard / lbInviteInProgress -- both false on this offline build: nothing here
    //     runs the junkyard-occupancy latch or the invite manager.
    // The rest are real: IsOnlineGameMode(), IsShowtimeGameMode() and IsSimPaused() are committed
    // X360 reconstructions on this module, and the vehicle list is the console's own +0x456E8.
    //
    // THE TIMER INTERFACE IS THE CONSOLE'S OWN SLOT, reached through a cast rather than a fake.
    // The console passes OutputBuffer::GetTimerRequest(lpOutput); this tree's OutputBuffer models
    // that member as the opaque `OutputBufferTimerRequestInterface { u8 maOpaque[16]; }`
    // (BrnGameStateModuleIO.h:279) only because nothing had needed its shape yet. It IS a
    // CgsSystem::TimerRequestInterface: that type is two TimerRequests (each {u32 muFlags;
    // f32 mfMultiplier}) at +0 and +8, i.e. exactly 16 bytes, and the storage slot is exactly 16
    // bytes at +16420. So this is a re-type of the same object at the same address, not a
    // substitute -- the identical move, and the identical justification, as AsActionQueue() in
    // BrnDriveThruManager.cpp. DELETE-WHEN BrnGameStateModuleIO.h declares the slot's real type.
    mDriveThruManager.Update(
        lpActionQueue,
        reinterpret_cast<CgsSystem::TimerRequestInterface*>(
            mpOutputBuffer->GetTimerRequestInterface()),
        lfGameTimestep,
        &mLastActiveRaceCarInterface,
        IsOnlineGameMode(),
        /*lbIsFreeburn*/ !lbIsAGameModeActive,
        IsShowtimeGameMode(),
        IsSimPaused(true, false),
        /*lbIsInJunkyard*/ false,
        /*lbInviteInProgress*/ false,
        GetVehicleList());

    // ---- 0a) THE PAYBACK MANAGER'S INPUT COPY (console 0x823A572C) ----------------------------
    // ⭐ [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING] GameStateModule::CopyInputDataToPaybackManager
    // @0x8239AA78 (r4 = the pre-world input buffer), unconditionally, straight after the drive-thru
    // tick and before the rumble producers below: the frame's timer block and the dirty-trick press
    // reach PaybackManager before its Update (in the takedown leg) reads them.
    // THE READ LOCK IS THE CONSOLE'S [FX-FLOW 2026-09-24, review D]: PreWorldUpdate @0x823A5328 takes
    // `IOBuffer::LockForRead(lpInput)` @0x823A542C (r30 == its r6, the pre-world input buffer) and
    // holds it past this call to `UnlockForRead` @0x823A5D7C; the accessor the copy reads through,
    // PreWorldInputBuffer::GetTimerStatusInterface @0x8231CE28, asserts "Not locked for reading"
    // without it. On this build the pre-world buffer is the module's stand-in, which PreWorldUpdate
    // does not lock as a whole, so the same lock is taken around this leg -- the move every other
    // pre-world leg here makes. No null test on the buffer: the console asserts `lpInput != NULL`
    // (@0x823A53E0, :1130) rather than branching, and the stand-in is allocated by
    // GameStateModule::Construct and freed only by Destruct.
    mpPreWorldInputBuffer->LockForRead();
    CopyInputDataToPaybackManager(mpPreWorldInputBuffer);
    mpPreWorldInputBuffer->UnlockForRead();

    // ---- 0b) THE RUMBLE PRODUCERS (console #55) ----------------------------------------------
    // ⭐ [FX-RUMBLE 2026-09-22, crash-parity G10-D1] RumbleManager::Update @0x82386A98. X360
    // PreWorldUpdate @0x823A5328 `bl` #55 (0x823A5800), straight-line (no branch in
    // 0x823A5328..0x823A5800 skips it), OUTSIDE the IsSimPaused block, right after
    // TrainingManager::Update (#54) and BEFORE the event merge + ProcessGameEvents (#57..#68):
    //     r3 = gsm+46680 (&mRumbleManager)          r4 = r23 = gsm+235488 (&mLastActiveRaceCarInterface)
    //     r5 = gsm+250272 (the crash-queue cache)   r6 = *(gsm+208304) (mePlayerActiveRaceCarIndex)
    //     r7 = gsm+250800 (&mContactSpyInterface)   f1 = f31 (the frame's game timestep)
    // ⓘ POSITION: this pump has no TrainingManager leg (that one runs later, from
    // PreWorldUpdateTrainingBringUp -- its own documented deviation); what the rumble needs is its
    // console order against its NEIGHBOURS, which this seat keeps: it runs before this frame's
    // case-31 arm posts the race-car-impact jolt (so the queue order is the console's) and before
    // UpdatePauseState (#88, below), so it reads the PREVIOUS frame's mbRumblePaused, as the
    // console does.
    // [FLAG PC] the crash-queue argument is the takedown lane's heap cache of gsm+250272; the
    // console body never reads it (r5 is overwritten before its first call), so a missing cache
    // passes NULL rather than inventing one.
    mRumbleManager.Update(&mLastActiveRaceCarInterface,
                          mpTakedownCache != 0 ? &mpTakedownCache->mRaceCarCrashEventQueue : 0,
                          mePlayerActiveRaceCarIndex,
                          &mContactSpyInterface,
                          lfGameTimestep);

    // ---- 1) the merged queue (console #57..#62) -----------------------------------------------
    // The console Constructs a LOCAL GameEventQueue and Appends THREE sources into it -- the carry
    // queue (+248384), the PreWorldInputBuffer's queue, and the InviteManager's (+2032) -- then
    // Clears the carry queue; ProcessGameEvents (leg 1' below) walks that local queue.
    // The pre-world input buffer is staged every sub-step (BrnGameModule re-Constructs it and
    // runs BridgeNetworkToGameState into it before this pump), and its game-event queue is where
    // the network posts the online-game events (17 start game, 18 start round, player
    // added/removed, ...), so it is merged here in the console's position: after the carry queue.
    // REDUCED by one source: the InviteManager's queue is never written on this build (its
    // Update does not run), so appending it would add nothing.
    GameStateModuleIO::GameEventQueue lGameEventQueue;
    lGameEventQueue.Construct();
    lGameEventQueue.Append(mGameEventCarryQueue);
    if (mpPreWorldInputBuffer != 0)
    {
        mpPreWorldInputBuffer->LockForRead();
        const GameStateModuleIO::PreWorldInputBuffer* lpcPreWorldInputBuffer = mpPreWorldInputBuffer;
        lGameEventQueue.Append(*lpcPreWorldInputBuffer->GetGameEventQueue());
        mpPreWorldInputBuffer->UnlockForRead();
    }
    mGameEventCarryQueue.Clear();

    // ---- 1-) THE SHOWTIME TRAFFIC HAND-OFF (console #65) --------------------------------------
    // ⭐⭐⭐ [FX-SHOWTIME2 2026-09-24] GameStateModule::UpdateShowtimeMode @0x82380EF8. X360
    // PreWorldUpdate @0x823A5328 `bl` #65 @0x823A5888, UNCONDITIONAL (no branch in
    // 0x823A56AC..0x823A58CC), bracketed by its own PerfMonCpu monitor (this+0x475CC), straight
    // after the event merge and the carry-queue Clear (#57..#62) and before ProcessGameEvents (#68):
    //     r4 = r30 (the pre-world input buffer)     r5 = r29 (the output buffer)
    //     r6 = gsm+250800 (&mContactSpyInterface)   r7 = gsm+278480 (the TrafficTypeResponse<32> cache)
    // It pops the crashed-traffic stack ProcessContacts fills into action 116 and scores last
    // frame's answer into maiNumCarsCrashed + action 140 -- the whole showtime per-car score.
    // The response cache is the takedown lane's heap copy of gsm+278480 on this build (filled by
    // GameStateModule::PostWorldUpdate); ConstructTakedownBringUp allocates
    // it in Construct and only Destruct frees it, so it is passed without a test, as the console
    // passes its embedded queue. The output buffer's write lock is already held (top of this leg).
    UpdateShowtimeMode(mpPreWorldInputBuffer, mpOutputBuffer, &mContactSpyInterface,
                       &mpTakedownCache->mTrafficTypeResponseQueue);

    // ---- 1') THE GAME-EVENT DISPATCHER (console #68) ------------------------------------------
    // GameStateModule::ProcessGameEvents (GameStateModule_ProcessGameEvents.cpp): one walk over the
    // merged queue, every event answered in arrival order through the dispatcher's switch, then the
    // prop-progression tail (action 199). The console holds the pre-world input buffer's read lock
    // across the whole of PreWorldUpdate; the dispatcher copies its timer block and several arms
    // read it, so the lock is taken around the call here as around this pump's other pre-world
    // reads. The output buffer's write lock is already held (top of this leg).
    //
    // ---- 1'') THE CAR-SELECT TICK (console, straight after ProcessGameEvents) -------------------
    // CarSelectManager::Update, the image's only caller of it, gated on the player being in a
    // junkyard (the console's 64-bit test of mJunkyardId): it is what ends the junkyard
    // transition-in and moves the director's junkyard state on. It runs after the dispatcher, so a
    // car-select event answered this frame (78 completes the entry, 94 starts / modifies / exits,
    // 9 ends a car change) reaches the same frame's tick. The timestep is the game timer's, the
    // same value the console latches near the top of PreWorldUpdate.
    mpPreWorldInputBuffer->LockForRead();
    {
        const GameStateModuleIO::PreWorldInputBuffer* lpcPreWorldInputBuffer = mpPreWorldInputBuffer;
        ProcessGameEvents(&lGameEventQueue, lpActionQueue, lpcPreWorldInputBuffer, mpOutputBuffer);

        if (mCarSelectManager.IsInJunkyard())
        {
            mCarSelectManager.Update(lpActionQueue, lpcPreWorldInputBuffer->GetControllerInput(), lfGameTimestep);
        }
    }
    mpPreWorldInputBuffer->UnlockForRead();

    // ---- 1a) THE TAKEDOWN FEED (console: the `if (!IsSimPaused)` block between #68 and #86) --
    // ⭐⭐⭐ [road-rage wave, agent C] GameStateModule::ProcessTakedownEvents @0x8238FC50. X360
    // PreWorldUpdate @0x823A5328, after ProcessGameEvents / CarSelectManager / OnlineCarSelect /
    // ImageManager::PreWorldUpdate and BEFORE EmmPreWorldUpdate (leg 1b below), runs -- under the
    // IsSimPaused(this,1,0) it computed at the top --
    //     CrashingRaceCarInterface::SetFromVehicleOutputInterface(...)
    //     TakedownManager::Update(gsm+568, ...)                           [X] not reconstructed
    //     *(gsm+249944) = 0;  TakedownEvent_::Append(gsm+249936, lpOutput->GetTakedownEventOutputQueue())
    //     MugshotManager::Update(...)  /  PaybackManager::Update(...)     [X] not staged (other lanes)
    // [FLAG PC 2026-09-02, verify V2] THE QUEUE THIS DRAINS IS NEVER FED ON THIS BUILD: the console
    // producer is TakedownManager::Update @0x8239FAC0 (gsm+568; DetectTakedowns -> ProcessTakedownEvent
    // @0x82393D40 -> TakedownEventOutputQueue @+0x4040), none of which is reconstructed. A run showing
    // 0 takedowns is that hole, not this drain. DELETE-WHEN TakedownManager lands.
    //     ProcessTakedownEvents(this, lpActionQueue, gsm+249936, lpOutput)   <-- THIS CALL
    // POSITION IS THE CONSOLE'S: it must run after the game-event drain (a takedown's UI/GUI
    // events are the same frame's) and before the ModeManager tick, which reads the road-rage
    // counter OnPlayerDoesATakedown just advanced (HasBeatenRoadRageTarget in UpdateCurrentMode).
    //
    // [FLAG PC bring-up] THE QUEUE IS THE OUTPUT BUFFER'S OWN, NOT THE MODULE'S COPY. The console
    // Clear()s its module-owned EventQueue<TakedownEvent,8> at gsm+249936 and Appends the output
    // buffer's takedown-event output queue into it every frame, then hands the COPY to
    // ProcessTakedownEvents. This tree does not model gsm+249936, so the source queue is passed
    // directly -- byte-identical content, one copy fewer. DELETE-WHEN the module queue is
    // modelled (the Mugshot / Payback legs will need it). The write-lock accessor is the
    // console's own (`GameStateModuleIO::Out` @0x82362B80 asserts "Not locked for writing", and
    // this function holds that lock); the cross-home cast is the same one BrnGameModule.cpp:1788
    // carries for the same forward-declared TakedownEventOutputQueueType, and the target type is
    // proven by the console's own TakedownEvent_::Append on it.
    // [takedown wave 2026-09-02] the whole !IsSimPaused takedown leg now lives in
    // GameStateModule_gTD_00.cpp (TakedownManager::Update -> the module-queue copy ->
    // ProcessTakedownEvents, in console order); the direct drain that stood here moved with it.
    // (Called on every frame: the leg clears its queues unconditionally, as the console does before
    //  its IsSimPaused branch, and only ticks the manager when not paused -- verify V3.)
    TakedownPreWorldLeg(lpActionQueue, lfGameTimestep, lrTimerStatusInterface, IsSimPaused(true, false));

    // ---- 1b) THE MODE MANAGER'S PRE-WORLD TICK (console #86) ---------------------------------
    // â­â­â­ [D4 stuntrace WAVE D] X360 PreWorldUpdate @0x823A5328 reaches ModeManager through ONE
    // hop, and this is that hop de-inlined:
    //     0x823A5A9C  bl GameStateModule::EmmPreWorldUpdate      (#86)
    //       @0x8238EF50, whose own body:
    //         v22 = PreWorldInputBuffer::GetTimerStatusInterface(a2);
    //         *(a1 + 208328 .. +208372) = v22[0..11]                 ; the 48-byte timer copy
    //         if (IsSimPaused(a1,1,0))  ModeManager::PausedUpdate(a1 + 4128, a3);
    //         else                      ModeManager::PreWorldUpdate(a1 + 4128, a3 /*out*/,
    //                                       a2 /*in*/, a1 + 208328 /*timer*/,
    //                                       GetPlayerActiveRaceCarIndex(a1),
    //                                       GetPlayerGlobalRaceCarIndex(a1),
    //                                       a5 /*isOnline byte, gsm+0x3C041*/, <action queue>, ...);
    // POSITION IS THE CONSOLE'S: after ProcessGameEvents (#68), before TriggerQueryManager::
    // PreWorldUpdate (#93). Do not move it below the trigger legs.
    //
    // âš  gsm+4128 (0x1020) IS mModeManager, and it is reached BY NAME through GetModeManager()
    // here -- never as an offset. (The stale campaign note "ModeManager is embedded at gsm+46640"
    // is wrong; +46640 is mTrainingManager.)
    //
    // â›” THE READ LOCK IS LOAD-BEARING. ModeManager::PreWorldUpdate calls
    // lpPreWorldInputBuffer->GetPlayerStatusInterface() and ->GetNetworkPlayerResultsInterface(),
    // and BOTH are the read-lock halves ("Not locked for reading", BrnGameStateModuleIO.h:147/149).
    // The console holds IOBuffer::LockForRead over the whole span (@0x823A5328 `bl` #16). Without
    // the bracket those two accessors assert every frame the moment a mode starts. They are the
    // ONLY buffer derefs in the function and both sit inside `if (mpCurrentGameMode != NULL)`, so
    // in free-burn the buffer is never touched -- which is why this leg is safe to run always.
    //
    // [FLAG PC bring-up] FOUR named deviations, none of them silent:
    //   (a) THE TIMER INTERFACE comes from the caller (BrnGameModule::mTimerStatusInterface, filled
    //       every sub-step by the console's own TimerStatusInterface::StoreTimers at
    //       BrnGameModule.cpp:1411) instead of from the 48-byte copy out of the PreWorldInputBuffer.
    //       Nothing on PC fills that buffer's timer block, so the console route would hand
    //       ModeManager an all-zero interface and every mode clock, the countdown and the mode
    //       timer would stand still. Same data, one copy earlier. See the header for the full note.
    //   (b) Global and active car inputs now both use their original cached snapshots,
    //       filled together after the previous world update (EmmPreWorldUpdate 0x8238F128).
    //   (c) lbPaused is false. The console's tenth argument is a stacked byte the IDA export
    //       renders as register residue (v50..v64), and EmmPreWorldUpdate reaches PreWorldUpdate
    //       only down its NOT-sim-paused arm, so "paused" here is not the sim pause. Not guessed
    //       at a value it might have had; passed the arm's own falsity.
    //   (d) [X] ModeManager::PausedUpdate (the console's sim-paused arm) is PARKED: it has no
    //       declaration and no body anywhere in the tree. While the sim is paused this leg is
    //       skipped entirely, which is what the console does with PreWorldUpdate on that arm --
    //       what is lost is PausedUpdate's own work, not this one's.
    if (mpPreWorldInputBuffer != 0 && !IsSimPaused(true, false))
    {
        mpPreWorldInputBuffer->LockForRead();
        mModeManager.PreWorldUpdate(
            mpOutputBuffer,
            mpPreWorldInputBuffer,
            lrTimerStatusInterface,
            GetPlayerActiveRaceCarIndex(),
            static_cast<::EGlobalRaceCarIndex>(GetPlayerGlobalRaceCarIndex()),
            IsOnlineGameMode(),
            lpActionQueue,
            &mLastGlobalRaceCarInterface,
            &mLastActiveRaceCarInterface,
            /*lbPaused -- FLAG (c)*/ false);
        mpPreWorldInputBuffer->UnlockForRead();
    }

    // ---- 1c) THE SECOND LEG OF THE SAME HOP (console #86, EmmPreWorldUpdate's own tail) -------
    // â­â­â­ [bounce wave] EmmPreWorldUpdate @0x8238EF50 does not stop at ModeManager. Its `bl`
    // stream continues:
    //     0x8238F168  bl ModeManager::PreWorldUpdate            <- leg 1b above
    //     0x8238F170  bl PerfMonCpu::StopMonitor
    //     0x8238F198  bl PerfMonCpu::StartMonitor
    //     0x8238F1A0  bl GameStateModuleIO::PreWorl<dInputBuffer::Get...>
    //     0x8238F1B0  bl GameStateModule::UpdateRoadRulesManager    <- THIS LEG
    // and the console guards it with `if (!IsSimPaused)` -- the SAME arm test leg 1b sits on,
    // which is why it is staged immediately after it and inside nothing new.
    //
    // âš ï¸ IT IS ITS OWN `if`, NOT AN `else`. In the console the ModeManager call sits inside
    // `if (IsSimPaused) PausedUpdate else PreWorldUpdate`, and THEN a separate
    // `if (!IsSimPaused) { UpdateRoadRulesManager }` follows. Both arms are the not-paused arm,
    // so the observable order and gating are identical either way; kept as a separate statement
    // so the shape matches the binary rather than reading as an else-branch that is not there.
    //
    // â­ WHY THIS LEG EXISTS AT ALL: its action-42 post is the ONLY producer of impact time in
    // the entire image, and without it VehiclePhysics::UpdateCrashing's aftertouch gate never
    // opens, so RaceCarPhysics::UpdateShowtimePhysics -- the whole P6 bounce chain -- never runs.
    // It is also the per-frame tick of the road rules (RoadRulesManager::Update). Body in
    // GameStateModule_RoadRules.cpp.
    //
    // The console's arguments: the output buffer and the pre-world input buffer's
    // ControllerInput (PreWorldInputBuffer::GetControllerInput, a read-lock accessor; the console
    // holds the read lock across the whole of PreWorldUpdate, here it is taken around the call as
    // leg 1b does). Construct allocates the buffer, so it is never null here.
    if (!IsSimPaused(true, false))
    {
        mpPreWorldInputBuffer->LockForRead();
        const GameStateModuleIO::PreWorldInputBuffer* lpcPreWorldInputBuffer = mpPreWorldInputBuffer;
        UpdateRoadRulesManager(mpOutputBuffer, lpcPreWorldInputBuffer->GetControllerInput());
        mpPreWorldInputBuffer->UnlockForRead();
    }

    // ---- 1c') THE REST OF THE SAME HOP: EmmPreWorldUpdate's TAIL (0x8238F1BC..0x8238F33C) ------
    // [FX-FLOW 2026-09-24, NEW-EMMTAIL] Both arms of the IsSimPaused test above (the paused
    // `bne loc_8238F1BC` @0x8238F188 and the fall-through after UpdateRoadRulesManager) meet at
    // 0x8238F1BC, so the tail is unconditional. Body and map at the definition below.
    EmmPreWorldUpdateTailBringUp(lrTimerStatusInterface);

    // (merge 2026-08-27: both waves added a leg at this seam the same day -- the bounce wave's
    // 1c above is EmmPreWorldUpdate's own tail; the scoring publish below runs AFTER
    // EmmPreWorldUpdate returns, per its console position. Both kept, console order.)

    // ---- 1d) PUBLISH THE SCORING SNAPSHOT (console #(RumbleManager::UpdatePauseState + 1)) ----
    // â­â­â­ [A9 scoring-feed wave 2026-08-27] GameStateModule::CopyScoringDataToOutput @0x8236CDC0.
    //
    // POSITION IS THE CONSOLE'S, and it is exact. GameStateModule::PreWorldUpdate @0x823A5328 --
    // this function's source and CopyScoringDataToOutput's SOLE xref-to -- runs it here:
    //     bl GameStateModule::EmmPreWorldUpdate            (#86; the ModeManager tick above)
    //     bl RumbleManager::UpdatePauseState
    //     bl CgsDev::PerfMonCpu::StartMonitor(*(this+292348))
    //     bl GameStateModule::CopyScoringDataToOutput(this, lpOutput)     <-- THIS CALL
    //     bl CgsDev::PerfMonCpu::StopMonitor(*(this+292348))
    //     if (a6 & 8) { bl TriggerQueryManager::PreWorldUpdate ... }      <-- leg 2 below
    // i.e. AFTER the mode tick and BEFORE the trigger legs. Do not move it: ModeManager::
    // PreWorldUpdate is what advances mStartTime/mEndTime and the per-car score records this
    // publishes, so running the copy first would publish a one-frame-stale snapshot.
    //
    // â“˜ UNCONDITIONAL, deliberately. The console's ModeManager tick sits inside
    // EmmPreWorldUpdate's not-sim-paused arm (and here inside the same guard, plus the PC-only
    // `mpPreWorldInputBuffer != 0`), but this call is OUTSIDE it -- while the sim is paused the
    // console still republishes the last scoring state every frame, which is what keeps the HUD
    // clock showing its frozen value instead of collapsing to zero.
    //
    // â“˜ The write lock this function already holds is the console's own
    // (`IOBuffer::LockForWrite(lpOutput)` at PreWorldUpdate's top) and it is required: the two
    // scoring-interface accessors CopyScoringDataToOutput goes through are write-lock asserted.
    // The mbIsUpdating bracket is required too -- GetPlayerActiveRaceCarIndex() and
    // IsOnlineGameMode() both assert it.
    //
    // [FLAG PC bring-up] the TimerStatusInterface argument is the ONE deviation, the same one this
    // function's own ModeManager call carries and for the same measured reason (nothing on PC
    // fills the module's copy of the PreWorldInputBuffer timer block at gsm+208328, which is where
    // the console reads its "now" from). Fully written up at the declaration in
    // BrnGameStateModule.h. DELETE-WHEN DoUpdate_GameStatePreWorld stages a real
    // PreWorldInputBuffer whose timer block is filled.
    //
    // ⭐ [FX-RUMBLE 2026-09-22, crash-parity G10-D1] ...AND THE CALL THE MAP ABOVE NAMES FIRST:
    // RumbleManager::UpdatePauseState @0x8236E728, `bl` #88 (0x823A5AC4), between
    // EmmPreWorldUpdate (#86) and this function's StartMonitor. Its arguments, from the asm:
    //     r4 = (*(gsm+232288) != 0)   lwz 0(r14) ; cntlzw ; extrwi 1,26 ; xori 1  (0x823A5AA0..AB0)
    //          == miSimPauseFlags != 0 -- the RAW pause word, not IsSimPaused's online-masked answer
    //     r5 = GameStateModuleIO::OutputBuffer::GetGameActionQueue(lpOutput)  (0x8231D4B8, the
    //          same accessor the top of this function already called for lpActionQueue)
    // It is the only writer of mbRumblePaused / mbGameWasPaused after Construct, and the gate
    // Update's crash jolt and UpdateImpacts read.
    mRumbleManager.UpdatePauseState(miSimPauseFlags != 0, lpActionQueue);

    CopyScoringDataToOutput(mpOutputBuffer, lrTimerStatusInterface);

    // ARTIST823A5328 calls the complete pre-world trigger pass here, after
    // ProcessGameEvents and before StuntManager consumes its gameplay latches.
    mTriggerQueryManager.PreWorldUpdate(mpPreWorldInputBuffer, mpOutputBuffer,
        &mStuntManager, &mDriveThruManager, &mLastActiveRaceCarInterface, GetVehicleList());

    // [DIAG] NOT IN THE X360 BINARY. Rung 0 of the `[UI-gate]` ladder, one-shot on the first frame
    // the armed set is non-empty: how many armed regions are SMASH (generic-region sub-type 8) and
    // BILLBOARD (sub-type 12). This is the line that separates "the prop was outside every smash
    // region" from "the trigger pump never ran" -- the wave's known blocker. Same logger and same
    // env guard (BRN_PROP_DIAG) as the "[prop-diag] BREAK" rung this ladder hangs off.
    //
    // â“˜ ROUND-7 NOTE -- READ THIS BEFORE DRAWING A CONCLUSION FROM THIS LINE. It is a ONE-SHOT and
    // it fires at the FIRST non-empty set, which on a junk-yard start is the junk-yard interior:
    // run 9 printed `armed smash=0 billboard=0 of=3` (BrnGame.log:863) and that line says NOTHING
    // about what was armed later. The per-rebuild timeline that does answer that question lives in
    // BrnTriggerQueryManager.cpp :: UpdateTriggers (`[UI-gate] trig rebuild #<n> pos=(...)`), which
    // reports every rebuild of the set with the position it was keyed on. Use that rung, not this
    // one, to decide whether a given gate's region was armed when the gate broke.
    {
        static bool       sbArmedLogged = false;
        static const bool sbDiag        = (getenv("BRN_PROP_DIAG") != 0);
        const u32         luArmedCount  = mTriggerQueryManager.GetActiveTriggerCount();
        if (sbDiag && !sbArmedLogged && luArmedCount > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            sbArmedLogged = true;
            const BrnTrigger::TriggerData* lpTriggerData = mTriggerQueryManager.GetTriggerData();
            s32 liSmash     = 0;
            s32 liBillboard = 0;
            if (lpTriggerData != 0)
            {
                for (u32 lu = 0; lu < luArmedCount; ++lu)
                {
                    const BrnTrigger::TriggerRegion* lpRegion =
                        lpTriggerData->GetRegion(mTriggerQueryManager.GetActiveTrigger(lu));
                    if (lpRegion->GetType() != BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
                    {
                        continue;
                    }
                    const BrnTrigger::GenericRegion* lpGeneric =
                        static_cast<const BrnTrigger::GenericRegion*>(lpRegion);
                    if (lpGeneric->GetType() == BrnTrigger::GenericRegion::E_TYPE_SMASH)
                    {
                        ++liSmash;
                    }
                    else if (lpGeneric->GetType() == BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_BOOST)
                    {
                        ++liBillboard;
                    }
                }
            }
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] armed smash=" << liSmash
                << " billboard=" << liBillboard
                << " of=" << luArmedCount << "\n";
        }
    }

    // ---- 2c) JUNCTION DETECTION + THE START ARM (console #96 and #98) ------------------------
    // â­â­â­ [D4 stuntrace WAVE D] The two D3-owned functions, staged in the console's own body
    // order. From PreWorldUpdate @0x823A5328's `bl` stream:
    //     #93   TriggerQueryManager::PreWorldUpdate            <- the two legs immediately above
    //     #95   ProgressionManager::PreWorldUpdate             <- [X] NOT STAGED (see below)
    //     #96   GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent   @0x82390418
    //     #97   GameStateModule::SendSetUpAllDriveThrusMessage  <- [X] NOT STAGED (see below)
    //     #98   GameStateModule::DetectModeStarts               @0x8239A428
    //     #103  StuntManager::Update                            <- the leg immediately below
    // Both take (r3 = this, r4 = r30, r5 = r29), and 0x823A5B2C..0x823A5B3C pins that pair as
    // (lpPreWorldInputBuffer, lpOutputBuffer) -- the identical r30/r29 the TriggerQueryManager call
    // four instructions earlier takes as its "in, out".
    //
    // âš  THE ORDER IS LOAD-BEARING IN BOTH DIRECTIONS.
    //   * #96 runs AFTER #93 because it reads TriggerQueryManager::mbPlayerInTrafficLightRegion /
    //     mPlayerCurrentTrafficLightId, and GetPlayerCurrentTrafficLightId() asserts
    //     IsPlayerInTrafficLightRegion() -- so it must see THIS frame's light-region state.
    //   * #98 runs AFTER #96 because ShouldStartSnapRaceMode gates on the junction cache #96 fills;
    //     with the two swapped the first frame of a hold would test last frame's junction.
    //   * both run BEFORE #103 (StuntManager::Update), which is the console's own placement.
    //
    // [x] #95 ProgressionManager::PreWorldUpdate IS STAGED (issue #10 wave, 2026-09-06) -- leg 2c
    // below, at the console's own position (after the trigger legs, before #96). It used to read
    // "NOT STAGED ... nothing in the junction/start chain reads what it writes"; what it writes is
    // the ODOMETER, and the HUD read 0.0 km for as long as this leg was missing.
    // [x] #97 SendSetUpAllDriveThrusMessage IS STAGED ([minimap blips, issue #9, 2026-09-07]) -- leg
    // 2d below, at the console's own position (after #96, before #98). It used to read "NOT STAGED:
    // the console gates it on a one-shot byte at gsm+0x2C988 that it clears in the same breath";
    // that byte is ProgressionManager+0x20988 (gsm+0xBB30 + 0x20988), the manager's
    // mbDriveThruDataDirtyFlag, already modelled as mbDriveThrusDirty and raised by Construct /
    // OnDriveThru / UnlockToProgressionRank / the junkyard exit -- it was armed on every boot and
    // nobody drained it, so the drive-thru icon table was never published and the minimap had no
    // gas / body-shop / paint / junkyard / car-park blips.
    //
    // â›” CROSS-LANE: THE BODIES ARE AGENT D3'S. This lane owns the CALL SITES and the two
    // declarations in BrnGameStateModule.h (see the [D4 PUMP SEAM] block there). Until D3's landing
    // is consolidated these two are unresolved externals at LINK time -- the per-TU compile gate
    // passes, the exe does not link. That is the parallel-wave contract, stated rather than hidden.
    //
    // ---- 2c) ProgressionManager::PreWorldUpdate -- THE ODOMETER (issue #10, 2026-09-06) --------
    // X360 PreWorldUpdate @0x823A5328, 0x823A5B84..0x823A5B9C, inside the same `(a6 & 8)` leg as
    // the trigger legs above, after their StopMonitor and BEFORE CheckIfPlayerIsAtJunctionWithAnEvent:
    //     lfs  f1, 0(r15)          ; gsm+292284 == the SIM step (simTimer +4 * +8)
    //     fmr  f2, f31             ; the GAME step (gameTimer +4 * +8)
    //     mr   r6, r29             ; lpOutput
    //     mr   r7, r23             ; gsm+235488 == this module's active-race-car snapshot
    //     mr   r8, r21             ; the caller's update-set halfword (arg_3E)
    //     clrlwi r9, r11, 24       ; lbIsInJunkyard == (junkyard id != 0) || gsm+0x2CE34 byte
    //     bl   ProgressionManager::PreWorldUpdate
    // This is the ONLY console caller of ProgressionManager::AddDistanceDriven @0x823668F0, the
    // writer of every "distance driven" number in the game (Profile online/offline, the per-car-
    // type tally, the current livery's own metres -> CopyScoringDataToOutput ->
    // GuiEventCurrentStatus -> the HUD odometer). It was never staged; the odometer read 0.0 km.
    //
    // THE TWO TIMESTEPS ARE THE FRAME'S TIMER SNAPSHOT, the same object CopyScoringDataToOutput
    // and ModeManager::PreWorldUpdate already read (see the banner on this function's
    // declaration): the sim step is the sim timer's base*multiplier, exactly what gsm+292284
    // holds on the console; the game step is the argument this pump was handed.
    //
    // [FLAG PC bring-up] ONE argument is a PC derivation, named rather than hidden:
    //   * lUpdateSet -- the console's caller value comes from ConstructUpdateSetFromFsm; this pump
    //     has no update set. The callee tests ONLY bit 0 (network catch-up, the same bit Physics/
    //     AI test -- never set on an offline build), so 0 is the offline console value.
    //
    // ARTIST PreWorldUpdate @0x823A5B48..0x823A5B80: the full 64-bit junkyard id at
    // this+0x2CDC0 (CarSelectManager+0x20) OR the online car-select byte at this+0x2CE34.
    // While either is set, ProgressionManager saves the car's pose instead of integrating
    // distance. OnProfileLoaded uses that saved pose to choose the next boot's junkyard.
    {
        const f32 lfSimTimestep =
            lrTimerStatusInterface.GetSimTimerStatus()->GetCurrentTimeStep();
        const BrnUpdateSet luUpdateSet = 0;
        const bool lbIsInJunkyard = (mCarSelectManager.GetJunkyardId() != 0)
                                 || mOnlineCarSelectManager.IsInOnlineCarSelect();
        mProgressionManager.PreWorldUpdate(lfSimTimestep, lfGameTimestep,
                                           mpOutputBuffer, &mLastActiveRaceCarInterface,
                                           luUpdateSet, lbIsInJunkyard);
    }

    // Read-locked for the same reason leg 1b is: whatever D3's bodies read off the buffer, they
    // read through the read-lock halves of its accessors.
    if (mpPreWorldInputBuffer != 0)
    {
        mpPreWorldInputBuffer->LockForRead();
        CheckIfPlayerIsAtJunctionWithAnEvent(mpPreWorldInputBuffer, mpOutputBuffer);

        // ---- 2d) #97 THE DRIVE-THRU ICON TABLE ([minimap blips, issue #9, 2026-09-07]) ----------
        // The console's PreWorldUpdate, between #96 and #98: read the progression manager's
        // drive-thru-data dirty byte (+0x20988), CLEAR it, and if the value read was set run
        // SendSetUpAllDriveThrusMessage on the output action queue. Read-then-clear is the
        // console's order: a discovery that lands while the message is being built is not lost
        // (OnDriveThru raises the byte again), and a boot publishes the table exactly once
        // (ProgressionManager::Construct seeds the byte set).
        {
            const bool lbDriveThruDataDirty = mProgressionManager.IsDriveThruDataDirty();
            mProgressionManager.ClearDriveThruDataDirtyFlag();
            if (lbDriveThruDataDirty)
            {
                SendSetUpAllDriveThrusMessage(mpOutputBuffer->GetGameActionQueue());
            }
        }
        // [!] D3's DetectModeStarts carries a THIRD argument the console does not: the
        // module's cached game timestep at +292284, whose producer (PreWorldUpdate's own
        // timer leg) is not reconstructed -- see its declaration. Fed the same game-timer
        // product every other leg in this function uses.
        DetectModeStarts(mpPreWorldInputBuffer, mpOutputBuffer, lfGameTimestep);

        // â­ [D4] THE HARNESS START INJECTION (NOT IN THE X360 BINARY). Runs immediately after
        // DetectModeStarts, inside the same read-lock bracket, because it stands in for exactly
        // what DetectModeStarts' gesture gate decides. Env-gated off; see the body.
        HarnessInjectEventStartBringUp(mpOutputBuffer);
        HarnessInjectPlayerCarBringUp();   // [car] BRN_DEBUG_PLAYER_CAR (harness-only, see the body)
        HarnessAuditVehicleAudioBringUp();   // [car-audio] BRN_VEHICLE_AUDIO_AUDIT (harness-only, see the body)
        HarnessInjectJunkyardCarBringUp();    // [car-audio] BRN_DEBUG_JUNKYARD_CAR (harness-only, see the body)

        // ModeManager::HandleOnlineTeamModes at its console slot: after DetectModeStarts and
        // GameStateInviteManager::Update (not staged here), with the takedown queue this tick's
        // TakedownPreWorldLeg (above) cleared and refilled (gsm+249936). Its own gate (online, in
        // progress, mode 11 or 13) keeps it inert offline.
        mModeManager.HandleOnlineTeamModes(mpPreWorldInputBuffer, mpOutputBuffer,
                                           &mpTakedownCache->mTakedownEventQueue);

        // ✅ [showtime S7b-b, 2026-08-27] THE HARNESS SHOWTIME INJECTION IS GONE, and this is
        // the line that used to call it. Its DELETE-WHEN was "ShouldStartShowtimeMode and the
        // DetectModeStarts else arm land"; both landed this session, so DetectModeStarts above now
        // reaches StartCrashMode through the console's own gate stack -- with NO environment
        // variable set, which is the whole point: a real player holding both bumpers must get
        // showtime on the published build, and with BRN_START_SHOWTIME they did not.
        // ⛔ DO NOT RE-ADD A SHORTCUT HERE. [[invented-arms-and-the-c4715-ratchet]]

        mpPreWorldInputBuffer->UnlockForRead();
    }

    // ---- 3) StuntManager::Update: CONSUME the latch ------------------------------------------
    // X360 line 332:
    //     v51 = GameStateModuleIO::OutputBuffer::GetGameActionQueue(a5);
    //     StuntManager::Update(a1 + 183952, v51, a1 + 235488, <f1 == the game timestep>, v50);
    // The timestep rides f1 and consumes NO GPR slot (the PPC float-arg rule), which is why the
    // pseudocode renders only four arguments. The interface argument is the module's own cached
    // snapshot, refreshed by GameStateModule::PostWorldUpdate.
    mStuntManager.Update(lpActionQueue, &mLastActiveRaceCarInterface,
                         lfGameTimestep, lbIsAGameModeActive);

    // [DIAG BRN_QUEUE_WATERMARK] NOT IN THE X360 BINARY -- THE 13312 GAME-ACTION QUEUE WATERMARK.
    // âš âš  WHY IT EARNS ITS PLACE, and why a plain assert does not cover it: this queue is
    // GameStateModuleIO::GameActionQueue == CgsModule::VariableEventQueue<13312,16>, and its
    // AddEvent (CgsVariableEventQueue.h:427) DOES NOT RETURN after the overflow assert -- it fires
    // "Queue overflow." at CgsVariableEventQueue.h:469 and then MEMCPYS ANYWAY, past macData[13312],
    // into whatever follows the queue in the OutputBuffer. On a build with asserts routed to the
    // log rather than to a break, a single overflow silently corrupts the buffer.
    // The exposure is NEW this wave and it is large: ModeManager::PrepareForMode posts action 23 at
    // 2272 BYTES (BrnModeManager.h's own size table), and action 24 at 48 on top of it -- three
    // mode starts in one sub-step is over half the buffer. Nothing before wave D could post 2 KB in
    // one action.
    // Reports the running PEAK of GetSizeInBytes() (== miBufferWritePos - miFirstEventOffset, the
    // bytes actually written this sub-step) so a near-miss shows up BEFORE the overflow does, and
    // only when the peak moves, so a quiet drive prints a handful of lines and a mode start prints
    // one. Off unless BRN_QUEUE_WATERMARK is set.
    {
        static const bool sbWatermarkDiag = (getenv("BRN_QUEUE_WATERMARK") != 0);
        static s32        siPeakBytes     = 0;
        if (sbWatermarkDiag && CgsDev::Log::gpDebugPrint != 0)
        {
            const s32 liBytes = lpActionQueue->GetSizeInBytes();
            if (liBytes > siPeakBytes)
            {
                siPeakBytes = liBytes;
                *CgsDev::Log::gpDebugPrint
                    << "[queue-wm] game-action queue peak " << siPeakBytes
                    << " / 13312 bytes, n=" << lpActionQueue->GetLength() << "\n";
            }
        }
    }

    // ---- 4) StreetManager::Update ------------------------------------------------------------
    // The console calls it after StuntManager::Update (and the rich-presence, achievement and
    // developer-challenge ticks, which are not staged here), in the same update-set leg, with:
    //   lbPaused          = the raw pause word != 0 (not IsSimPaused's online-masked answer)
    //   lfSimTimeStep     = mfSimTimeStep
    //   the pre-world input buffer, the output buffer, the cached active-race-car snapshot and
    //   the cached AI car snapshot
    //   7th  (bool)       = a mode is running and its params carry KU_FLAG_USES_NAVIGATION
    //   8th  (bool)       = a mode is running and its params carry KU_FLAG_DISABLE_UPCOMING_ROAD_SIGNS
    //                       (true skips the upcoming-street walk)
    //   lfWrongWayTime    = ScoringSystem::GetPlayerWrongWayTime()
    // It runs the upcoming-street walk and the wrong-way timer, the friend / server score
    // updates, the buffered high scores and the profile score copy. It reads the input buffer's
    // network interface through the read-lock accessor; the console holds that lock across the
    // whole of PreWorldUpdate, here it is taken around the call.
    {
        const bool lbModeRunning = (mModeManager.GetCurrentGameMode() != 0);
        const GameModeParams* lpModeParams = mModeManager.GetCurrentGameModeParams();
        const bool lbUsesNavigation =
            lbModeRunning && lpModeParams->GetFlag(GameModeParams::KU_FLAG_USES_NAVIGATION);
        const bool lbDisableUpcomingRoadSigns =
            lbModeRunning && lpModeParams->GetFlag(GameModeParams::KU_FLAG_DISABLE_UPCOMING_ROAD_SIGNS);
        mpPreWorldInputBuffer->LockForRead();
        mStreetManager.Update(miSimPauseFlags != 0, mfSimTimeStep, mpPreWorldInputBuffer, mpOutputBuffer,
                              &mLastActiveRaceCarInterface, &mLastAICarOutputInterface,
                              lbUsesNavigation, lbDisableUpcomingRoadSigns,
                              mModeManager.GetScoringSystem()->GetPlayerWrongWayTime());
        mpPreWorldInputBuffer->UnlockForRead();
    }

    mbIsUpdating = false;
    mpOutputBuffer->UnlockForWrite();
}

// ============================================================================
// [D4 stuntrace WAVE D] HarnessInjectEventStartBringUp -- HARNESS-ONLY, NOT IN THE X360 BINARY.
//
// WHAT IT IS FOR: the console's offline start gesture is ANALOGUE -- SetButtonPressed @0x823BA240
// computes the pre-world buffer's ControllerInput byte +0x45 (mbRaceModePressed) as
// (padAction[+0x00] > 0.25f && padAction[+0x08] > 0.25f), i.e. both analogue triggers
// (accelerator AND brake), and ShouldStartSnapRaceMode @0x82363700 then requires it held for
// 0.35 s at speed <= 30 before it writes start mechanism 2 and DetectModeStarts calls
// StartModeAtLights. A scripted boot-drive (tools/diagnostics/flow_run.ps1) cannot hold two
// analogue triggers, so this leg stands in for the HOLD -- and for nothing else.
//
// WHAT IT SUBSTITUTES AND WHAT IT DOES NOT. It calls StartModeAtLights @0x82396CF8 directly with
// E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS (2) -- the exact value ShouldStartSnapRaceMode
// writes at a junction, and the value StartModeAtLights EARLY-RETURNS without
// (`cmpwi cr6, r31, 2 / bne loc_82397300` @0x82396D64). Everything downstream of that point is
// the console's own: the junction lookup, the RaceEventData fetch, the mu8Mode -> runtime-mode map
// through ProgressionManager::GetEvent, the special-event-car gate, ModeManager::StartGameMode.
// So this bypasses the GESTURE, not the START. If the junction is wrong, or the event does not
// resolve, or the special-event-car gate rejects the current car (it posts action 272 and returns
// -- expected on Burning Route junctions with an arbitrary car), this leg fails exactly the way
// the real gesture would, which is the point.
//
// GATES, all three required, and it fires AT MOST ONCE per process:
//   1. BRN_START_EVENT=1 in the environment (read once, like every other diag gate in this file),
//   2. TriggerQueryManager::IsPlayerInTrafficLightRegion() -- the player is actually standing in a
//      traffic-light trigger box. This is the console's own precondition for mechanism 2, and it
//      is also what makes GetPlayerCurrentTrafficLightId() safe to read (it asserts the same
//      predicate). Its writer is TriggerQueryManager's light-region leg; if this never becomes
//      true, the junction detection is the thing that is broken, not this hook.
//   3. no game mode already running (the same !mpCurrentGameMode gate the console's own start
//      arms carry).
//
// [!] IT IS NOT THE CASE-20 PATH, DELIBERATELY. The dispatcher's case-20 arm
// (GameStateModule_ProcessGameEvents_wBT_02.cpp) hard-codes E_MODE_OFFLINE_RACE / mechanism
// DEFAULT -- that is the binary's -- so injecting through case 20 would start an offline RACE,
// never a stunt run.
//
// DELETE-WHEN the offline event flow can be driven by a real pad in the harness (or the gesture is
// scriptable): this function and its one call site go together.
// ============================================================================
void GameStateModule::HarnessInjectEventStartBringUp(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    static const bool sbHarnessStart = (getenv("BRN_START_EVENT") != 0);
    static bool       sbFired        = false;

    if (!sbHarnessStart || sbFired || lpOutputBuffer == 0 || mpPreWorldInputBuffer == 0)
    {
        return;
    }

    // A benchmark car swap can finish while the junkyard UI is still open.
    // Wait for its normal exit before injecting the event-start gesture.
    if (mCarSelectManager.IsInJunkyard() || !mTriggerQueryManager.IsPlayerInTrafficLightRegion())
    {
        return;
    }

    if (mModeManager.GetCurrentGameMode() != 0)
    {
        return;
    }

    // FLAG PC-platform leaf: when both harness requests are armed, finish the
    // debug car swap before starting a mode (the swap itself requires freeburn).
    // Resolve the same ID/name forms as HarnessInjectPlayerCarBringUp.
    static const char* spcRequestedCar = getenv("BRN_DEBUG_PLAYER_CAR");
    if (spcRequestedCar != 0 && spcRequestedCar[0] != '\0')
    {
        static CgsID slRequestedCarId = 0;
        if (slRequestedCarId == 0)
        {
            const BrnResource::VehicleList* lpVehicles = GetVehicleList();
            const CgsID lId = CgsIDCompress(spcRequestedCar);
            for (s32 li = 0; lpVehicles != 0 && li < lpVehicles->GetVehicleCount(); ++li)
            {
                const BrnResource::VehicleListEntry* lpEntry = lpVehicles->GetVehicleData(li);
                if (lpEntry != 0 && (lpEntry->GetId() == lId
                    || _stricmp(lpEntry->GetName(), spcRequestedCar) == 0))
                {
                    slRequestedCarId = lpEntry->GetId();
                    break;
                }
            }
        }
        if (slRequestedCarId == 0 || GetActivePlayerCarId() != slRequestedCarId)
            return;
        // The requested ID changes before the world finishes streaming/resetting
        // the replacement. Wait for the published, loaded player model too.
        if (!mLastActiveRaceCarInterface.IsPlayerCarActive())
            return;
        const EActiveRaceCarIndex lePlayer = mLastActiveRaceCarInterface.GetPlayerActiveRaceCarIndex();
        if (!mLastActiveRaceCarInterface.IsRaceCarLoaded(lePlayer)
            || mLastActiveRaceCarInterface.GetCarModelId(lePlayer) != slRequestedCarId)
            return;
    }

    sbFired = true;

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        // GetPlayerCurrentTrafficLightId asserts IsPlayerInTrafficLightRegion(), which the gate
        // above has already established -- this read is safe here and nowhere else.
        *CgsDev::Log::gpDebugPrint
            << "[start] ***** HARNESS-ONLY START INJECTION (BRN_START_EVENT=1) ***** "
            << "light trigger id "
            << static_cast<s32>(mTriggerQueryManager.GetPlayerCurrentTrafficLightId())
            << " -- bypassing the 0.35 s analogue accelerator+brake hold and calling "
            << "StartModeAtLights with mechanism "
            << static_cast<s32>(E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS)
            << " (the value ShouldStartSnapRaceMode writes at a junction). Everything downstream "
            << "is the console's own. One-shot; will not fire again this process.\n";
    }

    // The caller holds the pre-world buffer's READ lock and the output buffer's WRITE lock, which
    // is the same bracket DetectModeStarts runs under one line above.
    StartModeAtLights(mpPreWorldInputBuffer, lpOutputBuffer,
                      E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS);
}

// ============================================================================
// [car-audio] HarnessInjectJunkyardCarBringUp -- NOT an X360 function.
//
// `BRN_DEBUG_JUNKYARD_CAR=<vehicle id>` (e.g. PUSCC01) picks that car WHILE THE PLAYER
// IS IN THE JUNKYARD, through the console's OWN path: SelectPlayerCarEvent, the event
// ProcessGameEvents case E_EVENT_SELECT_PLAYER_CAR turns into
// CarSelectManager::RequestChangeCar -- exactly what the car-select carousel posts when
// the player moves the selection.  The following Accept then exits the junkyard through
// UpdateExitState with the NEW mDesiredCarId, which is the owner's own sequence.
//
// WHY IT EXISTS: the harness makes a FRESH profile every run and a fresh profile owns
// exactly ONE car, so the carousel has a single entry and -CarSelectTaps has nothing to
// move to (measured 2026-09-16: Next:3, DPadRight:3 and no taps all ended on
// VEH_PUSMC01).  The owner's report is "every car that is NOT the Cavalry has no
// sound", so a one-car profile cannot reproduce it.  BRN_DEBUG_PLAYER_CAR swaps the car
// MID-DRIVE instead and takes the ChangePlayerCarEvent path, which measured CLEAN -- so
// the junkyard path needed its own lever.
//
// If the profile does not own the car, it is added first with the console's own
// ProgressionManager::AddCar(id, 0) -- the same call CarSelectManager::
// DEBUG_UnlockCarsForTesting makes for a car the profile lacks -- because
// CarSelectManager::GetProfileCarData asserts the desired car IS owned.
//
// Fires at most ONCE per process, KI_JUNKYARD_ARM_UPDATES updates after IsInJunkyard()
// first holds.  A name that matches no vehicle is a HARD REFUSAL with a log line, never
// a silent no-op.  Inert unless the variable is set; a default run is byte-identical.
// ============================================================================
void GameStateModule::HarnessInjectJunkyardCarBringUp()
{
    static const char* spcSpec = getenv("BRN_DEBUG_JUNKYARD_CAR");
    static bool        sbDone  = false;
    static s32         siArmed = 0;
    // The gate above is the CarSelectManager's own state, not a frame count: the pick must
    // land while the carousel is live (E_STATE_CAR_SELECT). RequestChangeCar overwrites
    // meState, so firing during E_STATE_TRANSITION_IN derails the entry and the GUI
    // car-select screen never opens -- measured 2026-09-16, four frame-counted runs never
    // printed "Entering Car Select". These 20 updates are only a settling margin once the
    // state gate already holds.
    const s32 KI_JUNKYARD_ARM_UPDATES = 20;

    if (spcSpec == 0 || spcSpec[0] == '\0' || sbDone)
    {
        return;
    }
    if (!mCarSelectManager.IsInJunkyard() || !mCarSelectManager.IsAtCarSelect())
    {
        siArmed = 0;
        return;
    }
    if (++siArmed < KI_JUNKYARD_ARM_UPDATES)
    {
        return;
    }
    sbDone = true;

    const BrnResource::VehicleList* lpVehicles = GetVehicleList();
    const CgsID lWantedId = CgsIDCompress(spcSpec);
    const BrnResource::VehicleListEntry* lpEntry = 0;
    for (s32 liVehicle = 0; lpVehicles != 0 && liVehicle < lpVehicles->GetVehicleCount(); ++liVehicle)
    {
        const BrnResource::VehicleListEntry* lpCandidate = lpVehicles->GetVehicleData(liVehicle);
        if (lpCandidate != 0
            && (lpCandidate->GetId() == lWantedId || _stricmp(lpCandidate->GetName(), spcSpec) == 0))
        {
            lpEntry = lpCandidate;
            break;
        }
    }
    if (lpEntry == 0)
    {
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[car-audio] FAIL: BRN_DEBUG_JUNKYARD_CAR=\"" << spcSpec
                << "\" matches no vehicle id or name in the vehicle list -- the car is NOT picked\n";
        }
        return;
    }

    // CarSelectManager::GetProfileCarData asserts the desired car is in the profile, so own
    // it first if it is not -- the console's own AddCar, unlock type 0 (the trophy-pass type
    // DEBUG_UnlockCarsForTesting uses).
    bool lbOwned = false;
    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    if (lpProfile != 0)
    {
        const s32 liCarCount = lpProfile->GetCarCount();
        for (s32 liCar = 0; liCar < liCarCount; ++liCar)
        {
            const BrnProgression::CarData* lpCandidate = lpProfile->GetCarData(liCar);
            if (lpCandidate != 0 && lpCandidate->GetId() == lpEntry->GetId())
            {
                lbOwned = true;
                break;
            }
        }
    }
    if (!lbOwned)
    {
        mProgressionManager.AddCar(lpEntry->GetId(), 0);
    }

    const BrnResource::WheelList* lpWheels = GetWheelList();
    const s32 liWheel = (lpWheels != 0) ? lpWheels->FindWheelIndexFromName(lpEntry->GetDefaultWheelName()) : -1;
    const CgsID lWheelId = (liWheel >= 0) ? lpWheels->GetWheelData(liWheel)->mID : mActivePlayerWheelId;

    GameStateModuleIO::SelectPlayerCarEvent lEvent = {};
    lEvent.mCarModelId   = lpEntry->GetId();
    lEvent.mWheelModelId = lWheelId;
    GetDebugGameEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                                       GameStateModuleIO::E_EVENT_SELECT_PLAYER_CAR, sizeof(lEvent));

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[car-audio] ***** HARNESS-ONLY JUNKYARD CAR PICK (BRN_DEBUG_JUNKYARD_CAR) ***** -> '"
            << lpEntry->GetName() << "' wheel index " << liWheel
            << " owned=" << (lbOwned ? 1 : 0)
            << " -- posted SelectPlayerCarEvent exactly as the car-select carousel does; "
            << "everything downstream is the console's own. One-shot.\n";
    }
}

// ============================================================================
// [car-audio] HarnessAuditVehicleAudioBringUp -- NOT an X360 function.
//
// `BRN_VEHICLE_AUDIO_AUDIT=1` dumps the WHOLE VehicleList once: for every entry, the
// car id, the display name, and the two audio bank names the sound chain keys off
// (mEngineName / mExhaustName), each with the "Engines\\%08x.bundle" path the loader
// will actually ask for.  Those two paths are built exactly as the three console sites
// build them --
//   GameDataModule::ProcessLoadVehicleRequest  (SOUND leg) -> HashString(mExhaustName)
//   GameDataModule::ProcessGetVehicleRequest   (SOUND leg) -> HashString(mEngineName)
//   PhysicsControl::SetupLoadData                          -> both
// -- so a line here whose bundle is not on disk under Engines\\ is a car that CANNOT
// have engine audio, whatever the rest of the chain does.
//
// Fires at most ONCE per process, as soon as the vehicle list is resolvable.  Inert
// unless the variable is set; a default run is byte-identical.
// ============================================================================
void GameStateModule::HarnessAuditVehicleAudioBringUp()
{
    static const char* spcOn = getenv("BRN_VEHICLE_AUDIO_AUDIT");
    static bool        sbDone = false;

    if (spcOn == 0 || spcOn[0] == '\0' || sbDone || CgsDev::Log::gpDebugPrint == 0)
    {
        return;
    }

    const BrnResource::VehicleList* lpVehicles = GetVehicleList();
    if (lpVehicles == 0 || lpVehicles->GetVehicleCount() <= 0)
    {
        return;   // not resolvable yet -- try again next update
    }
    sbDone = true;

    *CgsDev::Log::gpDebugPrint
        << "[car-audio] ***** VEHICLE AUDIO AUDIT ***** " << lpVehicles->GetVehicleCount()
        << " entries; columns: index id name engineName engineBundle exhaustName exhaustBundle\n";

    for (s32 liVehicle = 0; liVehicle < lpVehicles->GetVehicleCount(); ++liVehicle)
    {
        const BrnResource::VehicleListEntry* lpEntry = lpVehicles->GetVehicleData(liVehicle);
        if (lpEntry == 0)
        {
            *CgsDev::Log::gpDebugPrint << "[car-audio] audit " << liVehicle << " <null entry>\n";
            continue;
        }

        char lacId[KI_CGSID_STRING_LEN];
        char lacEngine[KI_CGSID_STRING_LEN];
        char lacExhaust[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEntry->GetId(), lacId);
        CgsIDConvertToString(lpEntry->GetEngineName(), lacEngine);
        CgsIDConvertToString(lpEntry->GetExhaustName(), lacExhaust);

        const u32 luEngineHash = static_cast<u32>(CgsResource::ID::HashString(
            reinterpret_cast<const u8*>(lacEngine)));
        const u32 luExhaustHash = static_cast<u32>(CgsResource::ID::HashString(
            reinterpret_cast<const u8*>(lacExhaust)));

        *CgsDev::Log::gpDebugPrint
            << "[car-audio] audit " << liVehicle
            << " id=" << lacId
            << " name=" << (lpEntry->GetName() ? lpEntry->GetName() : "?")
            << " engine=" << lacEngine
            << " engineBundle=" << CgsDev::E_PRINTMODE_HEXONCE << luEngineHash
            << " exhaust=" << lacExhaust
            << " exhaustBundle=" << CgsDev::E_PRINTMODE_HEXONCE << luExhaustHash
            << "\n";
    }

    *CgsDev::Log::gpDebugPrint << "[car-audio] ***** VEHICLE AUDIO AUDIT END *****\n";
}

// ============================================================================
// [car] HarnessInjectPlayerCarBringUp -- NOT an X360 function, and DELIBERATELY PERMANENT.
//
// `BRN_DEBUG_PLAYER_CAR=<vehicle id or name>` (e.g. PDDK01, or P_DLC_DirtKing_01) swaps the
// player into that car, once, through the console's OWN debug path: the ChangePlayerCarEvent that
// ResetPlayerDebugComponent::ChangeCar @0x82382B20 posts from the "Change player car" development
// menu, into the same debug game-event queue, drained by the same E_EVENT_CHANGE_PLAYER_CAR case
// (OnPlayerCarChange + the ResetPlayerCarAction + the colour action). The harness has no pad to
// open that menu with, and b5-decomp issue #19 ("some cars' meshes / shadows are corrupted")
// can only be reproduced by standing in the reporter's cars.
//
// GATES, and it fires AT MOST ONCE per process: the env var set (read once); the player car
// attached (GetActivePlayerCarId() != 0); not in the junkyard; no game mode running (the menu's
// own gate); then KI_CAR_SWAP_ARM_UPDATES pre-world updates with all of those holding, so the car
// is on the road when it is swapped. A name that matches no vehicle is a HARD REFUSAL with a log
// line, never a silent no-op. Inert unless the variable is set; a default run is byte-identical.
// ============================================================================
void GameStateModule::HarnessInjectPlayerCarBringUp()
{
    static const char* spcSpec  = getenv("BRN_DEBUG_PLAYER_CAR");
    static bool        sbDone   = false;
    static s32         siArmed  = 0;
    const s32 KI_CAR_SWAP_ARM_UPDATES = 180;

    if (spcSpec == 0 || spcSpec[0] == '\0' || sbDone)
    {
        return;
    }
    if (GetActivePlayerCarId() == 0 || mCarSelectManager.IsInJunkyard()
        || mModeManager.GetCurrentGameMode() != 0)
    {
        siArmed = 0;
        return;
    }
    if (++siArmed < KI_CAR_SWAP_ARM_UPDATES)
    {
        return;
    }
    sbDone = true;

    const BrnResource::VehicleList* lpVehicles = GetVehicleList();
    const CgsID lWantedId = CgsIDCompress(spcSpec);
    const BrnResource::VehicleListEntry* lpEntry = 0;
    for (s32 liVehicle = 0; lpVehicles != 0 && liVehicle < lpVehicles->GetVehicleCount(); ++liVehicle)
    {
        const BrnResource::VehicleListEntry* lpCandidate = lpVehicles->GetVehicleData(liVehicle);
        if (lpCandidate != 0
            && (lpCandidate->GetId() == lWantedId || _stricmp(lpCandidate->GetName(), spcSpec) == 0))
        {
            lpEntry = lpCandidate;
            break;
        }
    }
    if (lpEntry == 0)
    {
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[car] FAIL: BRN_DEBUG_PLAYER_CAR=\"" << spcSpec
                << "\" matches no vehicle id or name in the vehicle list -- the car is NOT swapped\n";
        }
        return;
    }

    const BrnResource::WheelList* lpWheels = GetWheelList();
    const s32 liWheel = (lpWheels != 0) ? lpWheels->FindWheelIndexFromName(lpEntry->GetDefaultWheelName()) : -1;
    const CgsID lWheelId = (liWheel >= 0) ? lpWheels->GetWheelData(liWheel)->mID : mActivePlayerWheelId;

    GameStateModuleIO::ChangePlayerCarEvent lEvent = {};
    lEvent.mCarModelId        = lpEntry->GetId();
    lEvent.mWheelModelId      = lWheelId;
    lEvent.mbResetPlayerCamera = true;
    lEvent.mbKeepResetSection  = true;
    GetDebugGameEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                                       GameStateModuleIO::E_EVENT_CHANGE_PLAYER_CAR, sizeof(lEvent));

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[car] ***** HARNESS-ONLY PLAYER CAR SWAP (BRN_DEBUG_PLAYER_CAR) ***** -> '"
            << lpEntry->GetName() << "' wheel index " << liWheel
            << " -- posted ChangePlayerCarEvent exactly as the development menu does; everything "
            << "downstream is the console's own. One-shot; will not fire again this process.\n";
    }
}

// ==============================================================================================
// [FX-GS2 2026-09-23, crash-parity G10-D11] The rival-tailing clock and its per-frame check.
//
// GetRivalTailingTime / SetRivalTailingTime: CheckForTailingRivals keeps one f32 per race-car
// slot at gsm+0x32D90 + 4 * slot (r27, stepped by `addi r27, r27, 4` @0x82376384 for EVERY slot,
// the player's included). The DWARF array is f32[7] (BrnGameStateModule.h:792), so slot 7 lands on
// the next word, muNetworkGameRandomSeed (gsm+0x32DAC). That aliasing is the console's; it is
// routed here through the seed word's bits rather than by indexing past the array.
// ==============================================================================================
f32 GameStateModule::GetRivalTailingTime(s32 liRaceCarIndex) const
{
    if (liRaceCarIndex < 7)
    {
        return mafRivalTailingTimes[liRaceCarIndex];
    }
    f32 lfTime;
    memcpy(&lfTime, &muNetworkGameRandomSeed, sizeof(lfTime));   // slot 7 == gsm+0x32DAC
    return lfTime;
}

void GameStateModule::SetRivalTailingTime(s32 liRaceCarIndex, f32 lfTime)
{
    if (liRaceCarIndex < 7)
    {
        mafRivalTailingTimes[liRaceCarIndex] = lfTime;
        return;
    }
    memcpy(&muNetworkGameRandomSeed, &lfTime, sizeof(lfTime));   // slot 7 == gsm+0x32DAC
}

// ----------------------------------------------------------------------------------------------
// CheckForTailingRivals @0x82375F90 (DWARF BrnGameStateModule.h:808; sole caller PreWorldUpdate
// @0x823A5328, `bl` @0x823A57A0). For each rival sitting just behind the player at speed, run a
// clock; at 3 s, post the GUI "on your tail" record and restart it.
//     0x82375FBC..0x82375FEC  mode type (ModeManager +0xD94) == 0 (offline race), or the current
//                             mode (+0xD98) is online (+0xAC) -- else return
//     0x82375FF0..0x82376000  meControllerState (gsm+0x38B64) == 3 -- else return
//     0x82376004..0x823760F8  the player's used-car bit (the bit array's "invalid index" assert)
//     0x823760FC..0x8237619C  |RaceCarState.mLinearVelocity (+0x330)| > 50.0 (flt_820138DC):
//                             vmsum3fp128 + vrsqrtefp + two Newton steps (0 when the square is 0);
//                             the exact root here, this tree's convention (BrnMathUtils.h)
//     0x823761A0..0x823761DC  assert "lpScoringSystem != NULL" (line 0x1949); the player's
//                             GetRaceCarDistanceToFinish
//     loop slot 0..7 (the enum ++ carries BurnoutConstants.h:39's range assert):
//       0x82376220  the player's slot: skipped (its clock untouched)
//       0x82376234  player < rival (fcmpu/bge) and !((player + 20.0 (flt_820054CC)) < rival)
//                   (fcmpu/blt) -- the rival is BEHIND the player by at most 20 m
//       0x823762DC  the rival's used-car bit ; 0x82376318 RaceCarState.mfSpeedMPH (+0x3CC) > 50.0
//       0x82376328  all true: clock += the time step
//       0x82376338  otherwise `fsel f0, f0, 0.0, f0`: a clock >= 0 resets to 0, a negative or NaN
//                   one is kept
//       0x82376344  !(clock < 3.0 (flt_8202AC20)) (a NaN passes too): AddOnTailEvent(GetRivalId(slot)
//                   from the module's active-car snapshot (gsm+0x397E0), slot) through the
//                   write-locked GetGameStateToGuiInterface, then the clock = 0.0 (0x8237637C)
// Because of the slot-7 aliasing, ClearData's 0xFFFFFFFF seed reads as a NaN clock: on the first
// gated frame in which the player is not in slot 7 the console posts one record for slot 7 and the
// seed word becomes 0.0f. That is the console's arithmetic and it is kept.
// ----------------------------------------------------------------------------------------------
void GameStateModule::CheckForTailingRivals(GameStateModuleIO::OutputBuffer* lpOutput,
                                            const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutput,
                                            f32 lfTimeStep)
{
    if (mModeManager.GetCurrentGameModeType() != GameStateModuleIO::E_MODE_OFFLINE_RACE)
    {
        const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        if (lpCurrentGameMode == 0 || !lpCurrentGameMode->IsOnline())
        {
            return;
        }
    }

    if (meControllerState != E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE)
    {
        return;
    }

    if (!lpVehicleOutput->GetUsedCarsBitArray().IsBitSet(static_cast<u32>(GetPlayerActiveRaceCarIndex())))
    {
        return;
    }

    const f32 lfPlayerSpeed = rw::math::vpu::Magnitude(
        lpVehicleOutput->GetRaceCar(static_cast<u32>(GetPlayerActiveRaceCarIndex()))->mLinearVelocity);
    if (!(lfPlayerSpeed > 50.0f))
    {
        return;
    }

    const ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
    CGS_ASSERT(lpScoringSystem != NULL, "lpScoringSystem != NULL");

    const f32 lfPlayerDistanceToFinish =
        lpScoringSystem->GetRaceCarDistanceToFinish(GetPlayerActiveRaceCarIndex());

    for (::EActiveRaceCarIndex leIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        if (GetPlayerActiveRaceCarIndex() == leIndex)
        {
            continue;
        }

        const f32 lfDistanceToFinish = lpScoringSystem->GetRaceCarDistanceToFinish(leIndex);
        f32 lfTime = GetRivalTailingTime(static_cast<s32>(leIndex));

        if (lfPlayerDistanceToFinish < lfDistanceToFinish
            && !((lfPlayerDistanceToFinish + 20.0f) < lfDistanceToFinish)
            && lpVehicleOutput->GetUsedCarsBitArray().IsBitSet(static_cast<u32>(leIndex))
            && lpVehicleOutput->GetRaceCar(static_cast<u32>(leIndex))->mfSpeedMPH > 50.0f)
        {
            lfTime = lfTime + lfTimeStep;
        }
        else
        {
            lfTime = (lfTime >= 0.0f) ? 0.0f : lfTime;   // fsel
        }
        SetRivalTailingTime(static_cast<s32>(leIndex), lfTime);

        if (!(lfTime < 3.0f))
        {
            lpOutput->GetGameStateToGuiInterface()->AddOnTailEvent(
                mLastActiveRaceCarInterface.GetRivalId(leIndex), leIndex);
            SetRivalTailingTime(static_cast<s32>(leIndex), 0.0f);

            // [FLAG PC witness] -- NOT IN THE X360 BINARY. Opt-in behind BRN_MODEMGR_DIAG (the
            // race-HUD cases' gate), first 12 records: proves the on-tail record was posted and names
            // the slot and the clock that crossed 3.0. DELETE-WHEN the on-tail HUD message has a
            // live oracle.
            static const bool sbTailingDiag = (getenv("BRN_MODEMGR_DIAG") != 0);
            static s32 siTailingWitnessed = 0;
            if (sbTailingDiag && siTailingWitnessed < 12 && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siTailingWitnessed;
                *CgsDev::Log::gpDebugPrint
                    << "[tailing] on-tail record: slot " << static_cast<s32>(leIndex)
                    << " player " << static_cast<s32>(GetPlayerActiveRaceCarIndex())
                    << " clock " << lfTime << " player speed " << lfPlayerSpeed
                    << " gap " << (lfDistanceToFinish - lfPlayerDistanceToFinish)
                    << " [FLAG PC witness]\n";
            }
        }
    }
}

// ==============================================================================================
// [FX-FLOW 2026-09-24, crash-parity NEW-EMMTAIL] EmmPreWorldUpdateTailBringUp -- the tail of
// GameStateModule::EmmPreWorldUpdate @0x8238EF50 after UpdateRoadRulesManager, in the console's
// order. r23 is the output buffer, r25 the ScoringSystem (`addi r25, r31, 0x1DD0` @0x8238F1C4 ==
// gsm+0x1020 ModeManager + 0xDB0, reached by name through GetScoringSystem()).
//
//   (1) 0x8238F1BC..0x8238F214  the game-mode elapsed time:
//         Time::Time(&t, f31 = flt_82001CC0 = 0.0f)
//         if (GetPlayerActiveRaceCarIndex() != -1)
//             t = ScoringSystem::GetRaceCarTotalTime(player, <sim time>)      ; 0x8231F480
//         OutputBuffer::SetGameModeElapsedTime(&t)                            ; 0x82362F80
//       <sim time> is var_90: the sim TimerStatus's Time out of the 48-byte timer copy at
//       gsm+0x32DC8 (+0x28 = second entry +0x10). [FLAG PC] read off the caller's interface, the
//       deviation leg 1b of PreWorldUpdateStuntBringUp names (same data, one copy earlier).
//   (2) 0x8238F218..0x8238F288  every active slot 0..7 (the BurnoutConstants.h:39 operator++):
//         CarData* lp = ScoringSystem::GetCarData(slot)                      ; sub_8231DCD0
//         if (lp) OutputBuffer::GetGameStateToNetworkInterface()             ; sub_8231D800
//                   ->SetActiveRaceCarIndex(lp +0x148 network id, lp +0x144 slot) ; 0x823558A0
//   (3) 0x8238F28C..0x8238F33C  the overtake record:
//         if (player != -1 && ScoringSystem::GetOvertakenRival(player))     ; inlined, h:2227
//             GameStateToGuiInterface::AddOvertakeEvent(                     ; inlined, +0xC8
//                 (u8)ScoringSystem::GetCarRacePosition(player), player)     ; 0x82326980
// ==============================================================================================
void GameStateModule::EmmPreWorldUpdateTailBringUp(const CgsSystem::TimerStatusInterface& lrTimerStatusInterface)
{
    ScoringSystem* const lpScoringSystem = mModeManager.GetScoringSystem();

    // ---- (1) the game-mode elapsed time -----------------------------------------------------
    CgsSystem::Time lGameModeElapsedTime(0.0f);   // Time::Time(f31 == flt_82001CC0 == 0.0f) @0x8238F1C8
    if (GetPlayerActiveRaceCarIndex() != ::E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        lGameModeElapsedTime = lpScoringSystem->GetRaceCarTotalTime(
            GetPlayerActiveRaceCarIndex(), lrTimerStatusInterface.GetSimTimerStatus()->GetTime());
    }
    mpOutputBuffer->SetGameModeElapsedTime(&lGameModeElapsedTime);

    // ---- (2) the network interface's active-race-car mapping ---------------------------------
    for (::EActiveRaceCarIndex leIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; leIndex++)
    {
        const CarData* lpCarData = lpScoringSystem->GetCarData(leIndex);   // BrnGameState::CarData
        if (lpCarData != 0)
        {
            mpOutputBuffer->GetGameStateToNetworkInterface()->SetActiveRaceCarIndex(
                lpCarData->GetNetworkPlayerID(), lpCarData->GetActiveRaceCarIndex());
        }
    }

    // ---- (3) the overtake record (GUI 371 via TranslateGuiInterfaceToGuiEvents) ---------------
    if (GetPlayerActiveRaceCarIndex() != ::E_ACTIVE_RACE_CAR_INDEX_INVALID &&
        lpScoringSystem->GetOvertakenRival(GetPlayerActiveRaceCarIndex()))
    {
        const ::EActiveRaceCarIndex lePlayer = GetPlayerActiveRaceCarIndex();
        const u8 lu8NewPosition =
            static_cast<u8>(lpScoringSystem->GetCarRacePosition(GetPlayerActiveRaceCarIndex()));
        mpOutputBuffer->GetGameStateToGuiInterface()->AddOvertakeEvent(lu8NewPosition, lePlayer);

        // [DIAG] NOT IN THE X360 BINARY -- BRN_MODEMGR_DIAG (the race-HUD cases' gate), first 12.
        static const bool sbOvertakeDiag = (getenv("BRN_MODEMGR_DIAG") != 0);
        static s32        siOvertakesWitnessed = 0;
        if (sbOvertakeDiag && siOvertakesWitnessed < 12 && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siOvertakesWitnessed;
            *CgsDev::Log::gpDebugPrint
                << "[overtake] player slot " << static_cast<s32>(lePlayer) << " gained a place -> position "
                << static_cast<s32>(lu8NewPosition) << " (GameStateToGuiInterface +0xC8 -> GUI 371)\n";
        }
    }
}

}
