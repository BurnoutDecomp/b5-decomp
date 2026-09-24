// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_gTD_00.cpp
//
// [takedown wave 2026-09-02, conductor] THE TAKEDOWN MANAGER'S PLUMBING INTO GameStateModule.
//
// On X360 the manager is embedded at gsm+568 and fed by two pieces of per-frame state the module
// keeps beside it:
//   gsm+249936  EventQueue<TakedownEvent,8>          the module's COPY of the output buffer's
//                                                    takedown-event queue (Clear + Append each tick)
//   gsm+250272  EventQueue<RaceCarCrashEvent,8>      the post-world crash queue, cached by
//                                                    CacheTakedownManagerPostWorldInputData @0x82375E70
//   gsm+250816  VehicleOutputInterface               the module's cached copy of the post-world
//                                                    vehicle output -- used-cars head at +0, the
//                                                    eight RaceCarStates at +0x10 (the console's
//                                                    8960-byte memcpy span). The frame's
//                                                    CrashingRaceCarInterface is built FROM it by
//                                                    SetFromVehicleOutputInterface into a STACK
//                                                    local of PreWorldUpdate (var_6D8).
// This build has neither a PostWorldInputBuffer nor a per-frame output buffer (mpOutputBuffer is
// new'd once), so the same state lives in a heap-allocated TakedownPostWorldCache (mpTakedownCache)
// and the manager itself is heap-allocated (mpTakedownManager) -- the mpTrainingManager precedent.
// FLAG PC deviation: pointers where the console embeds; the bodies below are the console's.
//   Embedding would need the complete TakedownManager (BrnTakedownManager.h) by value, which
//   changes GameStateModule's layout and its Construct/Destruct lifetime -- a change this lane
//   did not take. BrnGameStateModule.h keeps only the forward declarations.
//
// Console positions reproduced here (GameStateModule::PreWorldUpdate @0x823A5328, !IsSimPaused arm,
// ): SetFromVehicleOutputInterface(stack, cachedVehicleOutput == r24 ==
// gsm+250816) ->
// TakedownManager::Update(this+0x238, activeIf, dt, crashQ, &crashingIf, preIn, out, trafficTypeQ)
// -> `*(gsm+249944) = 0` (the module copy's miLength) + TakedownEvent_::Append(gsm+249936, out's
// takedown queue) -> MugshotManager::Update(gsm +0x500, ...) -> PaybackManager::Update(gsm +0x570,
// ...) ->
// ProcessTakedownEvents(actionQ, gsm+249936, out).
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameStateTakedownCache.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"     // MugshotManager (Construct / Update)
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"     // PaybackManager (Construct / Update)
#include "GameSource/GameState/Offences/BrnDriveThruManager.h"    // DriveThroughsCanNowOpenAgain (OnModeFinish / OnModeEnd)
#include "GameSource/GameState/BrnGameActions.h"                   // SetTakedownCameraAction (OnModeFinish)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"   // GameMode::GetCurrentState / GetTimeInMode (harness hook)
#include <cstdlib>   // getenv / atof (harness hook)

