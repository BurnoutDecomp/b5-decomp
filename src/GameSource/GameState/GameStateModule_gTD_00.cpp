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
//   gsm+250816  CrashingRaceCarInterface             built by SetFromVehicleOutputInterface from the
//                                                    cached VehicleOutputInterface (gsm+250832)
// This build has neither a PostWorldInputBuffer nor a per-frame output buffer (mpOutputBuffer is
// new'd once), so the same state lives in a heap-allocated TakedownPostWorldCache (mpTakedownCache)
// and the manager itself is heap-allocated (mpTakedownManager) -- the mpTrainingManager precedent:
// BrnGameStateModule.h cannot include BrnTakedownManagerTypes.h (its second EActiveRaceCarIndex
// would re-bind the widely-included header), so the complete types are confined to this TU.
// FLAG PC deviation: pointers where the console embeds; the bodies below are the console's.
//
// Console positions reproduced here (GameStateModule::PreWorldUpdate @0x823A5328, !IsSimPaused arm,
// asm 0x823A59C8..0x823A5A10): SetFromVehicleOutputInterface(stack, cachedVehicleOutput) ->
// TakedownManager::Update(this+0x238, activeIf, dt, crashQ, &crashingIf, preIn, out, trafficTypeQ)
// -> `*(gsm+249944) = 0` (the module copy's miLength) + TakedownEvent_::Append(gsm+249936, out's
// takedown queue) -> [MugshotManager / PaybackManager::Update -- absent on this build] ->
// ProcessTakedownEvents(actionQ, gsm+249936, out).
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameStateTakedownCache.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
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

// X360 CacheTakedownManagerPostWorldInputData @0x82375E70 (post-world #18): the crash queue is
// Clear()ed (`*(gsm+250280) = 0`) and Append()ed from the PostWorldInputBuffer's copy of the world
// output's VehicleManagerOutputInterface crash queue. This build hands the world output's queues in
// directly (the argument is the deviation, not the body). The traffic-type response queue is NOT
// part of that function: Update's 7th argument is gsm+278480 (r26 @0x823A5878), a
// TrafficTypeResponse<32> queue the module owns, Constructed @0x82380388 and Clear+Append'ed in
// GameStateModule::PostWorldUpdate @0x8238F358 (verify V3). Cached here at the same post-world point.
void GameStateModule::CacheTakedownPostWorldInputs(
        const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>* lpRaceCarCrashEventQueue,
        const CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse>* lpTrafficTypeResponseQueue)
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

    // 0x823A59C8..0x823A59D0: the crashing-race-car scratch, from the cached VehicleOutputInterface.
    // That cache (gsm+250832, an 8960-byte X360 copy) is not modelled here; Update only asserts the
    // pointer non-null (BrnTakedownManager.cpp:182) and none of its callees receive it, so the
    // Clear()ed interface is passed. [FLAG] SetFromVehicleOutputInterface not run on this build.
    mpTakedownCache->mCrashingRaceCarInterface.Clear();

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

    // [MugshotManager::Update @gsm+1280 / PaybackManager::Update @gsm+1392 sit here on the console;
    //  neither manager exists on this build -- named, not faked.]

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
// (no such arm on this build yet) and GameStateModule::OnModeEnd @0x823767E0 (parked). ClearAllTakedowns
// (OnModeFinish @0x82390EE0 -- parked -- and the online case 18) has no host caller yet.
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

}
