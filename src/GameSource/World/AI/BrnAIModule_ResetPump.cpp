// =================================================================================================
// BrnAIModule_ResetPump.cpp -- the AI MODULE's half of the reset-on-track round trip
// (resetpump wave, 2026-08-26).
//
//   BrnAI::AIModule::Update                    @0x8279B478  (a MINIMAL-COMPLETE SLICE)
//   BrnAI::AIModule::ProcessRequestInterface   @0x8278A7A8  (the request drain; a SLICE)
//   BrnAI::AIModule::UpdateResetOnTrackManager @0x8279ABB0  (the manager tick; a SLICE)
//   BrnAI::AIModule::GetAICar                  @0x82765AD0  (whole)
//
// ⭐⭐⭐ WHY Update IS A SLICE AND NOT A STUB, AND WHY THE SLICE IS THE RIGHT SHAPE
// The console's Update is a 25-callee per-frame spine over the whole opponent AI: driver
// updates, route requests, traffic sorting, in/out-of-range vehicle export, BuzzBy. Every one of
// those callees is ABSENT from this tree and most of them reach the AIDriver / AICar interiors
// that BrnAICar.h still models as explicit pads. Reproducing the spine would mean 6,000+ lines
// of code that cannot run.
//
// What CAN run, today, is the reset-on-track pump -- and it is a self-contained sub-chain of the
// same body, in the console's own order:
//     ProcessRequestInterface(input, output, updateSet)      (console call at 0x8279B838)
//     lfTime = simTimer.mTime.seconds + simTimer.mTime.fraction   (0x8279B844..0x8279B86C)
//     UpdateResetOnTrackManager(output->GetAIModuleResultInterface(), lfTime)  (0x8279B87C)
// Both calls sit INSIDE the console's `if (mbPlayerDataSet)` gate, which this slice reproduces
// rather than skips -- that flag is the console's own "is there a player car to reason about",
// and RCEM::WriteUpdatedAIData (landed this wave) is the only thing that raises it.
//
// ⛔ [FLAG PC bring-up] THE FOURTEEN LEGS THIS SLICE DOES NOT RUN, named so nobody re-derives
// the list: PausedUpdate @0x8279A1E0 · the transient RouteMapModuleIO "Route" input buffer
// (created at 0x8279B4F8, destroyed at the tail) · HandleGameActions @0x82791FD0 ·
// HandleManagementEvents @0x82798620 · StoreDrivenCarData @0x827957F0 · SortTrafficIntoAICars
// @0x8278A970 · the RaceRouteRequest append · UpdateCars @0x8279A518 · RouteRequestManager::Update
// @0x82797FA8 · RouteMapModule::Update (vtbl+68) · UpdateCarRoutes @0x827955F0 · the RouteResponse
// append · UpdateDrivers @0x8279B148 · ProcessAIVehicleInputs @0x82795E10 ·
// ProcessOutOfRangeVehicles @0x8276E910 · ProcessInRangeVehicles @0x8276EA18 · ExportCarData
// @0x8276EB28 · BuzzBy::Update @0x8278B8C8 and its game-event 113 tail. NONE of them is on the
// reset-on-track path, and every one of them is an ABSENT function, not a dropped call.
// DELETE-WHEN the AIDriver/AICar stack lands.
//
// ⚠️ THE PERF MONITOR (dword_82F30154 / dword_82F30168) IS NOT STARTED. Its AddMonitor never
// runs on this build (AIModule::Construct's monitor block is parked), so starting a monitor on
// handle 0 would time an unnamed row. Same disposition as ResetOnTrackManager::Construct's.
// =================================================================================================