namespace BrnGameState
{

// X360 GameStateModule::Construct @0x82380388: the inlined TakedownManager::Construct (the two
// manager pointers; `*(gsm+1256) = gsm+568` is the embedded debug component's mpTakedownManager,
// written by TakedownManager::Construct -- agent T1's finding) and the three
// per-frame queues' Constructs (`RaceCarCrashEvent_8_::Construct(gsm+250272)`,
// `TakedownEvent_8_::Construct(gsm+249936)`, ContactSpyInterface::Construct(gsm+250800) ...).
void GameStateModule::ConstructTakedownBringUp()
{
    if (mpTakedownManager == 0)
    {
        mpTakedownManager = new TakedownManager();
    }
    mpTakedownManager->Construct(&mModeManager, &mProgressionManager);

    if (mpTakedownCache == 0)
    {
        mpTakedownCache = new TakedownPostWorldCache();
    }
    mpTakedownCache->Construct();

    // The two managers the module Constructs immediately after the TakedownManager, in the console's
    // order: MugshotManager::Construct(gsm +0x500, gsm) then PaybackManager::Construct(gsm +0x570,
    // gsm). Each takes the owning module as its single argument and registers its own debug
    // component. Held by pointer here for the reason recorded on the members -- BrnMugshotManager.h
    // includes BrnGameStateModule.h, so a by-value member is a genuine include cycle; the allocation
    // site moves, nothing else does.
    //
    // THE never-Constructed-QUEUE TRAP, PAID HERE BECAUSE THIS LANDING ARMS IT. Both managers
    // publish their HUD records onto GetOutputGuiEventQueue()'s VariableEventQueue<18432,16>
    // (mOutputGuiEventQueue), and until now NOTHING in this tree ever wrote to it, so nothing
    // noticed that it is never Constructed -- the module is heap-allocated, so its write cursor and
    // length are garbage and the first AddEvent would walk off the buffer. The console's own
    // Construct carries `VariableEventQueue<18432,16>::Construct(gsm +0x2E580)`; that call belongs
    // in GameStateModule::Construct, which is not this file. DELETE-WHEN it lands there.
    mOutputGuiEventQueue.Construct();

    if (mpMugshotManager == 0)
    {
        mpMugshotManager = new MugshotManager();
    }
    mpMugshotManager->Construct(this);

    if (mpPaybackManager == 0)
    {
        mpPaybackManager = new PaybackManager();
    }
    mpPaybackManager->Construct(this);
}

// X360 GameStateModule::Prepare @0x8239E578, stage 14: `if (TakedownManager::Prepare(gsm+568))`.
bool GameStateModule::PrepareTakedownBringUp()
{
    return mpTakedownManager->Prepare();
}

// ==============================================================================================
// GameStateModule::CacheTakedownManagerPostWorldInputData  (, post-world `bl` #18)
//
// The console body, statement for statement (r29 == gsm, r26 == lpInput, r31 == gsm+250816,
// r27 == gsm+250800, r30 == lpInput's VehicleOutputInterface):
//   CGS_ASSERT(lpInput, "lpInput")
//   *(gsm+250280) = 0                     the crash queue's miLength                 (Clear)
//   *(gsm+250800) = 0                     the contact-spy word
//   *(+250816 +0x2628) = 0                the cached traffic-state queue's miLength   (Clear)
//   *(+250816 +0x2318) = 0                the cached impact queue's miLength          (Clear)
//   VariableEventQueue<1536,16>::Clear(+250816 +0x65F0)   the cached game-event queue (Clear)
//   std 0, +250816                        the used-cars head, zeroed
//   stb 0, +250816 +0x6C00 .. +0x6C04     the five AggressiveDrivingFlags bytes
//  *(gsm+250800) = *<contact-spy accessor>(lpInput)  [NAME NOT RECOVERED: that
//                                        symbol is unnamed in the image and its body is only the
//                                        "Not locked for reading" assert around a member fetch]
//   RaceCarCrashEvent_::Append(gsm+250272, GetRaceCarCrashEventQueue(lpInput))
//   r30 = GetVehicleOutputInterface(lpInput)
//   PhysicalTrafficState_::Append(+250816 +0x2620, r30 +0x2620)
//   ImpactEvent_::Append          (+250816 +0x2310, r30 +0x2310)
//   VariableEventQueue::Append    (+250816 +0x65F0, r30 +0x65F0)
//   std *(r30 +0), +250816                the used-cars head
//   memcpy(+250816 +0x10, r30 +0x10, 0x2300)   the eight RaceCarStates (CONSOLE span)
// That Clear-then-Append/copy of every field of the cached interface IS
// VehicleOutputInterface::operator= (, BrnVehicleOutputInterface.cpp), so the copy runs
// through that committed symbol -- by sizeof on the host, never at the console's 8960.
//   ONE RECORDED DIFFERENCE: the console leaves the cached AggressiveDrivingFlags ZEROED (the five
//   byte stores above are never followed by a copy) while operator= copies them. Nothing reads that
//   field out of the cache -- SetFromVehicleOutputInterface touches only mUsedRaceCars and the
//   RaceCarStates -- so the copy is inert; recorded rather than special-cased.
//
// [FLAG PC bring-up] THE ARGUMENTS ARE THE DEVIATION, NOT THE BODY -- the same reduction
// ProcessContacts carries. The console reads both values out of the PostWorldInputBuffer nothing on
// this build stages; the world module's UpdateOutputBuffer publishes exactly these two types, so
// they arrive as arguments. The contact-spy word (gsm+250800) is deliberately NOT cached here:
// this build feeds ProcessContacts the interface directly at the same post-world point
// (PostWorldUpdateStuntBringUp leg 5), so caching it too would be the one-feed-not-two mistake.
// ==============================================================================================
void GameStateModule::CacheTakedownManagerPostWorldInputData(
        const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
        const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>* lpRaceCarCrashEventQueue)
{
    if (mpTakedownCache == 0)
    {
        return;
    }

    mpTakedownCache->mRaceCarCrashEventQueue.Clear();
    if (lpRaceCarCrashEventQueue != 0)
    {
        mpTakedownCache->mRaceCarCrashEventQueue.Append(*lpRaceCarCrashEventQueue);
    }

    // [PC GUARD] the console has no null test here -- it reads the interface straight out of the
    // PostWorldInputBuffer. A null pointer has nothing to copy, so the tripwire costs nothing.
    if (lpVehicleOutputInterface != 0)
    {
        mpTakedownCache->mVehicleOutputInterface = *lpVehicleOutputInterface;
    }
}

// The traffic-type response queue is NOT part of CacheTakedownManagerPostWorldInputData. It is
// TakedownManager::Update's 7th argument, gsm+278480 (r26), a TrafficTypeResponse<32>
// queue the module owns: Constructed in GameStateModule::Construct and Clear+Append'ed
// by GameStateModule::PostWorldUpdate itself ( -- the miLength store at and
// TrafficTypeResponse_::Append at), two `bl` BEFORE the cache call. Reproduced at that
// position, with the same argument deviation as the cache above.
void GameStateModule::CacheTakedownTrafficTypeResponses(
        const CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse>* lpTrafficTypeResponseQueue)
{
    if (mpTakedownCache == 0)
    {
        return;
    }
    mpTakedownCache->mTrafficTypeResponseQueue.Clear();
    if (lpTrafficTypeResponseQueue != 0)
    {
        mpTakedownCache->mTrafficTypeResponseQueue.Append(*lpTrafficTypeResponseQueue);
    }
}

// The !IsSimPaused takedown leg of PreWorldUpdate (see the banner). Replaces the direct
// ProcessTakedownEvents call that stood in PreWorldUpdateStuntBringUp.
void GameStateModule::TakedownPreWorldLeg(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                          f32 lfGameTimestep,
                                          const CgsSystem::TimerStatusInterface& lrTimerStatusInterface,
                                          bool lbSimPaused)
{
    // Console PreWorldUpdate clears the module's own GUI event queue (gsm +0x2E580) once per frame,
    // at the head of the function, immediately before ProcessGameEvents. Reproduced here, the
    // earliest per-frame point this file owns: nothing between the console's position and this one
    // writes that queue on this build -- the only writers tree-wide are the two manager ticks below.
    // Without the clear the queue only ever grows, and AddEvent's overflow arm asserts and then
    // writes the record ANYWAY, so repeated payback publishes eventually walk off the storage.
    // DELETE-WHEN the clear can sit in PreWorldUpdate itself, beside the game-event drain.
    //
    // FLAG NOT REPRODUCED (blocked outside this file): the console's matching per-frame DRAIN at the
    // tail of PreWorldUpdate, `OutputBuffer::GetGuiEventQueue()->Append(<this queue>)`, unconditional
    // just before the race-distance fill. OutputBufferGuiEventQueue is still the opaque byte-array
    // placeholder in BrnGameStateModuleIO.h rather than the console's VariableEventQueue<18432,16>,
    // so the destination has neither an Append nor a Construct. Until it is retyped there, the
    // records the managers publish are cleared unread instead of reaching the GUI.
    mOutputGuiEventQueue.Clear();

    // The console guards none of this leg: the two manager ticks below sit unconditionally inside
    // its not-paused arm, gated only by the pause byte. So the takedown manager's own pointer is
    // NOT part of the leg's entry test any more -- it gates only its own tick, and the mugshot and
    // payback ticks are no longer silently coupled to takedown bring-up succeeding.
    if (mpTakedownCache == 0)
    {
        return;
    }

    CgsModule::EventQueue<TakedownEvent, 8>* lpOutputTakedownQueue =
        reinterpret_cast<CgsModule::EventQueue<TakedownEvent, 8>*>(
            mpOutputBuffer->GetTakedownEventOutputQueue());

    // [FLAG PC bring-up] THE OUTPUT BUFFER IS PERSISTENT ON THIS BUILD. The console gets a fresh
    // OutputBuffer every frame (CreateIOBuffer in DoUpdate_GameStatePreWorld), so its takedown
    // queue starts empty; here it would accumulate, and every past takedown would be re-scored each
    // frame. Cleared at the top of the leg -- before Update posts this frame's events and before the
    // world bridge (BrnGameModule.cpp GetTakedownEventOutputQueue) copies them out later in the frame.
    // DELETE-WHEN the per-frame output buffer lands.
    lpOutputTakedownQueue->Clear();

    // Console PreWorldUpdate line 276: `*(gsm+249944) = 0` -- the module copy is cleared BEFORE the
    // IsSimPaused branch, so a paused frame never re-drains last frame's events (verify V3).
    mpTakedownCache->mTakedownEventQueue.Clear();
    if (lbSimPaused)
    {
        return;
    }

    // : the crashing-race-car scratch, built from the cached
    // VehicleOutputInterface (r24 == gsm+250816) exactly as the console builds it -- for every slot
    // the cache's mUsedRaceCars marks in use, that car's RaceCarState::mbCrashing. The cache is
    // filled at the post-world point by CacheTakedownManagerPostWorldInputData above.
    mpTakedownCache->mCrashingRaceCarInterface.SetFromVehicleOutputInterface(
        &mpTakedownCache->mVehicleOutputInterface);

    // [PC HARNESS, NOT X360] BRN_FORCE_TAKEDOWN=<seconds>: once the current mode has been IN_PROGRESS
    // for that long, fire the console's own "Force takedown" debug action (aggressor = car 0, victim
    // = car 1, STANDARD), so ProcessQueuedTakedowns -> ProcessTakedownEvent -> OnPlayerDoesATakedown
    // run deterministically on a scripted drive. Off unless the variable is set. DELETE-WHEN a
    // scripted ram can be relied on.
    {
        static bool sbForced = false;
        static const char* spcForce = getenv("BRN_FORCE_TAKEDOWN");
        if (!sbForced && spcForce != 0 && mpTakedownManager != 0)
        {
            const GameMode* lpMode = mModeManager.GetCurrentGameMode();
            if (lpMode != 0 && lpMode->GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS &&
                mModeManager.GetTimeInMode() >= static_cast<f32>(atof(spcForce)))
            {
                sbForced = true;
                mpTakedownManager->HarnessForceTakedown();
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "[td] HARNESS force-takedown fired (car 0 -> car 1) [FLAG PC harness]\n";
                }
            }
        }
    }

