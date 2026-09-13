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
// takedown queue) -> [MugshotManager::Update @gsm+1280 / PaybackManager::Update @gsm+1392 -- see
// the note at the leg] ->
// ProcessTakedownEvents(actionQ, gsm+249936, out).
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameStateTakedownCache.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
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
    if (mpTakedownManager == 0 || mpTakedownCache == 0)
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
        if (!sbForced && spcForce != 0)
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
    // place it is taken is not.]
    mpPreWorldInputBuffer->LockForRead();
    mpTakedownManager->Update(&mLastActiveRaceCarInterface,
                              lfGameTimestep,
                              &mpTakedownCache->mRaceCarCrashEventQueue,
                              &mpTakedownCache->mCrashingRaceCarInterface,
                              mpPreWorldInputBuffer,
                              mpOutputBuffer,
                              &mpTakedownCache->mTrafficTypeResponseQueue);
    mpPreWorldInputBuffer->UnlockForRead();

    // 0x823A59F8..0x823A5A10: `*(gsm+249944) = 0; TakedownEvent_::Append(gsm+249936, out's queue)`.
    mpTakedownCache->mTakedownEventQueue.Append(*lpOutputTakedownQueue);   // (the copy was cleared above)

    // [X] STILL PARKED, re-measured 2026-09-13 -- and the old wording ("neither manager exists on
    // this build") was wrong in one half and right in the other:
    //  * BOTH CLASSES ARE BODIED. MugshotManager::Update is BrnMugshotManager.cpp and
    //  PaybackManager::Update is BrnPaybackManager.cpp, both carrying the console's own
    //     signature -- whose third parameter is `const VehicleOutputInterface*`, i.e. r24, the very
    //     cache this file now owns.
    //   * WHAT IS ACTUALLY MISSING IS OWNERSHIP AND THE MOUNT, both outside this lane:
    //       (a) GameStateModule has no member for either manager (the console embeds them at
    //           gsm+1280 / gsm+1392) and no Construct call for them, so there is nothing to tick;
    //       (b) BrnPaybackManager.cpp / BrnPaybackDebugComponent.cpp are NOT mounted in the exe
    //           build list, so calling PaybackManager::Update would be an unresolved external for
    //           the whole build. (BrnMugshotManager.cpp IS mounted.)
    // Console position, for whoever lands them ( /):
    //     MugshotManager::Update(gsm+1280, preIn, out, cachedVehicleOutput, gsm+249936,
    //                            meCurrentGameModeType, <pause flag>);
    //     PaybackManager::Update(gsm+1392, preIn, out, cachedVehicleOutput, gsm+249936,
    //                            meCurrentGameModeType);
    // The Mugshot call's 7th argument (r9) is `*(r14) != 0` off a stack-held pointer
    // this lane did not resolve -- pin it before wiring, do not guess it.

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
//   0x823767F0  MugshotManager::OnRoundEnd(this + 1280)     -- neither manager exists on this
//   0x823767F8  PaybackManager::OnRoundEnd(this + 1392)        build; named, not faked (the same
//                                                              two TakedownPreWorldLeg names)
//   0x82376800  TakedownManager::ClearRaceCarData(this + 568)
//   0x82376808  lwz this+7604 == meCurrentGameModeType ; == 2 || == 16 (the two SHOWTIME modes) ->
//   0x82376838      stw 0, +284504   == mShowtimePendingTrafficIndexStack's count (Clear)
//   0x8237683C      stfs 2.0, +284444 == mfTimeSinceLastCrashMode (the post-mode lockout, re-armed)
//   0x82376840      sth -1, +284508  == muShowtimeRequestedTrafficIndex (K_INVALID_VEHICLE_INDEX)
//   0x82376848  stb 0, this+46620 ; std 0, this+46448   == DriveThroughsCanNowOpenAgain()
// ==============================================================================================
void GameStateModule::OnModeEnd()
{
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

}