#include "GameSource/World/AI/BrnAIModule.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                // AIModuleIO::InputBuffer
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"   // AIModuleIO::OutputBuffer
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"  // AIModuleRequestInterface
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"   // AIModuleResultInterface
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"       // RaceCarAIInterface
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnAI
{

// =================================================================================================
// GetAICar @0x82765AD0 -- `assert(index <= 0x22)` then `return 5472 * index + this + 560`, i.e.
// &maAICars[index]. The console's baked 5472 IS sizeof(AICar) on this host too (static_asserted
// in BrnAICar.h), so the subscript below and the console's arithmetic are the same address.
// The assert is the console's own streamed "Invalid AI car index: " tripwire (BrnAIModule.h:445),
// lowered to a static-message CGS_ASSERT per the standing rule. NON-GATING on the console; the
// bail below is the PC deviation, because on the host an out-of-range index walks off a 191,520
// byte array instead of into the module's own tail.
// =================================================================================================
AICar* AIModule::GetAICar(u32 luIndex) const
{
    CGS_ASSERT(luIndex < 35u, "Invalid AI car index: ");   // BrnAIModule.h:445
    if (luIndex >= 35u)
    {
        return 0;
    }
    return const_cast<AICar*>(&maAICars[luIndex]);
}

// =================================================================================================
// ProcessRequestInterface @0x8278A7A8 -- a SLICE (the request drain, whole).
//
//   0x8278A7C0  requests = lpInputBuffer->GetAIModuleRequestInterface()
//   0x8278A7D8  for (i = 0; i < requests->mResetOnTrackRequestQueue.miLength; ++i)
//   0x8278A7E4    mResetOnTrackManager.PushResetOnTrackRequest(&queue.GetEvent(i))
//   0x8278A80C  then an 8-slot AI-DRIVER sweep: for each active-car slot whose AIDriver is live
//               and IsStuck(), and only when the frame is NOT paused, build a synthetic
//               ResetOnTrackRequest (type/speed chosen by the driver's AICar state) and push it
//               too, then zero the driver's stuck timer.
//
// ⛔ [FLAG PC bring-up] THE AI-DRIVER SWEEP IS PARKED and it is UNREACHABLE: it opens with
// `GetAIDriver(i)->[+7529]`, and neither AIModule::GetAIDriver @0x82765B90 nor AIDriver::IsStuck
// @0x82766670 nor the eight AIDriver objects exist in this tree (AIDriver::Prepare is the parked
// leg of AIModule::Prepare stage 4). It is the AI's "an opponent got stuck, put it back" path --
// nothing to do with the player's crash recovery, which arrives through the queue above.
// DELETE-WHEN the AIDriver stack lands.
// =================================================================================================
void AIModule::ProcessRequestInterface(const AIModuleIO::InputBuffer* lpInputBuffer,
                                       AIModuleIO::OutputBuffer* lpOutputBuffer,
                                       BrnUpdateSet lUpdateSet)
{
    (void)lpOutputBuffer;   // console a3 -- read only by the parked AI-driver sweep
    (void)lUpdateSet;       // console a4 -- the sweep's "not paused" gate

    CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer != NULL");
    if (lpInputBuffer == 0)
    {
        return;
    }

    const AIModuleIO::AIModuleRequestInterface* lpRequests =
        lpInputBuffer->GetAIModuleRequestInterface();
    if (lpRequests == 0)
    {
        return;
    }

    const AIModuleIO::AIModuleRequestInterface::ResetOnTrackRequestQueue* lpQueue =
        lpRequests->GetResetOnTrackRequestQueue();

    for (s32 liRequest = 0; liRequest < lpQueue->GetLength(); ++liRequest)
    {
        mResetOnTrackManager.PushResetOnTrackRequest(&lpQueue->GetEvent(liRequest));

        // [DIAG resetpump] NOT IN THE X360 BINARY. The middle witness of the three-point pair
        // the race-car module opens and closes: "the request left RCEM", "the AI RECEIVED it",
        // "a result came back". Without the middle one a lost request and a refused request
        // look identical in the log.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[resetpump] request RECEIVED by the AI module: global car "
                << static_cast<s32>(lpQueue->GetEvent(liRequest).GetGlobalRaceCarIndex())
                << " type " << static_cast<s32>(lpQueue->GetEvent(liRequest).GetResetType())
                << "\n";
        }
    }
}