    // 0x823A59D4..0x823A59F4: the manager's tick. The console runs PreWorldUpdate under
    // LockBuffersForIO (DoUpdate_GameStatePreWorld), so the pre-world buffer's const accessors
    // (GetTakedownEventInputQueue asserts "Not locked for reading", BrnGameStateModuleIO.cpp:147)
    // see a read lock; the module's stand-in buffer is never locked by the seam, so the lock is
    // taken here for the duration of the tick. [FLAG PC bring-up: the lock is the console's, the
    // place it is taken is not.] The lock spans the two manager ticks below as well: both reach the
    // pre-world buffer through its read-locked accessors (the in-game player-status interface and
    // the network-to-game-state interface), exactly as they do inside the console's own lock.
    mpPreWorldInputBuffer->LockForRead();
    if (mpTakedownManager != 0)
    {
        mpTakedownManager->Update(&mLastActiveRaceCarInterface,
                                  lfGameTimestep,
                                  &mpTakedownCache->mRaceCarCrashEventQueue,
                                  &mpTakedownCache->mCrashingRaceCarInterface,
                                  mpPreWorldInputBuffer,
                                  mpOutputBuffer,
                                  &mpTakedownCache->mTrafficTypeResponseQueue);
    }

    // 0x823A59F8..0x823A5A10: `*(gsm+249944) = 0; TakedownEvent_::Append(gsm+249936, out's queue)`.
    mpTakedownCache->mTakedownEventQueue.Append(*lpOutputTakedownQueue);   // (the copy was cleared above)

    // The two managers the console ticks between the takedown-queue drain and ProcessTakedownEvents,
    // in that order and with the console's argument lists:
    //     MugshotManager::Update(gsm +0x500, preIn, out, cachedVehicleOutput, the module's takedown
    //                            queue copy, meCurrentGameModeType, <the pause bool>)
    //     PaybackManager::Update(gsm +0x570, preIn, out, cachedVehicleOutput, the same queue copy,
    //                            meCurrentGameModeType)
    // THE SEVENTH ARGUMENT IS RESOLVED. In the console leg r9 is `*(r14) != 0`, where r14 is a
    // stack slot holding a pointer the module computed far earlier in the same function -- the
    // module's own pause-reason bitfield, gsm +0x38B60 == miSimPauseFlags. Two independent reads
    // pin it: the same stack-held pointer feeds DriveThruManager::Update's pause argument earlier
    // in the tick, and it is dereferenced again right after this leg as RumbleManager::
    // UpdatePauseState's `lbPaused`. So the argument is `miSimPauseFlags != 0`, i.e. "is anything
    // paused" -- not the leg's own lbSimPaused (which is the stricter IsSimPaused answer the leg
    // already early-returned on).
    if (mpMugshotManager != 0)
    {
        mpMugshotManager->Update(mpPreWorldInputBuffer,
                                 mpOutputBuffer,
                                 &mpTakedownCache->mVehicleOutputInterface,
                                 &mpTakedownCache->mTakedownEventQueue,
                                 GetCurrentGameModeType(),
                                 miSimPauseFlags != 0);
    }