// =================================================================================================
// UpdateResetOnTrackManager @0x8279ABB0 -- a SLICE (the manager tick, whole).
//
//   0x8279ABD4  StartMonitor(dword_82F30168)                                   [PARKED]
//   0x8279ABE0  if (!GetAICar(mePlayerGlobalRaceCarIndex)) skip everything
//   0x8279ABF4  lePlayer = GetAICar(mePlayerGlobalRaceCarIndex)->miRaceCarIndex   (AICar +0x14C4)
//   0x8279AC00  Camera::Camera(<stack>, this + 322048)                          [PARKED]
//   0x8279AC0C  mResetOnTrackManager.Update(lpResults, lePlayer, lfTime)        ⭐ REPRODUCED
//   0x8279AC24  for each published ResetOnTrackResult: fetch the AI car it names, re-seed its
//               route state, and either log "<AI> Transform NOT set" or store the new transform
//               and call AICar::SetDirection                                    [PARKED]
//
// ⛔ [FLAG PC bring-up] THE RESULT FAN-OUT IS PARKED. It writes ~13 members of the AICar's
// route/driver block (offsets +7168..+7272 off the car's own driver pointer at +5296) that have
// no names in BrnAICar.h, and calls AICar::SetDirection @0x8276B2C0, which is absent. It is how
// the AI's own model of a reset car catches up -- and every AICar on this build is INACTIVE, so
// there is nothing to catch up. The PLAYER's recovery does not go through here at all: it goes
// through the ResetOnTrackResult ring the manager just wrote, which
// WorldModule::BridgeAIToEntityModules_PrePhysics carries to the race-car module.
//
// ⚠️ lePlayer IS NOT "the player's global race-car index" on this build -- see the banner on
// mePlayerGlobalRaceCarIndex in BrnAIModule.h. It is 0, which is what the console's own .bss
// holds when the AI-driver writer arm never runs, and the manager uses it only for range
// asserts. The request itself carries the real index.
// =================================================================================================
void AIModule::UpdateResetOnTrackManager(AIModuleIO::AIModuleResultInterface* lpResults,
                                         f32 lfTime)
{
    CGS_ASSERT(lpResults != 0, "lpResults != NULL");
    if (lpResults == 0)
    {
        return;
    }

    const AICar* lpPlayerAICar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
    if (lpPlayerAICar == 0)
    {
        return;
    }

    // [FLAG PC bring-up] the Camera copy at this+322048 -- ResetOnTrackManager's own mCamera
    // member is parked in that class too (see its Update banner).

    // ⚠️ [FLAG PC bring-up] THE RANGE GUARD IS THE DEVIATION, and it exists because the
    // console's next act is two HALTING asserts. ResetOnTrackManager::Update stores this value
    // and immediately calls its own GetAICar on it, which asserts 0 <= index < 35. On the
    // console miRaceCarIndex is .bss-zero until AICar::Construct (an ARTIST export hole) fills
    // it; AIModule::Construct now seeds it to that same zero, so this guard should never fire.
    // It is here because a dev assert on the PC blocks the game waiting for END -- an
    // out-of-range value would hang the run rather than mis-place one car.
    s32 liPlayer = lpPlayerAICar->GetRaceCarIndex();
    if (liPlayer < 0 || liPlayer >= 35)
    {
        static bool sbReportedBadPlayerIndex = false;
        if (!sbReportedBadPlayerIndex && CgsDev::Log::gpDebugPrint != 0)
        {
            sbReportedBadPlayerIndex = true;
            *CgsDev::Log::gpDebugPrint
                << "[resetpump] AICar[" << static_cast<s32>(mePlayerGlobalRaceCarIndex)
                << "].miRaceCarIndex is " << liPlayer
                << ", out of [0,35) -- clamping to 0 for ResetOnTrackManager::Update's range"
                   " asserts. AICar::Construct is an export hole; see AIModule::Construct.\n";
        }
        liPlayer = 0;
    }

    mResetOnTrackManager.Update(
        lpResults,
        static_cast<EGlobalRaceCarIndex>(liPlayer),
        lfTime);

    // [FLAG PC bring-up] the AI-car result fan-out -- see the banner.
}