    if (mpPaybackManager != 0)
    {
        mpPaybackManager->Update(mpPreWorldInputBuffer,
                                 mpOutputBuffer,
                                 &mpTakedownCache->mVehicleOutputInterface,
                                 &mpTakedownCache->mTakedownEventQueue,
                                 GetCurrentGameModeType());
    }
    mpPreWorldInputBuffer->UnlockForRead();

    if (mpTakedownCache->mTakedownEventQueue.GetLength() > 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << "[td] " << mpTakedownCache->mTakedownEventQueue.GetLength()
                                   << " takedown event(s) this frame [FLAG PC witness]\n";
    }

    ProcessTakedownEvents(lpActionQueue, &mpTakedownCache->mTakedownEventQueue, mpOutputBuffer,
                          lrTimerStatusInterface);
}

// ProcessGameEvents @0x823A0A18 case 27 (POST_EVENT_LEAVE) ends with `TakedownManager::
// ClearRaceCarData(gsm+568)` -- wired. The console's other callers are ProcessGameEvents case 32
// (no such arm on this build yet) and GameStateModule::OnModeEnd @0x823767E0 (below, LIVE
// 2026-09-10). ClearAllTakedowns' host callers: OnModeFinish @0x82390EE0 (below) and the online
// case 18 (no such arm yet).
bool GameStateModule::IsInTakedownCamera() const
{
    return mpTakedownManager != 0 && mpTakedownManager->IsInTakedownCamera();
}

void GameStateModule::ClearTakedownRaceCarData()
{
    if (mpTakedownManager != 0)
    {
        mpTakedownManager->ClearRaceCarData();
    }
}


// ==============================================================================================
// GameStateModule::OnModeFinish  (X360 0x82390EE0) -- FinishCurrentMode @0x8234B978's last call.
//
//   0x82390EFC  var_30 = -1 ; var_2C = 0 ; var_2B = 0        (an 8-byte SetTakedownCameraAction:
//                                                             focus -1, active 0, signature 0)
//   0x82390F1C  AddEvent(lpOutputBuffer->GetGameActionQueue(), &var_30, 6, 8)
//   0x82390F30  TakedownManager::ClearAllTakedowns(this + 568, lpOutputBuffer->GetGameActionQueue())
//   0x82390F38  stb 0, this+46620 ; std 0, this+46448       == DriveThruManager (this+44240)
//               +0x94C mbDriveThroughsCloseWhenUsed / +0x8A0 maDriveThroughClosed:
//               DriveThroughsCanNowOpenAgain().
// ==============================================================================================
void GameStateModule::OnModeFinish(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    GameStateModuleIO::GameActionQueue* lpActionQueue = lpOutputBuffer->GetGameActionQueue();

    GameStateModuleIO::SetTakedownCameraAction lCameraOff;
    lCameraOff.meFocusOnRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID; // li r11, -1 -> var_30 (the GLOBAL enum -- two of this name are visible here)
    lCameraOff.mbActive              = false;                             // stb 0, var_2C
    lCameraOff.mbIsSignature         = false;                             // stb 0, var_2B
    lCameraOff.mbIsRevengeTakedown   = false;                             // +6 / +7: stack residue on
    lCameraOff.muPad07               = 0;                                 //  the console; zeroed here
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCameraOff),
                            GameStateModuleIO::E_ACTION_SET_TAKEDOWN_CAMERA_STATE,
                            static_cast<s32>(sizeof(lCameraOff)));

    // Embedded by value on the console; a pointer on this build (see ConstructTakedownBringUp),
    // guarded the same way ClearTakedownRaceCarData is.
    if (mpTakedownManager != 0)
    {
        mpTakedownManager->ClearAllTakedowns(lpActionQueue);
    }

    mDriveThruManager.DriveThroughsCanNowOpenAgain();
}