// =================================================================================================
// Update @0x8279B478 -- THE MINIMAL-COMPLETE SLICE. See the file banner for what it does not run.
//
// Console order, with the reproduced steps starred:
//   0x8279B4B0  if (lUpdateSet & 1) return PausedUpdate()                        [PARKED]
//   0x8279B4C4  StartMonitor(dword_82F30154)                                     [PARKED]
//   0x8279B4D0  assert(lpInputBufferStack)  ...  :611                          ⭐
//   0x8279B4E4  assert(lpOutputBufferStack) ...  :612                          ⭐
//   0x8279B4F0  assert(lpInputBuffer)       ...  :613                          ⭐
//   0x8279B4F4  assert(lpOutputBuffer)      ...  :614                          ⭐
//   0x8279B4F8  the transient RouteMapModuleIO "Route" input buffer              [PARKED]
//   0x8279B558  lpInputBuffer->LockForRead(); lpOutputBuffer->LockForWrite()    ⭐
//   0x8279B57C  if (lpInputBuffer->GetRaceCarAIInterface()->mbPlayerDataSet)    ⭐ THE GATE
//   0x8279B5A4    dt = simTimer.mfTimeStepMultiplier * simTimer.mfBaseTimeStep   [PARKED with
//                                                                                its consumers]
//   0x8279B5C8    the player pose/direction reads                                [PARKED]
//   0x8279B5F8    mePlayerActiveRaceCarIndex = ai->GetPlayerActiveRaceCarIndex()⭐
//   0x8279B640    the AI-driver arm that writes mePlayerGlobalRaceCarIndex       [PARKED]
//   ... fourteen legs ...                                                        [PARKED]
//   0x8279B838    ProcessRequestInterface(lpInputBuffer, lpOutputBuffer, lUpdateSet)  ⭐
//   0x8279B844    lfTime = (f32)simTimer.mTime.miSeconds + simTimer.mTime.mfFraction  ⭐
//   0x8279B87C    UpdateResetOnTrackManager(lpOutputBuffer->GetAIModuleResultInterface(), lfTime) ⭐
//   ... five more legs ...                                                       [PARKED]
//   0x8279B9C0  UnlockForRead / UnlockForWrite / StopMonitor / DestroyIOBuffer  ⭐ (locks only)
//
// ⚠️ THE TIME ARGUMENT IS AN ABSOLUTE TIME, NOT A DELTA, and the two are three instructions
// apart in the console. `lwz r10, 0x28(timer) ; lfs f0, 0x2C(timer) ; ... fcfid ; frsp ;
// fadds f30, f13, f0` -- +0x28/+0x2C are the SIM TimerStatus's mTime.{miSeconds, mfFraction}
// (the sim block starts at +24, mTime at +16). The manager uses it to age its recent-reset ring
// against a 3.0 s window, so feeding it the frame delta instead would make that ring never age.
// =================================================================================================
void AIModule::Update(CgsModule::IOBufferStack* lpInputBufferStack,
                      CgsModule::IOBufferStack* lpOutputBufferStack,
                      const AIModuleIO::InputBuffer* lpInputBuffer,
                      AIModuleIO::OutputBuffer* lpOutputBuffer,
                      BrnUpdateSet lUpdateSet)
{
    if ((lUpdateSet & 1) != 0)
    {
        // [FLAG PC bring-up] AIModule::PausedUpdate @0x8279A1E0 -- absent. It runs the same
        // ProcessRequestInterface drain plus the paused-frame route bookkeeping. Dropping it
        // means a request posted on a PAUSED frame waits for the next running frame; the crash
        // exit never posts on a paused frame (the crash module's tick is itself pause-gated).
        return;
    }

    CGS_ASSERT(lpInputBufferStack  != 0, "lpInputBufferStack != NULL");    // X360 :611
    CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");   // X360 :612
    CGS_ASSERT(lpInputBuffer       != 0, "lpInputBuffer != NULL");         // X360 :613
    CGS_ASSERT(lpOutputBuffer      != 0, "lpOutputBuffer != NULL");        // X360 :614

    (void)lpInputBufferStack;    // used only by the parked "Route" IO-buffer pair
    (void)lpOutputBufferStack;

    if (lpInputBuffer == 0 || lpOutputBuffer == 0)
    {
        return;
    }

    lpInputBuffer->LockForRead();
    lpOutputBuffer->LockForWrite();

    const AIModuleIO::RaceCarAIInterface* lpRaceCarAI = lpInputBuffer->GetRaceCarAIInterface();

    if (lpRaceCarAI != 0 && lpRaceCarAI->mbPlayerDataSet)
    {
        mePlayerActiveRaceCarIndex = lpRaceCarAI->GetPlayerActiveRaceCarIndex();

        // [FLAG PC bring-up] the arm that writes mePlayerGlobalRaceCarIndex needs the AI driver
        // chain -- see the member's banner in BrnAIModule.h.

        {
            // [DIAG resetpump] NOT IN THE X360 BINARY. ONE line, the first time the gate opens.
            // It is the witness for the exact thing that blocked the previous wave: this body
            // runs at all only because RCEM::WriteUpdatedAIData now sets mbPlayerDataSet.
            static bool sbReportedFirstOpen = false;
            if (!sbReportedFirstOpen && CgsDev::Log::gpDebugPrint != 0)
            {
                sbReportedFirstOpen = true;
                *CgsDev::Log::gpDebugPrint
                    << "[resetpump] AIModule::Update: mbPlayerDataSet is SET -- the AI body is "
                       "running (player active car "
                    << static_cast<s32>(mePlayerActiveRaceCarIndex) << ")\n";
            }
        }

        ProcessRequestInterface(lpInputBuffer, lpOutputBuffer, lUpdateSet);

        const CgsSystem::TimerStatusInterface* lpTimers = lpInputBuffer->GetTimerInterface();
        if (lpTimers != 0)
        {
            const CgsSystem::Time lSimTime = lpTimers->GetSimTimerStatus()->GetTime();
            const f32 lfTime = static_cast<f32>(lSimTime.GetSeconds()) + lSimTime.GetFraction();

            UpdateResetOnTrackManager(
                reinterpret_cast<AIModuleIO::AIModuleResultInterface*>(
                    lpOutputBuffer->GetAIModuleResultInterfaceForWrite()),
                lfTime);
        }
    }

    lpInputBuffer->UnlockForRead();
    lpOutputBuffer->UnlockForWrite();
}

}   // namespace BrnAI