// ==============================================================================================
// GameStateModule::OnModeEnd  (X360 0x823767E0) -- SendModeStopMessages @0x8234BEC0's tail.
//
//   0x823767F4  MugshotManager::OnRoundEnd(this + 0x500, r4)
//   0x823767FC  PaybackManager::OnRoundEnd(this + 0x570, r4)
//     ✅ [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING] WIRED -- the "argument unrecovered" FLAG that
//     stood here is resolved. The console sets only r3 at these two calls because r4 is ALREADY the
//     argument: SendModeStopMessages loads `mr r4, r29` (== !lbOnlineLobbyHandover) @0x8234C6DC
//     before `bl OnModeEnd`, OnModeEnd never writes r4, and MugshotManager::OnRoundEnd @0x82357AF8
//     only reads it (`clrlwi r11, r4, 24`, no r4 store), so the same bool reaches both callees.
//     DWARF: OnModeEnd(bool) :609, both OnRoundEnd(bool lbResetState).
//   0x82376804  TakedownManager::ClearRaceCarData(this + 568)
//   0x82376808  lwz this+7604 == meCurrentGameModeType ; == 2 || == 16 (the two SHOWTIME modes) ->
//   0x82376838      stw 0, +284504   == mShowtimePendingTrafficIndexStack's count (Clear)
//   0x8237683C      stfs 2.0, +284444 == mfTimeSinceLastCrashMode (the post-mode lockout, re-armed)
//   0x82376840      sth -1, +284508  == muShowtimeRequestedTrafficIndex (K_INVALID_VEHICLE_INDEX)
//   0x82376848  stb 0, this+46620 ; std 0, this+46448   == DriveThroughsCanNowOpenAgain()
// ==============================================================================================
void GameStateModule::OnModeEnd(bool lbResetState)
{
    // Embedded by value on the console (+0x500 / +0x570: no test before either `bl`, 0x823767F4 /
    // 0x823767FC); held by pointer on this build and never null here, so no test here either
    // [FX-FLOW 2026-09-24, review D]: ConstructTakedownBringUp -- called from GameStateModule::
    // Construct -- assigns both from `new` and dereferences them on the spot (->Construct(this)),
    // nothing else ever writes mpMugshotManager, and mpPaybackManager is cleared only by
    // GameStateModule::Destruct, which has no ModeManager leg left to end a mode. OnModeEnd's one
    // caller is ModeManager::SendModeStopMessages, i.e. a running game.
    mpMugshotManager->OnRoundEnd(lbResetState);
    mpPaybackManager->OnRoundEnd(lbResetState);

    // [diag] BRN_MODEMGR_DIAG -- NOT IN THE X360 BINARY: the round-end hand-off.
    if (getenv("BRN_MODEMGR_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[payback] OnModeEnd(resetState " << (lbResetState ? 1 : 0)
            << ") -> MugshotManager / PaybackManager::OnRoundEnd\n";
    }

    ClearTakedownRaceCarData();

    const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
    if (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
        leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        mShowtimePendingTrafficIndexStack.Clear();
        mfTimeSinceLastCrashMode        = 2.0f;
        muShowtimeRequestedTrafficIndex = 0xFFFFu;
    }

    mDriveThruManager.DriveThroughsCanNowOpenAgain();
}

// ==============================================================================================
// GameStateModule::CopyInputDataToPaybackManager  (X360 0x8239AA78; DWARF BrnGameStateModule.h:835)
// [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING] Called by PreWorldUpdate @0x823A5328 at 0x823A572C, once
// per pre-world tick, unconditionally, right after DriveThruManager::Update. Both callees are
// inlined on the console:
//   0x8239AA98  PreWorldInputBuffer::GetTimerStatusInterface (0x8231CE28)
//   0x8239AAA0..0x8239AB0C  the 48-byte copy into PaybackManager +0x00  == SetTimerInterface
//   0x8239AB10  PreWorldInputBuffer::GetControllerInput (0x823632F8)
//   0x8239AB14..0x8239AB58  `lbz 0xC` (mbDirtyTrickPressed) -> +0x264/+0x265 and the press-edge
//                           ChangeState(5)                                 == SetDirtyTrickButtonState
// Without it the payback manager's timer copy never left Construct's Clear(): its aggressor timer
// never advanced and the OnRoundStart / OnRoundEnd reseed read frame count 0; and the dirty-trick
// button never reached the aggressor FSM.
//
// THE TIMER TYPE. The buffer's block is GameStateModuleIO::TimerStatusInterface, this tree's
// padding fork of CgsSystem::TimerStatusInterface (BrnGameStateModuleIO.h; BrnGameModule.cpp's
// publish banner): the same 48 bytes -- two {frame count, base step, multiplier, running, time}
// runs -- under a second declaration. The DWARF types the buffer member and SetTimerInterface's
// parameter as the one CgsSystem type, and CgsSystem::TimerStatus keeps its members private, so the
// block is handed over AS that type (a re-type of the same object, pinned by the size assert) and
// copied by the real TimerStatusInterface::operator=, exactly the member-wise copy the console
// inlines. DELETE-WHEN the fork is retired in favour of the CgsSystem type.
// ==============================================================================================
void GameStateModule::CopyInputDataToPaybackManager(
        const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer)
{
    static_assert(sizeof(GameStateModuleIO::TimerStatusInterface) == sizeof(CgsSystem::TimerStatusInterface),
                  "the pre-world timer block and CgsSystem::TimerStatusInterface are one 48-byte object");

    // Embedded by value on the console (this + 0x570, used with no test); a pointer on this build,
    // never null here (see OnModeEnd above: set from `new` in GameStateModule::Construct's
    // ConstructTakedownBringUp, cleared only by Destruct) -- the one caller is the pre-world tick.

    mpPaybackManager->SetTimerInterface(reinterpret_cast<const CgsSystem::TimerStatusInterface*>(
        lpPreWorldInputBuffer->GetTimerStatusInterface()));
    mpPaybackManager->SetDirtyTrickButtonState(
        lpPreWorldInputBuffer->GetControllerInput()->mbDirtyTrickPressed);

    // [diag] BRN_MODEMGR_DIAG -- NOT IN THE X360 BINARY. One line, the first time a running game
    // timer arrives: the witness that the copy is dispatched and carries a live frame count.
    static const bool sbPaybackDiag = (getenv("BRN_MODEMGR_DIAG") != 0);
    static bool       sbReported    = false;
    const GameStateModuleIO::TimerStatusInterface::Entry& lrGameTimer =
        lpPreWorldInputBuffer->GetTimerStatusInterface()->maEntries[0];
    if (sbPaybackDiag && !sbReported && lrGameTimer.miWord00 != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        sbReported = true;
        *CgsDev::Log::gpDebugPrint
            << "[payback] CopyInputDataToPaybackManager: game timer frame " << lrGameTimer.miWord00
            << " step " << lrGameTimer.mfValue04 * lrGameTimer.mfValue08 << " -> PaybackManager\n";
    }
}

}
