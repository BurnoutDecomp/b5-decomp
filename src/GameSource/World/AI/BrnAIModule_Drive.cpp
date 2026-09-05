// =================================================================================================
// BrnAIModule_Drive.cpp -- the DRIVE legs of BrnAI::AIModule's per-frame spine
// (aiwave lane A1, 2026-09-03).
//
//   BrnAI::AIModule::GetAIDriver              @0x82765B90  (whole)
//   BrnAI::AIModule::GetAISectionsData        @0x8277BC00  (whole; the host read of the same handle)
//   BrnAI::AIModule::GetMasterRoute           (inlined-away DWARF accessor; &mMasterRoute)
//   BrnAI::AIModule::UpdateCars               @0x8279A518  (whole)
//   BrnAI::AIModule::UpdateOneProximityIndex  @0x8276E660  (whole)
//   BrnAI::AIModule::DoRoundRobins            @0x82798540  (whole)
//   BrnAI::AIModule::RoundRobinDrivers        @0x82798408  (whole; the work call is an A3 seam)
//   BrnAI::AIModule::UpdateDrivers            @0x8279B148  (whole)
//   BrnAI::AIModule::SetSuitabilityForAggression @0x8276E7C0 (whole; behind the A3 seam)
//   BrnAI::AIModule::StoreDrivenCarData       @0x827957F0  (whole)
//   BrnAI::AIModule::SortTrafficIntoAICars    @0x8278A970  (whole; behind the A3 seam)
//   BrnAI::AIModule::ProcessAIVehicleInputs   @0x82795E10  (whole)
//   BrnAI::AIModule::ProcessOutOfRangeVehicles @0x8276E910 (whole)
//   BrnAI::AIModule::ProcessInRangeVehicles   @0x8276EA18  (whole)
//   BrnAI::AIModule::ExportCarData            @0x8276EB28  (whole)
//
// =================================================================================================
// THE CONSOLE-ORDERED CALL SEQUENCE OF AIModule::Update @0x8279B478 (all callees, in order).
// The conductor wires this into BrnAIModule_ResetPump.cpp's Update slice; nothing here edits it.
// Status: LANDED = this file · EXISTS = already in the tree (file named) · ABSENT = owner lane/park.
//
//  #  asm         callee                                                          status
//  1  0x8279B4B0  if (lUpdateSet & 1) return PausedUpdate(...)      @0x8279A1E0   ABSENT (park; the
//                                                                                  paused-frame drain)
//  2  0x8279B4C4  CgsDev::PerfMonCpu::StartMonitor(dword_82F30154)                 ABSENT (perf monitors
//                                                                                  never AddMonitor'd)
//  3  0x8279B4D0  the four `!= NULL` asserts (:611..:614)                          EXISTS (ResetPump)
//  4  0x8279B4F8  RouteMapModuleIO::InputBuffer helper ctor(&lRouteIn, inStack,
//                 "Route")  @0x82794B80 -- CreateIOBuffer<RouteMapModuleIO::
//                 InputBuffer> on the INPUT stack                                   ABSENT (Route lane;
//                                                                                  BrnRouteMapModuleIO_IOHelpers.cpp
//                                                                                  may carry it)
//  5  0x8279B558  lpInputBuffer->LockForRead(); lpOutputBuffer->LockForWrite()     EXISTS (ResetPump)
//  6  0x8279B57C  if (GetRaceCarAIInterface()->mbPlayerDataSet == 1)  THE GATE      EXISTS (ResetPump)
//  7  0x8279B5A4  lfDt = timer->GetSimTimerStatus()->mfTimeStepMultiplier(+32)
//                       * ->mfBaseTimeStep(+28)   (SIM block = +24; +4/+8 inside)  EXISTS (types;
//                                                                                  expression to add)
//  8  0x8279B5C8  lPlayerPos = RaceCarAIInterface::GetPlayerCarPosition()  (v127)  EXISTS (BrnRaceCarAIInterfaces.cpp)
//  9  0x8279B5E0  RaceCarAIInterface::GetPlayerCarDirection()  (result unused)     EXISTS
// 10  0x8279B5F8  mePlayerActiveRaceCarIndex = GetPlayerActiveRaceCarIndex()       EXISTS (ResetPump)
// 11  0x8279B60C  if (GetAIDriver(active)->mbIsActive)
//                   mePlayerGlobalRaceCarIndex = driver->mpCar ? mpCar->miRaceCarIndex : -1
//                   (asm 0x8279B640..0x8279B6A0; reads +7392/+7529/+5316)           LANDED (GetAIDriver;
//                                                                                  the arm itself is 6 lines
//                                                                                  for the conductor)
// 12  0x8279B6B0  lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);
//                 if (+270920 && !+270921)  +270912 += (player->mbIsCrashing(+5442)
//                                           ? lfDt*0.5 : lfDt)   (the AIDebugComponent's
//                 time accumulator @+270908.. -- BrnAIDebugComponent.h)              ABSENT (debug
//                                                                                  component; drop-safe)
// 13  0x8279B6F0  CgsModule::LockBuffersForIO(lRouteIn.mpBuffer)                    ABSENT (with #4)
// 14  0x8279B6FC  HandleGameActions(this, in, out, lRouteIn)        @0x82791FD0     ABSENT (owner lane:
//                                                                                  mode start/end/
//                                                                                  checkpoint actions)
// 15  0x8279B70C  CgsModule::UnlockBuffersForIO(lRouteIn.mpBuffer)                  ABSENT (with #4)
// 16  0x8279B714  HandleManagementEvents(this, in)                  @0x82798620     ABSENT (owner lane:
//                                                                                  the activate/attach
//                                                                                  ring -- THIS is what
//                                                                                  raises mbIsActive)
// 17  0x8279B720  StoreDrivenCarData(this, in)                      @0x827957F0     LANDED
// 18  0x8279B72C  SortTrafficIntoAICars(this, in)                   @0x8278A970     LANDED (A3 seam)
// 19  0x8279B738  sub_82794BE8(&lRouteOut, outStack, "Route") -- the OUTPUT-stack
//                 RouteMapModuleIO::OutputBuffer helper ctor                         ABSENT (with #4)
// 20  0x8279B748  LockBuffersForIO(lRouteIn); RouteMapModuleIO::RaceRouteRequest
//                 Queue::Append(lRouteIn->GetRaceRouteRequestQueue(),
//                               in->GetRaceRouteRequestQueue())     @0x8277B588     EXISTS (BaseEventQueue
//                                                                                  _RaceRouteRequest.cpp)
// 21  0x8279B774  UpdateCars(this, lfDt, lRouteIn, out)             @0x8279A518     LANDED
// 22  0x8279B77C  RouteRequestManager::Update(&mRouteRequestManager(+270952), maAICars,
//                 GetAICar(playerGlobal), GetAISectionsData(), lRouteIn, &mBuzzBy)
//                                                                   @0x82797FA8     EXISTS (BrnRouteRequestManager.cpp:453)
//                                                                                  -- mRouteRequestManager has
//                                                                                  NO named member yet (:329)
// 23  0x8279B7A4  UnlockBuffersForIO(lRouteIn)                                      ABSENT (with #4)
// 24  0x8279B7B4  mRouteMapModule.Update(inStack, outStack, lRouteIn, lRouteOut)
//                 (vtbl+68)                                                         EXISTS? (BrnRouteMapModule.cpp;
//                                                                                  Route lane)
// 25  0x8279B7C0  lRouteOut->LockForRead(); UpdateCarRoutes(this, out, lRouteOut)
//                                                                   @0x827955F0     ABSENT (owner lane:
//                                                                                  route responses -> AICar::
//                                                                                  UpdateRoute)
// 26  0x8279B7D8  RouteResponseQueue::Append(out->GetRouteResponseQueueForWrite(),
//                 lRouteOut->GetRouteResponseQueue()(sub_8276B148)) @0x8277B668     EXISTS (BaseEventQueue
//                                                                                  _RouteResponse.cpp)
// 27  0x8279B7F0  lRouteOut->UnlockForRead(); RouteMapModuleIO helper dtor(&lRouteOut)
//                                                                                  ABSENT (with #4)
// 28  0x8279B808  UpdateDrivers(this, in, out, v1 = lPlayerPos, f1 = lfDt) @0x8279B148  LANDED
// 29  0x8279B838  ProcessRequestInterface(this, in, out, lUpdateSet) @0x8278A7A8    EXISTS (ResetPump)
// 30  0x8279B844  lfTime = (f32)sim.mTime.miSeconds + sim.mTime.mfFraction          EXISTS (ResetPump)
// 31  0x8279B87C  UpdateResetOnTrackManager(out->GetAIModuleResultInterface(), lfTime)
//                                                                   @0x8279ABB0     EXISTS (ResetPump)
// 32  0x8279B884  ProcessAIVehicleInputs(this, out)                 @0x82795E10     LANDED
// 33  0x8279B88C  ProcessOutOfRangeVehicles(this, out)              @0x8276E910     LANDED
// 34  0x8279B894  ProcessInRangeVehicles(this, out)                 @0x8276EA18     LANDED
// 35  0x8279B89C  ExportCarData(this, out)                          @0x8276EB28     LANDED
// 36  0x8279B8A4  mBuzzBy.Update(lfDt, GetAICar(playerGlobal), mpClosestCar, &lbBuzz)
//                                                                   @0x8278B8C8     EXISTS (BrnAIBuzzBy.cpp:260;
//                                                                                  member mBuzzBy LANDED)
// 37  0x8279B8D8  if (lbBuzz) { GameEvent{+0x0 = 16?; see asm: v46 hi = 16}
//                   out->GetGameEventQueue()->AddEvent(&ev, 113, 4) }  (event 113 = the
//                 "buzz occurred" game event, payload 4 bytes)                       EXISTS (queue typed;
//                                                                                  expression to add)
// 38  0x8279B9C0  UnlockForRead / UnlockForWrite                                     EXISTS (ResetPump)
// 39  0x8279B9D0  StopMonitor(dword_82F30154)                                        ABSENT (#2)
// 40  0x8279B9DC  inStack->DestroyIOBuffer<RouteMapModuleIO::InputBuffer>(&lRouteIn)
//                 + assert "mpStack->DestroyIOBuffer( &mpBuffer )" (CgsModuleIOHelper.h:57)
//                                                                                  ABSENT (with #4)
//
// The same table lives in scratch/aiwave/A1_update_spine.md with the wiring notes.
//
// =================================================================================================
// THE OUTPUT RECORD PHYSICS CONSUMES (the field chain, end to end):
//   ProcessAIVehicleInputs  ->  lpOutputBuffer->GetVehicleDriverInterface()  (X360 this+0x15120,
//     a BrnPhysics::Vehicle::VehicleDriverInputInterface whose FIRST member is the
//     VariableEventQueue<5040,16> mDriverUpdateQueue)
//   ->  queue.AddEvent<BrnAIDriverControls>(&record, E_DRIVER_TYPE_AI == 1)
//   ->  WorldModule::BridgeAIModuleToPhysicsModule @0x827AAAA8 (the ONLY console caller of the
//       read twin OutputBuffer::GetVehicleInterface @0x8279CA00) appends it into
//       PhysicsModuleIO::InputBuffer::GetVehicleDriverInterface()   [HOST: a LinkStub at
//       WorldLinkStubs.cpp:2315 -- the conductor's bridge to un-gate]
//   ->  VehicleManager::UpdateDrivers (BrnVehicleManager_UpdateDrivers.cpp) walks
//       lpDriverInputInterface->GetUpdateDriverQueue(), case E_DRIVER_TYPE_AI ->
//   ->  VehicleManager::UpdateAIDriver @0x825C5110 (BrnVehicleManager_DriverArms.cpp:266):
//         maRaceCarDrivers[lpControls->miVehicleID].meDriverType = E_DRIVER_TYPE_AI;
//         maRaceCarDrivers[lpControls->miVehicleID].mControls    = *lpControls;   (memcpy 0x50)
//       so the record's miVehicleID IS the active-race-car slot, and mfGas / mfBrake /
//       mfHandBrake / mfSteering / mbBoost / mfSpeedMatchSpeed / mbDoSpeedMatch /
//       mbForceComeOutOfDrift / mbForceDrift are what RaceCarPhysics drives with next tick.
//
// =================================================================================================
// THE A3 SEAM. Three legs call AIDriver members that BrnAIDriver.h (lane A3's file) does not
// declare yet: DoRoundRobinWork(ERoundRobinType) (DWARF BrnAIDriver.h:327),
// ClearNearbyVehicles() (:275), AddNearbyTrafficToAvoidance(const TrafficAIEntity*) (:279),
// AddNearbyAIToAvoidance(const AICar*) (:283) and an accessor to the embedded AIAggression
// sub-machine (mAggression @+0x1C00, DWARF :547). Their bodies below are complete and compile
// once `BRNAI_AIWAVE_A3_LANDED` is 1; until then each leg is a NAMED park with a one-shot witness.
// [FLAG header_request] DELETE-WHEN lane A3's BrnAIDriver.h declares the five and the conductor
// flips the macro (or deletes it).
// =================================================================================================

// Lane A3 landed 2026-09-03: BrnAIDriver.h now declares DoRoundRobinWork / ClearNearbyVehicles /
// AddNearbyTrafficToAvoidance / AddNearbyAIToAvoidance / GetAggression, and BrnAIDriver.cpp +
// BrnAIDriver_Update.cpp body them. The three legs below are live.
#ifndef BRNAI_AIWAVE_A3_LANDED
#define BRNAI_AIWAVE_A3_LANDED 1
#endif

// [FLAG header_request] BrnTraffic::BrnTrafficIO::TrafficAIInterface::GetTrafficEntityCount and
// ::GetTrafficEntity are DECLARED in BrnTrafficAIInterfaces.h (:145 / :148) but have no bodies in
// this tree, and the console has no out-of-line symbol for either (names.tsv carries neither --
// the X360 reads mu16EntityCount @+0 and indexes maActiveEntityList @+16 inline). The traffic half
// of SortTrafficIntoAICars is therefore the ONLY leg of the A3 seam still gated: flip this to 1
// once that header restores the two accessors inline, e.g.
//   u16 GetTrafficEntityCount() const { return mu16EntityCount; }
//   const TrafficAIEntity* GetTrafficEntity(u16 lu16Index) const {
//       CGS_ASSERT(lu16Index < mu16EntityCount, "lu16Index < mu16EntityCount");   // :267
//       return &maActiveEntityList[lu16Index]; }
// The rival-vs-rival half of the avoidance feed below is LIVE either way.
#ifndef BRNAI_TRAFFICAI_ENTITY_ACCESSORS_PRESENT
#define BRNAI_TRAFFICAI_ENTITY_ACCESSORS_PRESENT 1   // flipped 2026-09-03: the accessors are inline in BrnTrafficAIInterfaces.h
#endif

#include "GameSource/World/AI/BrnAIModule.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                // AIModuleIO::InputBuffer
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"   // AIModuleIO::OutputBuffer
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"       // RaceCarAIInterface / AIRaceCarInterface
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"      // AICarOutputInterface
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"   // PlaceOnTrackRequest queue
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h" // BrnWorld::PlayerVehicleControls (mbReset)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"       // BrnPhysics::Vehicle::BrnAIDriverControls
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h" // BrnPhysics::Vehicle::VehicleDriverInputInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SharedClasses/AI/AISectionsResourceType.h"                    // BrnAI::AISectionsData

#if BRNAI_AIWAVE_A3_LANDED
#include "GameSource/World/AI/BrnAIAggression.h"                        // AIAggression::SetSuitabilityForAggression
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h" // TrafficAIInterface
#endif

#include <cmath>

namespace BrnAI
{

namespace
{
    // The baked float constants the drive legs load (image.bin, big-endian, VA - 0x82000000).
    const f32 KF_ONE                         = 1.0f;     // flt_82001C98
    const f32 KF_ZERO                        = 0.0f;     // flt_82001CC0
    const f32 KF_PASS_THROUGH_TRAFFIC_DISTANCE = 100.0f; // flt_820C3FAC  (ProcessInRangeVehicles)
    const f32 KF_ROUND_ROBIN_FAN_WORK_A      = 4.0f;     // flt_820C41C0  (DoRoundRobins, stack var_30)
    const f32 KF_ROUND_ROBIN_FAN_WORK_B      = 4.0f;     // flt_820C41C0  (DoRoundRobins, stack var_2C)
    const f32 KF_ROUND_ROBIN_WORK_SCALE      = 0.0625f;  // flt_82046E00  (DoRoundRobins)
    const f32 KF_FLT_MAX                     = 3.4028235e38f; // flt_8204F664 (UpdateOneProximityIndex)
    const s32 KI_ROUND_ROBIN_HNG_WORK        = 4;        // DoRoundRobins `li r4, 4`

    // [FLAG PC witness] one-shot / first-N witnesses for the legs that used to be absent. Named
    // per the standing rule; each prints at most the count it states. DELETE-WHEN the rivals are
    // seen driving in a Road Rage run.
    inline bool WitnessOnce(bool& lrbFlag)
    {
        if (lrbFlag || CgsDev::Log::gpDebugPrint == 0)
        {
            return false;
        }
        lrbFlag = true;
        return true;
    }

    inline bool IsFiniteF32(f32 lfValue) { return std::isfinite(lfValue); }

    // The AI record with its driver type stamped. BrnPlayerDriverControls keeps meDriverType
    // protected and BrnAIDriverControls (lane-foreign header) declares no constructor, so the
    // console's `stw 1, +0x44` (E_DRIVER_TYPE_AI) is done from a derived constructor exactly as
    // BrnTrafficDriverControls does for its own type. No members, no virtuals: same 80-byte image.
    // [FLAG header_request] DELETE-WHEN BrnVehicleDriverControls.h gives BrnAIDriverControls the
    // ctor `BrnAIDriverControls() { meDriverType = E_DRIVER_TYPE_AI; }`.
    struct AIDriverControlsRecord : public BrnPhysics::Vehicle::BrnAIDriverControls
    {
        AIDriverControlsRecord() { meDriverType = BrnPhysics::Vehicle::E_DRIVER_TYPE_AI; }
    };
    static_assert(sizeof(AIDriverControlsRecord) == 0x50,
                  "AIDriverControlsRecord must keep the console's 80-byte BrnAIDriverControls image");
}

// =================================================================================================
// GetAIDriver @0x82765B90 -- `if (index >= 8) assert("Invalid driver index: " << index)` (the
// streamed StrStream form, BrnAIModule.h:453), then `return 7536 * index + this + 192080`, i.e.
// &maAIDrivers[index]. The console assert is NON-GATING and the return walks off the array; the
// bail below is the PC deviation for the same reason GetAICar's is (an out-of-range index would
// land inside the RaceBalancingManager on this host instead of the console's tail).
// =================================================================================================
AIDriver* AIModule::GetAIDriver(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(static_cast<u32>(leActiveRaceCarIndex) < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS),
               "Invalid driver index: ");   // BrnAIModule.h:453
    if (static_cast<u32>(leActiveRaceCarIndex) >= static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS))
    {
        return 0;   // [GUARD] host-only; see the banner
    }
    return &maAIDrivers[static_cast<s32>(leActiveRaceCarIndex)];
}

// =================================================================================================
// GetAISectionsData @0x8277BC00 -- the console builds a temporary
// CgsResource::BaseResourcePtr::CreateFromHandle(this + 295880 == mMapDataHandle), asserts
// AISectionsData::GetMemoryResource() != NULL ("lpAISectionsData != NULL", BrnAIModule.cpp:2485)
// and unlinks the temporary. The host's GetLoadedAISectionsData() (BrnAIModule.cpp) is that
// read without the temporary's list churn -- RouteMapModule resolved the same handle at Prepare
// stage 3 -- so this body is the assert on top of it.
// =================================================================================================
AISectionsData* AIModule::GetAISectionsData() const
{
    const AISectionsData* lpAISectionsData = GetLoadedAISectionsData();
    CGS_ASSERT(lpAISectionsData != 0, "lpAISectionsData != NULL");   // BrnAIModule.cpp:2485
    return const_cast<AISectionsData*>(lpAISectionsData);
}

// The inlined-away DWARF accessor (RouteMapDebugComponent::RenderHUD reads this + 0x46B60).
const Route* AIModule::GetMasterRoute() const
{
    return &mMasterRoute;
}

// =================================================================================================
// UpdateCars @0x8279A518 (89 insns).
//
//   0x8279A538  StartMonitor(dword_82F30158)                                      [PARKED: #2]
//   0x8279A548  lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex (0x4E9FC))
//   0x8279A554  if (lpPlayerCar->meCarState (0x14C8) is IN_RANGE(0) or OUT_OF_RANGE(1))  == IsActive()
//   0x8279A580    liCount = mbIsInGameMode (lbzx 0x4EB7E) ? 8 : 35
//   0x8279A5CC    for (i = 0; i < liCount; i++)   (EGlobalRaceCarIndex++ asserts <= 35, :84)
//   0x8279A5DC      if (GetAICar(i)->IsActive())
//   0x8279A608        r19 = mbIsInOnlineGameMode (lbzx 0x4EB7D); r20 = this + 0x46B60 (mMasterRoute)
//   0x8279A610        r7  = GetAISectionsData()
//   0x8279A630        AICar::Update(car, r4 = this + 0x3D9D0 (mRaceBalancingManager), f1 = dt,
//                                   r6 = lpPlayerCar, r7 = sections, r8 = &mMasterRoute, r9 = online)
//   0x8279A664  UpdateOneProximityIndex()
//   0x8279A66C  StopMonitor(dword_82F30158)                                       [PARKED: #2]
//
// The two buffer arguments (r5 = the "Route" input buffer, r6 = the AI output buffer) are never
// read by the body -- confirmed by the register map: r4..r6 are overwritten before any use.
// =================================================================================================
void AIModule::UpdateCars(f32 lfTimeStep,
                          RouteMapModuleIO::InputBuffer* lpRouteInputBuffer,
                          AIModuleIO::OutputBuffer* lpOutputBuffer)
{
    (void)lpRouteInputBuffer;   // console r5 -- unused by the body
    (void)lpOutputBuffer;       // console r6 -- unused by the body

    AICar* lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));

    // [GUARD] the null test is host-only: the console's GetAICar cannot return null.
    if (lpPlayerCar != 0 && lpPlayerCar->IsActive())
    {
        const s32 liCarCount = mbIsInGameMode ? KI_MAX_ACTIVE_RACE_CARS : KI_MAX_OUT_OF_RANGE_RACE_CARS;

        static s32 siWitnessFrames = 0;
        s32 liUpdated = 0;

        for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0;
             static_cast<s32>(leCar) < liCarCount;
             leCar++)
        {
            AICar* lpAICar = GetAICar(static_cast<u32>(leCar));
            if (lpAICar->IsActive())
            {
                AISectionsData* lpAISectionsData = GetAISectionsData();
                lpAICar->Update(&mRaceBalancingManager,
                                lfTimeStep,
                                lpPlayerCar,
                                lpAISectionsData,
                                &mMasterRoute,
                                mbIsInOnlineGameMode);
                ++liUpdated;
            }
        }

        // [FLAG PC witness] first 3 frames with at least one active car. DELETE-WHEN rivals drive.
        if (liUpdated > 0 && siWitnessFrames < 3 && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siWitnessFrames;
            *CgsDev::Log::gpDebugPrint
                << "[aidrive] UpdateCars: " << liUpdated << " active AICar(s) of " << liCarCount
                << " updated (player global " << static_cast<s32>(mePlayerGlobalRaceCarIndex)
                << ", inGameMode " << static_cast<s32>(mbIsInGameMode) << ")\n";
        }
    }

    UpdateOneProximityIndex();
}

// =================================================================================================
// UpdateOneProximityIndex @0x8276E660 (88 insns). One car per frame gets its proximity rank.
//
//   0x8276E670  r31 = this + 0x50000 - 0x1428 == +322520 (meProximityGlobalRaceCarIndexRoundRobin)
//   0x8276E67C  lpCar = GetAICar(cursor)                    <- BEFORE the increment
//   0x8276E688  cursor++ (asserts <= 35, :84); if (cursor == 35) cursor = 0
//   0x8276E6EC  liPlayer = GetAICar(mePlayerGlobalRaceCarIndex)->miRaceCarIndex (0x14C4)
//   0x8276E6F8  r29 = +322828 (mfClosestDistance) = FLT_MAX;  r28 = +322832 (mpClosestCar) = 0
//   0x8276E714  for (i = 0; i < 35; i++)
//   0x8276E720    if (GetAICar(i)->IsActive())
//   0x8276E748      f0 = car[i]->+0x1508 (distance to player);  f13 = lpCar->+0x1508
//   0x8276E750      if (f13 > f0) ++liCloserCars
//   0x8276E75C      if (f0 < mfClosestDistance && liPlayer != i) { mfClosestDistance = f0; mpClosestCar = car[i]; }
//   0x8276E7AC  lpCar->miProximityIndex (0x152C) = muNumAggressiveCars (lbzx 0x4EB78) - liCloserCars
//
// +0x1508 is the host's mfBuzzDistanceToPlayer == DWARF mfDistanceToPlayer (:694).
// =================================================================================================
void AIModule::UpdateOneProximityIndex()
{
    AICar* lpCar = GetAICar(static_cast<u32>(meProximityGlobalRaceCarIndexRoundRobin));

    meProximityGlobalRaceCarIndexRoundRobin++;   // asserts <= E_GLOBAL_RACE_CAR_INDEX_COUNT
    if (meProximityGlobalRaceCarIndexRoundRobin == E_GLOBAL_RACE_CAR_INDEX_COUNT)
    {
        meProximityGlobalRaceCarIndexRoundRobin = E_GLOBAL_RACE_CAR_INDEX_0;
    }

    // [GUARD] host-only null tests (GetAICar's PC bail); the console dereferences unconditionally.
    const AICar* lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
    if (lpCar == 0 || lpPlayerCar == 0)
    {
        return;
    }
    const s32 liPlayerRaceCarIndex = lpPlayerCar->GetRaceCarIndex();

    mfClosestDistance = KF_FLT_MAX;
    mpClosestCar      = 0;

    s32 liCloserCars = 0;
    for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0;
         leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leCar++)
    {
        AICar* lpOther = GetAICar(static_cast<u32>(leCar));
        if (lpOther->IsActive())
        {
            const f32 lfOtherDistance = lpOther->mfBuzzDistanceToPlayer;
            if (lpCar->mfBuzzDistanceToPlayer > lfOtherDistance)
            {
                ++liCloserCars;
            }
            if (lfOtherDistance < mfClosestDistance && liPlayerRaceCarIndex != static_cast<s32>(leCar))
            {
                mfClosestDistance = lfOtherDistance;
                mpClosestCar      = lpOther;
            }
        }
    }

    lpCar->miProximityIndex = static_cast<s32>(muNumAggressiveCars) - liCloserCars;
}

// =================================================================================================
// DoRoundRobins @0x82798540 (55 insns).
//
//   0x82798550  liWork = RoundRobinDrivers(this, 4, E_ROUND_ROBIN_HNG (1))
//   0x82798560  an inlined Lerp over two stack-stored copies of flt_820C41C0 (4.0):
//                 var_30 = 4.0 ; var_2C = 4.0
//                 t = (f32)liWork * 0.0625 (flt_82046E00)
//                 fsel: t = (-t >= 0) ? 0 : t ; fsel: t = (1 - t >= 0) ? t : 1     == Clamp(t, 0, 1)
//                 vsubfp  v12 = splat(var_30) - splat(var_2C)              == (A - B)
//                 vmaddfp v0  = v12 * splat(t) + splat(var_2C)             == B + (A - B) * t
//                 fctiwz -> (s32)
//   0x82798604  RoundRobinDrivers(this, that count, E_ROUND_ROBIN_FAN (0))
// With A == B == 4.0 the fan pass always gets 4; the arithmetic is kept so the constants
// (which were clearly two tunables in the source) stay visible.
// =================================================================================================
void AIModule::DoRoundRobins()
{
    const s32 liHNGWork = RoundRobinDrivers(KI_ROUND_ROBIN_HNG_WORK, E_ROUND_ROBIN_HNG);

    f32 lfT = static_cast<f32>(liHNGWork) * KF_ROUND_ROBIN_WORK_SCALE;
    lfT = (-lfT >= 0.0f) ? KF_ZERO : lfT;            // fsel f0, f12(-t), f13(0), f0(t)
    lfT = ((KF_ONE - lfT) >= 0.0f) ? lfT : KF_ONE;    // fsel f0, f12(1-t), f0(t), f13(1)

    const f32 lfFanWork = KF_ROUND_ROBIN_FAN_WORK_B
                        + (KF_ROUND_ROBIN_FAN_WORK_A - KF_ROUND_ROBIN_FAN_WORK_B) * lfT;

    RoundRobinDrivers(static_cast<s32>(lfFanWork), E_ROUND_ROBIN_FAN);
}

// =================================================================================================
// RoundRobinDrivers @0x82798408 (78 insns).
//
//   0x82798424  StartMonitor(dword_82F30164)                                      [PARKED: #2]
//   0x82798434  if (mbIsInOnlineGameMode (lbzx 0x4EB7D))
//   0x8279844C    d = GetAIDriver(mePlayerActiveRaceCarIndex (0x4E9F8)); r = d->mbIsActive (0x1D69)
//                 ? d->DoRoundRobinWork(type) : 0;  StopMonitor; return r
//   0x82798484  cursor = &this[4 * (type + 80626)] == &meCurrentRoundRobin[type] (+322504)
//   0x8279849C  liStart = *cursor;  if (liMaxWork > 0) do {
//   0x827984B4    (*cursor)++  (asserts <= 8, :39);  if (*cursor == 8) *cursor = 0
//   0x827984F8    if (GetAIDriver(*cursor)->mbIsActive) { --liMaxWork; liDone += DoRoundRobinWork(type); }
//   0x82798518  } while (*cursor != liStart && liMaxWork > 0)
//   0x82798530  StopMonitor; return liDone
// =================================================================================================
s32 AIModule::RoundRobinDrivers(s32 liMaxWork, ERoundRobinType leType)
{
    if (mbIsInOnlineGameMode)
    {
        AIDriver* lpDriver = GetAIDriver(mePlayerActiveRaceCarIndex);
        s32 liWork = 0;
        if (lpDriver != 0 && lpDriver->mbIsActive)   // [GUARD] null is host-only
        {
#if BRNAI_AIWAVE_A3_LANDED
            liWork = lpDriver->DoRoundRobinWork(leType);
#else
            // [FLAG header_request] AIDriver::DoRoundRobinWork(ERoundRobinType) @0x82796340 --
            // DWARF BrnAIDriver.h:327; not declared in BrnAIDriver.h yet. DELETE-WHEN A3 lands.
            (void)leType;
#endif
        }
        return liWork;
    }

    s32 liWorkDone = 0;
    EActiveRaceCarIndex& lrCursor = meCurrentRoundRobin[static_cast<s32>(leType)];
    const EActiveRaceCarIndex leStart = lrCursor;

    if (liMaxWork > 0)
    {
        do
        {
            lrCursor++;   // asserts <= E_ACTIVE_RACE_CAR_INDEX_COUNT (BurnoutConstants.h:39)
            if (lrCursor == E_ACTIVE_RACE_CAR_INDEX_COUNT)
            {
                lrCursor = E_ACTIVE_RACE_CAR_INDEX_0;
            }

            AIDriver* lpDriver = GetAIDriver(lrCursor);
            if (lpDriver != 0 && lpDriver->mbIsActive)   // [GUARD] null is host-only
            {
                --liMaxWork;
#if BRNAI_AIWAVE_A3_LANDED
                liWorkDone += lpDriver->DoRoundRobinWork(leType);
#else
                // [FLAG header_request] AIDriver::DoRoundRobinWork -- see above. The work counter
                // still advances so the cursor walks exactly as the console's does.
                static bool sbReported = false;
                if (WitnessOnce(sbReported))
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[aidrive] RoundRobinDrivers: AIDriver::DoRoundRobinWork is PARKED "
                           "(BRNAI_AIWAVE_A3_LANDED == 0) -- racing-line / HNG round-robin work "
                           "is not being done\n";
                }
#endif
            }
        }
        while (lrCursor != leStart && liMaxWork > 0);
    }

    return liWorkDone;
}

// =================================================================================================
// SetSuitabilityForAggression @0x8276E7C0 (~70 insns).
//
//   0x8276E7D8  d = GetAIDriver(index); if (!d) assert("Missing Driver in aggression check\n" :1251)
//   0x8276E8xx  car = d->mpCar (0x1CE0); if (!car) return
//               if (car->meRouteFindingStyle (0x14C0) == 2)               -> suitable = 1
//               else { d->mAggression.mbIsSuitableForAggression (0x1C60) = 0;
//                      if (!mbIsInOnlineGameMode (0x4EB7D) && d->mbIsActive (0x1D69)
//                          && mbDoAggressiveDriving (0x4EB7B)
//                          && mePlayerActiveRaceCarIndex (0x4E9F8) != index
//                          && AICar::GetSpeed(car) >= flt_8300D704
//                          && (car->meRouteFindingStyle == 0 || car->mfRaceTimer (0x14FC) >= 10.0))
//                        suitable = 1 }
//   The lpCarInterface argument (r5) is never read by the body.
//
// [FLAG PC bring-up] flt_8300D704 is a .bss float (KF_MIN_SPEED_FOR_AGGRESSION or similar) whose
// dyn-init writer has no ARTIST export (the only exported reader is this function); the
// pre-init value 0.0 is what the image holds. Until the writer is found the threshold is 0.0,
// i.e. speed never blocks suitability. DELETE-WHEN the constant's writer is recovered.
// =================================================================================================
void AIModule::SetSuitabilityForAggression(EActiveRaceCarIndex leActiveRaceCarIndex,
                                           const AIModuleIO::RaceCarAIInterface* lpCarInterface)
{
    (void)lpCarInterface;   // console r5 -- unused by the body

    AIDriver* lpDriver = GetAIDriver(leActiveRaceCarIndex);
    CGS_ASSERT(lpDriver != 0, "Missing Driver in aggression check\n");   // BrnAIModule.cpp:1251
    if (lpDriver == 0)
    {
        return;   // [GUARD] host-only
    }

    AICar* lpCar = lpDriver->GetCar();
    if (lpCar == 0)
    {
        return;
    }

#if BRNAI_AIWAVE_A3_LANDED
    const f32 KF_MIN_SPEED_FOR_AGGRESSION = 0.0f;   // flt_8300D704 -- see the banner FLAG

    bool lbSuitable;
    if (lpCar->GetRouteFindingStyle() == static_cast<ERouteFindingStyle>(2))
    {
        lbSuitable = true;
    }
    else
    {
        lpDriver->GetAggression()->SetSuitabilityForAggression(false);
        lbSuitable = !mbIsInOnlineGameMode
                  && lpDriver->mbIsActive != 0
                  && mbDoAggressiveDriving
                  && mePlayerActiveRaceCarIndex != leActiveRaceCarIndex
                  && lpCar->GetSpeed() >= KF_MIN_SPEED_FOR_AGGRESSION
                  && (lpCar->GetRouteFindingStyle() == static_cast<ERouteFindingStyle>(0)
                      || lpCar->mfRaceTimer >= 10.0f);
    }
    if (lbSuitable)
    {
        lpDriver->GetAggression()->SetSuitabilityForAggression(true);
    }
#else
    // [FLAG header_request] AIDriver::GetAggression() (the embedded AIAggression @+0x1C00, DWARF
    // BrnAIDriver.h:547) is not declared in BrnAIDriver.h; BrnAIDriver.cpp reaches it through a
    // TU-local offset helper. The whole body above is the console's; nothing runs until A3 lands.
    static bool sbReported = false;
    if (WitnessOnce(sbReported))
    {
        *CgsDev::Log::gpDebugPrint
            << "[aidrive] SetSuitabilityForAggression is PARKED (BRNAI_AIWAVE_A3_LANDED == 0)\n";
    }
#endif
}

// =================================================================================================
// UpdateDrivers @0x8279B148 (204 insns). Register map: r31 = this, r16 = in, r18 = out,
// v125 = v1 (player car position), f29 = f1 (time step).
//
//   0x8279B194  StartMonitor(dword_82F3015C)                                      [PARKED: #2]
//   0x8279B1AC  assert(lpInputBuffer :1383); r15 = in->GetRaceCarAIInterface(); assert(r15 :1388)
//   0x8279B208  lpPlayerCar = GetAIDriver(mePlayerActiveRaceCarIndex)->mbIsActive
//                             ? GetAICar(mePlayerGlobalRaceCarIndex) : 0
//   0x8279B238  DoRoundRobins()
//   0x8279B244  [first, last) = mbIsInOnlineGameMode ? [playerActive, playerActive + 1) : [0, 8)
//                 (the +1 is the enum ++, asserting <= 8 :39)
//   0x8279B2C8  for each slot i in range:
//   0x8279B2D0    d = GetAIDriver(i); if (!d) continue         (never null on the console)
//   0x8279B2EC    SetSuitabilityForAggression(i, r15)
//   0x8279B2F0    if (lpPlayerCar) { d->mpAggressionVictim (0x1CE8) = lpPlayerCar;
//                                    d->meAggressionVictim (0x1CF0) = lpPlayerCar->miRaceCarIndex (0x14C4) }
//   0x8279B308    StartMonitor(dword_82F30160)                                    [PARKED: #2]
//   0x8279B334    AIDriver::Update(d, f1 = dt, r5 = (i == miLineUpdateTokenCounter (0x4EB60)),
//                                  v1 = player position, r6 = lpPlayerCar,
//                                  r7 = mbDoInRangeCatchup (lbzx 0x4EB79), r8 = &mRandom (0x47F80))
//   0x8279B33C    StopMonitor(dword_82F30160)                                     [PARKED: #2]
//   0x8279B344    car = d->mpCar (0x1CE0); if (car && car->mbPlaceOnTrackRequested (0x153C)) {
//                   speed = car->mfPlaceOnTrackSpeed (0x150C); pos = car+0x1490; dir = car+0x14A0;
//                   car->mbPlaceOnTrackRequested = 0;
//   0x8279B394      results = out->GetAIModuleResultInterface() (W twin @0x8276DD10)
//   0x8279B3D4      PlaceOnTrackRequest{pos, dir, GetAIDriver(i)->mpCar->miRaceCarIndex, speed}
//                   -> EventQueue<PlaceOnTrackRequest,128>::AddEvent(results + 0x1810) }
//   0x8279B3DC    if (in->GetPlayerVehicleControls()->mbReset (+0x37))  GetAIDriver(i)->ResetAttribSysValues()
//   0x8279B42C  miLineUpdateTokenCounter = (miLineUpdateTokenCounter + 1 < 8) ? +1 : 0
//   0x8279B448  StopMonitor(dword_82F3015C)                                       [PARKED: #2]
//
// AIDriver::Update's host parameter names (lbActive / lbValidPlayer) are the host header's
// guesses; the console passes (i == miLineUpdateTokenCounter) and mbDoInRangeCatchup in those
// slots. Recorded here rather than renamed (BrnAIDriver.h is lane A3's).
// =================================================================================================
void AIModule::UpdateDrivers(const AIModuleIO::InputBuffer* lpInputBuffer,
                             AIModuleIO::OutputBuffer* lpOutputBuffer,
                             Vector3 lPlayerCarPosition,
                             f32 lfTimeStep)
{
    CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer != NULL");   // X360 :1383
    if (lpInputBuffer == 0 || lpOutputBuffer == 0)
    {
        return;   // [GUARD] host-only
    }

    const AIModuleIO::RaceCarAIInterface* lpCarInterface = lpInputBuffer->GetRaceCarAIInterface();
    CGS_ASSERT(lpCarInterface != 0, "lpCarInterface != NULL");   // X360 :1388

    AICar* lpPlayerCar = 0;
    {
        const AIDriver* lpPlayerDriver = GetAIDriver(mePlayerActiveRaceCarIndex);
        if (lpPlayerDriver != 0 && lpPlayerDriver->mbIsActive)
        {
            lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
        }
    }

    DoRoundRobins();

    EActiveRaceCarIndex leFirst;
    EActiveRaceCarIndex leLast;
    if (mbIsInOnlineGameMode)
    {
        leFirst = mePlayerActiveRaceCarIndex;
        leLast  = mePlayerActiveRaceCarIndex;
        leLast++;   // asserts <= E_ACTIVE_RACE_CAR_INDEX_COUNT (:39)
    }
    else
    {
        leFirst = E_ACTIVE_RACE_CAR_INDEX_0;
        leLast  = E_ACTIVE_RACE_CAR_INDEX_COUNT;
    }

    // The reset flag lives at +0x37 of the 60-byte PlayerVehicleControls block
    // (BrnPlayerVehicleControls.h:54 mbReset; 13 floats then mbHorn/mbChangeView/mbStart/mbReset).
    // The host accessor is untyped (const void*); this is the console's typed read.
    const BrnWorld::PlayerVehicleControls* lpPlayerControls =
        static_cast<const BrnWorld::PlayerVehicleControls*>(lpInputBuffer->GetPlayerVehicleControls());

    static s32 siWitnessFrames = 0;
    s32 liActiveDrivers = 0;

    for (EActiveRaceCarIndex leSlot = leFirst; leSlot < leLast; leSlot++)
    {
        AIDriver* lpDriver = GetAIDriver(leSlot);
        if (lpDriver == 0)
        {
            continue;
        }

        SetSuitabilityForAggression(leSlot, lpCarInterface);

        if (lpPlayerCar != 0)
        {
            lpDriver->SetAggressionVictimCar(lpPlayerCar);                   // stw 0x1CE8
            lpDriver->SetAggressionVictim(lpPlayerCar->GetRaceCarIndex());  // stw 0x1CF0
        }

        const bool lbLineUpdateToken = (leSlot == static_cast<EActiveRaceCarIndex>(miLineUpdateTokenCounter));
        lpDriver->Update(lfTimeStep,
                         lbLineUpdateToken,
                         lPlayerCarPosition,
                         lpPlayerCar,
                         mbDoInRangeCatchup,
                         &mRandom);
        if (lpDriver->mbIsActive)
        {
            ++liActiveDrivers;
        }

        // ---- the place-on-track hand-off: the AI car asked to be re-seated (reset fan-out) ----
        AICar* lpCar = lpDriver->GetCar();
        if (lpCar != 0 && lpCar->mbPlaceOnTrackRequested)
        {
            const f32     lfSpeed     = lpCar->mfPlaceOnTrackSpeed;
            const Vector3 lPosition   = lpCar->mPlaceOnTrackPosition;
            const Vector3 lDirection  = lpCar->mPlaceOnTrackDirection;
            lpCar->mbPlaceOnTrackRequested = false;

            AIModuleIO::AIModuleResultInterface* lpResults =
                reinterpret_cast<AIModuleIO::AIModuleResultInterface*>(
                    lpOutputBuffer->GetAIModuleResultInterfaceForWrite());
            const AIDriver* lpDriverAgain = GetAIDriver(leSlot);
            AIModuleIO::PlaceOnTrackRequest lRequest;
            lRequest.Construct(static_cast<EGlobalRaceCarIndex>(lpDriverAgain->GetCar()->GetRaceCarIndex()),
                               lfSpeed, lPosition, lDirection);
            lpResults->GetPlaceOnTrackRequestQueue()->AddEvent(lRequest);
        }

        if (lpPlayerControls != 0 && lpPlayerControls->mbReset)
        {
            GetAIDriver(leSlot)->ResetAttribSysValues();
        }
    }

    ++miLineUpdateTokenCounter;
    if (miLineUpdateTokenCounter >= KI_MAX_ACTIVE_RACE_CARS)
    {
        miLineUpdateTokenCounter = 0;
    }

    // [FLAG PC witness] first 3 frames with an active driver. DELETE-WHEN rivals drive.
    if (liActiveDrivers > 0 && siWitnessFrames < 3 && CgsDev::Log::gpDebugPrint != 0)
    {
        ++siWitnessFrames;
        *CgsDev::Log::gpDebugPrint
            << "[aidrive] UpdateDrivers: " << liActiveDrivers << " active AIDriver(s) updated"
            << " (player active " << static_cast<s32>(mePlayerActiveRaceCarIndex)
            << ", player AICar " << (lpPlayerCar != 0 ? "bound" : "NULL")
            << ", online " << static_cast<s32>(mbIsInOnlineGameMode) << ")\n";
    }
}

// =================================================================================================
// StoreDrivenCarData @0x827957F0 (392 insns) -- THE INPUT SIDE. Per active-car slot whose
// AIDriver is active, copy the race-car module's snapshot into the AICar.
//
//   0x82795844  assert(lpInputBuffer :2003); r27 = in->GetRaceCarAIInterface(); assert(r27 :2010)
//   0x827958E4  for (i = 0; i < 8; i++)   (asserts <= 8 :39)
//   0x827958EC    d = GetAIDriver(i); if (!d->mbIsActive (0x1D69)) continue
//   0x82795908    if (!IsActiveRaceCarDataValid(r27, i))
//                   assert("Driver " << i << " does not have an active race car!\n" :2019)
//   0x827959DC    car = d->mpCar (0x1CE0)
//   0x827959E4    section = GetActiveRaceCarSection(i)
//   0x827959F4    m = GetActiveRaceCarMatrix(i): four rows, each x/y/z tested `vcmpeqfp v,v`
//                   (NaN test) -> assert("Invalid car matrix" :2024)
//   0x82795BB4    v = GetActiveRaceCarVelocity(i): x/y/z NaN test -> assert("Invalid car velocity" :2025)
//   0x82795C5C    s = GetActiveRaceCarSpeed(i): NaN test -> assert("Invalid car speed" :2026)
//   0x82795CA4    r28 = (mePlayerActiveRaceCarIndex (0x4E9F8) == i)
//   0x82795CC8    r29 = r28 && mbAIDrivesPlayer (lbzx 0x4EB82)
//   0x82795CE4    v = GetActiveRaceCarVelocity(i) (again, into v127)
//   0x82795CF4    r20 = IsActiveRaceCarTouchingPlayer(i); r19 = ...TouchingAnother(i)
//                 r18 = ...Drifting(i); r17 = ...OnStartLine(i); r16 = ...InShowtime(i)
//                 r15 = ...Crashing(i); r14 = ...InAir(i); f31 = GetActiveRaceCarSpeed(i)
//                 r31 = GetActiveRaceCarMatrix(i); r4 = GetAISectionsData()
//   0x82795DC4    AICar::UpdateInRangeData(car, r4 = sections, r5 = &matrix, v1 = velocity,
//                     f1 = speed, r7 = section, r8 = inAir, r9 = crashing, r10 = showtime,
//                     stack: onStartLine, isPlayer(r28), isPlayer && AIDrivesPlayer(r29),
//                            drifting, touchingAnother, touchingPlayer)
//
// PPC arg map: the Vector3 rides in v1 and takes NO GPR slot; f1 skips r6, hence section in r7
// and the last six bools in the 8-byte stack slots var_151.. (X360 save area = right-justified).
// IsActiveRaceCarDataValid is the inlined `mSetActiveRaceCars.IsBitSet(i)` every other accessor
// of the interface asserts on (BrnRaceCarAIInterfaces.h:729/:739); GetActiveRaceCarVelocity /
// GetActiveRaceCarSpeed are the same bit-checked reads of maVelocities[i] / mafSpeeds[i] (the
// host interface exposes those members, and lacks the two accessors by name).
// =================================================================================================
void AIModule::StoreDrivenCarData(const AIModuleIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer != NULL");   // X360 :2003
    if (lpInputBuffer == 0)
    {
        return;   // [GUARD] host-only
    }

    const AIModuleIO::RaceCarAIInterface* lpCarInterface = lpInputBuffer->GetRaceCarAIInterface();
    CGS_ASSERT(lpCarInterface != 0, "lpCarInterface != NULL");   // X360 :2010
    if (lpCarInterface == 0)
    {
        return;   // [GUARD] host-only
    }

    static s32 siWitnessFrames = 0;
    s32 liStored = 0;

    for (EActiveRaceCarIndex leSlot = E_ACTIVE_RACE_CAR_INDEX_0;
         leSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leSlot++)
    {
        AIDriver* lpDriver = GetAIDriver(leSlot);
        if (lpDriver == 0 || !lpDriver->mbIsActive)
        {
            continue;
        }

        const u32 luSlot = static_cast<u32>(leSlot);

        // IsActiveRaceCarDataValid(i) == mSetActiveRaceCars.IsBitSet(i). NON-GATING on the
        // console (it asserts and reads anyway); reproduced as such.
        CGS_ASSERT(lpCarInterface->mSetActiveRaceCars.IsBitSet(luSlot),
                   "Driver  does not have an active race car!\n");   // X360 :2019 (streamed index dropped)

        AICar* lpCar = lpDriver->GetCar();

        const u16             luSection  = lpCarInterface->GetActiveRaceCarSection(leSlot);
        const Matrix44Affine& lrMatrix   = lpCarInterface->GetActiveRaceCarMatrix(leSlot);

        // "Invalid car matrix" :2024 -- x/y/z of all four rows must be finite (vcmpeqfp v==v).
        const bool lbMatrixValid =
            IsFiniteF32(lrMatrix.xAxis.x) && IsFiniteF32(lrMatrix.xAxis.y) && IsFiniteF32(lrMatrix.xAxis.z) &&
            IsFiniteF32(lrMatrix.yAxis.x) && IsFiniteF32(lrMatrix.yAxis.y) && IsFiniteF32(lrMatrix.yAxis.z) &&
            IsFiniteF32(lrMatrix.zAxis.x) && IsFiniteF32(lrMatrix.zAxis.y) && IsFiniteF32(lrMatrix.zAxis.z) &&
            IsFiniteF32(lrMatrix.wAxis.x) && IsFiniteF32(lrMatrix.wAxis.y) && IsFiniteF32(lrMatrix.wAxis.z);
        CGS_ASSERT(lbMatrixValid, "Invalid car matrix");   // X360 :2024

        const Vector3 lVelocity = lpCarInterface->maVelocities[luSlot];   // GetActiveRaceCarVelocity(i)
        CGS_ASSERT(IsFiniteF32(lVelocity.x) && IsFiniteF32(lVelocity.y) && IsFiniteF32(lVelocity.z),
                   "Invalid car velocity");   // X360 :2025

        const f32 lfSpeed = lpCarInterface->mafSpeeds[luSlot];            // GetActiveRaceCarSpeed(i)
        CGS_ASSERT(IsFiniteF32(lfSpeed), "Invalid car speed");             // X360 :2026

        const bool lbIsPlayer         = (mePlayerActiveRaceCarIndex == leSlot);
        // CORRECTED 2026-09-05 (aiwave2 conductor, asm 0x82795CB4..0x82795CD4): r29 = 1 when the
        // slot is the player's; then `lbzx [0x4EB82] ; cmplwi ; beq CD8` SKIPS the `mr r29, r26`
        // (= 0) when mbAIDrivesPlayer is FALSE -- so the flag is isPlayer && !mbAIDrivesPlayer:
        // "this is the player's car and a human, not the AI, is driving it". The old `&&
        // mbAIDrivesPlayer` left the human player's AICar with mbIsDrivenByPlayer == 0, and
        // AIAggression::FindTarget @0x82793CA0 refuses a player car whose byte is 0 -- no rival
        // could ever target the player (run4: every rival PASSIVE / OUT_OF_RANGE beside the player).
        const bool lbIsDrivenByPlayer = lbIsPlayer && !mbAIDrivesPlayer;

        const bool lbTouchingPlayer  = lpCarInterface->IsActiveRaceCarTouchingPlayer(leSlot);
        const bool lbTouchingAnother = lpCarInterface->IsActiveRaceCarTouchingAnother(leSlot);
        const bool lbDrifting        = lpCarInterface->IsActiveRaceCarDrifting(leSlot);
        const bool lbOnStartLine     = lpCarInterface->IsActiveRaceCarOnStartLine(leSlot);
        const bool lbInShowtime      = lpCarInterface->IsActiveRaceCarInShowtime(leSlot);
        const bool lbCrashing        = lpCarInterface->IsActiveRaceCarCrashing(leSlot);
        const bool lbInAir           = lpCarInterface->IsActiveRaceCarInAir(leSlot);

        AISectionsData* lpAISectionsData = GetAISectionsData();

        if (lpCar == 0)
        {
            // [GUARD] host-only: a live driver with no car is an AV on the console too. Witness it
            // once -- it means HandleManagementEvents' attach ring raised mbIsActive without
            // AIDriver::SetAICar.
            static bool sbReported = false;
            if (WitnessOnce(sbReported))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[aidrive] StoreDrivenCarData: driver slot " << static_cast<s32>(leSlot)
                    << " is active but has NO AICar (mpCar NULL) -- skipping\n";
            }
            continue;
        }

        lpCar->UpdateInRangeData(lpAISectionsData,
                                 lrMatrix,
                                 lVelocity,
                                 lfSpeed,
                                 luSection,
                                 lbInAir,
                                 lbCrashing,
                                 lbInShowtime,
                                 lbOnStartLine,
                                 lbIsPlayer,
                                 lbIsDrivenByPlayer,
                                 lbDrifting,
                                 lbTouchingAnother,
                                 lbTouchingPlayer);
        ++liStored;
    }

    // [FLAG PC witness] first 3 frames that stored anything. DELETE-WHEN rivals drive.
    if (liStored > 0 && siWitnessFrames < 3 && CgsDev::Log::gpDebugPrint != 0)
    {
        ++siWitnessFrames;
        *CgsDev::Log::gpDebugPrint
            << "[aidrive] StoreDrivenCarData: " << liStored << " active driver snapshot(s) stored into AICars\n";
    }
}

// =================================================================================================
// SortTrafficIntoAICars @0x8278A970 (131 insns).
//
//   0x8278A98C  traffic = in->GetTrafficAI(); assert(traffic->mu16EntityCount <= 256
//                 "mu16EntityCount <= KI_MAX_TRAFFIC_NEAR_RACECARS", BrnTrafficAIInterfaces.h:282)
//   0x8278A9xx  for (i = 0; i < 8; i++) GetAIDriver(i)->mNearbyVehicles.miCount (+0x700) = 0
//                 == AIDriver::ClearNearbyVehicles()
//   0x8278AAxx  for (e = 0; e < count; e++) { entity = &maActiveEntityList[e] (176-byte stride,
//                 list @+16; assert "lu16Index < mu16EntityCount" :267; assert(lpTraffic :2337);
//                 d = GetAIDriver(entity->meNearbyRaceCarIndex (+0x20));
//                 if (d->mbIsActive) d->AddNearbyTrafficToAvoidance(entity) }
//   0x8278AAxx  for (i = 0; i < 8; i++) if (GetAIDriver(i)->mbIsActive)
//                 for (j = 0; j < 8; j++) if (j != i && GetAIDriver(j)->mbIsActive
//                                            && GetAIDriver(j)->mpCar)
//                   GetAIDriver(i)->AddNearbyAIToAvoidance(GetAIDriver(j)->mpCar)
//
// This is the AVOIDANCE feed (traffic + other rivals into each driver's NearbyVehicles), not the
// race-car -> AICar state path; it is a whole body behind the A3 seam.
// =================================================================================================
void AIModule::SortTrafficIntoAICars(const AIModuleIO::InputBuffer* lpInputBuffer)
{
#if BRNAI_AIWAVE_A3_LANDED
    for (EActiveRaceCarIndex leSlot = E_ACTIVE_RACE_CAR_INDEX_0; leSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; leSlot++)
    {
        GetAIDriver(leSlot)->ClearNearbyVehicles();
    }

#if BRNAI_TRAFFICAI_ENTITY_ACCESSORS_PRESENT
    // The AIModuleIO InputBuffer seat is already typed (BrnAIModuleIO.h:75 typedefs
    // BrnTraffic::BrnTrafficIO::TrafficAIInterface inside the class), so no cast is needed.
    const BrnTraffic::BrnTrafficIO::TrafficAIInterface* lpTrafficAI = lpInputBuffer->GetTrafficAI();
    const u16 luEntityCount = lpTrafficAI->GetTrafficEntityCount();
    CGS_ASSERT(luEntityCount <= 256u, "mu16EntityCount <= KI_MAX_TRAFFIC_NEAR_RACECARS");   // :282

    for (u16 luEntity = 0; luEntity < luEntityCount; ++luEntity)
    {
        const BrnTraffic::BrnTrafficIO::TrafficAIEntity* lpTraffic = lpTrafficAI->GetTrafficEntity(luEntity);
        CGS_ASSERT(lpTraffic != 0, "lpTraffic");   // BrnAIModule.cpp:2337
        AIDriver* lpDriver = GetAIDriver(lpTraffic->meNearbyRaceCarIndex);
        if (lpDriver != 0 && lpDriver->mbIsActive)
        {
            lpDriver->AddNearbyTrafficToAvoidance(lpTraffic);
        }
    }
#else
    // [FLAG header_request] see BRNAI_TRAFFICAI_ENTITY_ACCESSORS_PRESENT at the top of this file.
    // AIDriver::AddNearbyTrafficToAvoidance @0x8277D4F8 IS bodied (BrnAIDriver_Update.cpp); only
    // the two TrafficAIInterface accessors that reach the entity list are missing.
    (void)lpInputBuffer;
    {
        static bool sbReportedTraffic = false;
        if (WitnessOnce(sbReportedTraffic))
        {
            *CgsDev::Log::gpDebugPrint
                << "[aidrive] SortTrafficIntoAICars: the TRAFFIC half is gated "
                   "(TrafficAIInterface::GetTrafficEntity/Count have no bodies) -- rivals avoid "
                   "each other but not traffic\n";
        }
    }
#endif

    for (EActiveRaceCarIndex leSlot = E_ACTIVE_RACE_CAR_INDEX_0; leSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; leSlot++)
    {
        AIDriver* lpDriver = GetAIDriver(leSlot);
        if (!lpDriver->mbIsActive)
        {
            continue;
        }
        for (EActiveRaceCarIndex leOther = E_ACTIVE_RACE_CAR_INDEX_0; leOther < E_ACTIVE_RACE_CAR_INDEX_COUNT; leOther++)
        {
            if (leOther == leSlot)
            {
                continue;
            }
            AIDriver* lpOther = GetAIDriver(leOther);
            if (lpOther->mbIsActive && lpOther->GetCar() != 0)
            {
                lpDriver->AddNearbyAIToAvoidance(lpOther->GetCar());
            }
        }
    }
#else
    (void)lpInputBuffer;
    // [FLAG header_request] (dead arm) AIDriver::ClearNearbyVehicles / AddNearbyTrafficToAvoidance
    // @0x8277D4F8 / AddNearbyAIToAvoidance @0x8277D6E0 (DWARF BrnAIDriver.h:275/:279/:283) are
    // not declared in BrnAIDriver.h. Rivals drive WITHOUT traffic/rival avoidance until A3 lands.
    static bool sbReported = false;
    if (WitnessOnce(sbReported))
    {
        *CgsDev::Log::gpDebugPrint
            << "[aidrive] SortTrafficIntoAICars is PARKED (BRNAI_AIWAVE_A3_LANDED == 0) -- no "
               "traffic / rival avoidance feed\n";
    }
#endif
}

// =================================================================================================
// ProcessAIVehicleInputs @0x82795E10 (153 insns) -- THE OUTPUT SIDE.
//
//   0x82795E38  if (!mbEnableDrivingInput (lbzx 0x4EB7C)) return
//   0x82795E54  assert(lpOutputBuffer :2144); q = out->GetVehicleDriverInterface() (W @0x8276D9C8);
//               assert(q "lpDriverInputInterface != NULL" :2148)
//   0x82795ED8  for (i = 0; i < 8; i++)   (asserts <= 8 :39)
//   0x82795EE8    the 80-byte BrnAIDriverControls on the stack (var_C0 == +0x00):
//                   +0x00 miVehicleID = -1        +0x04..+0x30 the 12 floats = 0.0 (f31)
//                   +0x34 mfBoostMaxSpeedScale = 1.0 (f30)   +0x38 miVehicleIDToMerge = -1
//                   +0x39 mbReset = 0   +0x3B mbBoost = 0   +0x3C..+0x3F = 0   +0x40 = 0
//                   +0x41 mbIsSteeringWheel = 0  +0x42 mbHorn = 0   +0x44 meDriverType = 1 (AI)
//                   +0x48 mfSpeedMatchSpeed = 0.0  +0x4C mbDoSpeedMatch = 0
//                   +0x4D mbForceComeOutOfDrift = 0  +0x4E mbSlamPlayer = 0
//                   (+0x3A mbToggle and the two pad bytes are NOT written by the console)
//   0x82795F08    d = GetAIDriver(i); if (d->mbIsActive (0x1D69)) {
//                   +0x00 = i;  +0x04 mfGas = d->mfAccelerator (0x1D28);  +0x08 mfBrake = d->mfBrake (0x1D2C)
//                   +0x0C mfHandBrake = d->mfHandBrake (0x1D30);  +0x10 mfSteering = d->mfSteeringAngle (0x1D24)
//                   +0x3B mbBoost = d->mbBoosting (0x1D6B)
//                   isPlayer = d->mpCar->mbIsPlayer (0x1549)
//                   +0x3C mbIsInvulnerableToVehicles = d->IsInvulnerable() || (isPlayer && mbAIPlayerInvulnerable (0x4EB83))
//                   +0x3D mbIsInvulnerableToWorld    = isPlayer && mbAIPlayerInvulnerable
//                   +0x3E mbForceDrift = d->mbWantToEnterDrift (0x1D65)
//                   +0x40 mbIsOnStartLine = d->IsOnStartLine();  +0x41 mbIsSteeringWheel = 1;  +0x4E = 0
//                   +0x48 mfSpeedMatchSpeed = d->mfForcedSpeed (0x1D18);  +0x4C mbDoSpeedMatch = d->mbUseForcedSpeed (0x1D66)
//                   +0x4D mbForceComeOutOfDrift = d->mbWantToExitDrift (0x1D64) }
//   0x82796034    VariableEventQueue<5040,16>::AddEvent<BrnAIDriverControls>(q, &record, liType = 1)
//
// One record per slot EVERY frame, active or not -- the inactive record carries miVehicleID = -1,
// which VehicleManager::UpdateAIDriver rejects on its `< KI_MAX_ACTIVE_RACE_CARS` assert... no:
// it asserts `liVehicleID < 8` only, and -1 passes it, then indexes maRaceCarDrivers[-1]. That
// is the CONSOLE's own behaviour (the bridge and UpdateDrivers gate on the type, not the id);
// see the risks in the lane report -- the host VehicleManager::UpdateAIDriver may need the
// console's own `miVehicleID >= 0` handling checked against @0x825C5110.
// =================================================================================================
void AIModule::ProcessAIVehicleInputs(AIModuleIO::OutputBuffer* lpOutputBuffer)
{
    using BrnPhysics::Vehicle::BrnAIDriverControls;
    using BrnPhysics::Vehicle::VehicleDriverInputInterface;

    if (!mbEnableDrivingInput)
    {
        return;
    }

    CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer != NULL");   // X360 :2144
    if (lpOutputBuffer == 0)
    {
        return;   // [GUARD] host-only
    }

    // [FLAG header_request] OutputBuffer::GetVehicleDriverInterface() hands out an untyped u8*
    // seat (maVehicleDriverInterface[0x14B0], a NOMINAL blob). The console type at this+0x15120
    // is BrnPhysics::Vehicle::VehicleDriverInputInterface (its first member IS the
    // VariableEventQueue<5040,16> the AddEvent below targets); sizeof on this host is 5296 ==
    // 0x14B0 exactly, pinned here. DELETE-WHEN the OutputBuffer owner types + Constructs the member.
    static_assert(sizeof(VehicleDriverInputInterface) <= 0x14B0,
                  "VehicleDriverInputInterface must fit the OutputBuffer's 0x14B0 vehicle-driver seat");
    VehicleDriverInputInterface* lpDriverInputInterface =
        reinterpret_cast<VehicleDriverInputInterface*>(lpOutputBuffer->GetVehicleDriverInterface());
    CGS_ASSERT(lpDriverInputInterface != 0, "lpDriverInputInterface != NULL");   // X360 :2148
    if (lpDriverInputInterface == 0)
    {
        return;   // [GUARD] host-only
    }
    VehicleDriverInputInterface::UpdateDriverEventQueue* lpQueue = lpDriverInputInterface->GetUpdateDriverQueue();

    static s32 siWitnessFrames = 0;
    s32 liActiveRecords = 0;

    for (EActiveRaceCarIndex leSlot = E_ACTIVE_RACE_CAR_INDEX_0;
         leSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leSlot++)
    {
        AIDriver* lpDriver = GetAIDriver(leSlot);

        AIDriverControlsRecord lControls;   // meDriverType = E_DRIVER_TYPE_AI (+0x44) from the ctor
        lControls.miVehicleID               = -1;
        lControls.mfGas                     = KF_ZERO;
        lControls.mfBrake                   = KF_ZERO;
        lControls.mfHandBrake               = KF_ZERO;
        lControls.mfSteering                = KF_ZERO;
        lControls.mfForwardSteering         = KF_ZERO;
        lControls.mfSpin                    = KF_ZERO;
        lControls.mfRequestedGas            = KF_ZERO;
        lControls.mfAftertouchLevel         = KF_ZERO;
        lControls.mfXSensor                 = KF_ZERO;
        lControls.mfYSensor                 = KF_ZERO;
        lControls.mfZSensor                 = KF_ZERO;
        lControls.mfGSensor                 = KF_ZERO;
        lControls.mfBoostMaxSpeedScale      = KF_ONE;
        lControls.miVehicleIDToMerge        = -1;
        lControls.mbReset                   = false;
        // [FLAG PC bring-up] +0x3A mbToggle is left as stack garbage by the console; zeroed here
        // so the memcpy'd record is deterministic. DELETE-WHEN never (harmless).
        lControls.mbToggle                  = false;
        lControls.mbBoost                   = false;
        lControls.mbIsInvulnerableToVehicles = false;
        lControls.mbIsInvulnerableToWorld   = false;
        lControls.mbForceDrift              = false;
        lControls.mbBoostBounce             = false;
        lControls.mbIsOnStartLine           = false;
        lControls.mbIsSteeringWheel         = false;
        lControls.mbHorn                    = false;
        lControls.mfSpeedMatchSpeed         = KF_ZERO;
        lControls.mbDoSpeedMatch            = false;
        lControls.mbForceComeOutOfDrift     = false;
        lControls.mbSlamPlayer              = false;

        if (lpDriver != 0 && lpDriver->mbIsActive)
        {
            const AICar* lpCar      = lpDriver->GetCar();
            const bool   lbIsPlayer = (lpCar != 0) ? lpCar->IsPlayerCar() : false;   // [GUARD] null car is host-only

            lControls.miVehicleID   = static_cast<s32>(leSlot);
            lControls.mfGas         = lpDriver->mfAccelerator;
            lControls.mfBrake       = lpDriver->mfBrake;
            lControls.mfHandBrake   = lpDriver->mfHandBrake;
            lControls.mfSteering    = lpDriver->mfSteeringAngle;
            lControls.mbBoost       = lpDriver->mbBoosting != 0;

            const bool lbPlayerInvulnerable = lbIsPlayer && mbAIPlayerInvulnerable;
            lControls.mbIsInvulnerableToVehicles = lpDriver->IsInvulnerable() || lbPlayerInvulnerable;
            lControls.mbIsInvulnerableToWorld    = lbPlayerInvulnerable;

            lControls.mbForceDrift          = lpDriver->mbWantToEnterDrift != 0;
            lControls.mfSpeedMatchSpeed     = lpDriver->mfForcedSpeed;
            lControls.mbDoSpeedMatch        = lpDriver->mbUseForcedSpeed != 0;
            lControls.mbForceComeOutOfDrift = lpDriver->mbWantToExitDrift != 0;
            lControls.mbIsOnStartLine       = lpDriver->IsOnStartLine();
            lControls.mbIsSteeringWheel     = true;
            lControls.mbSlamPlayer          = false;

            // [rival] PC witness (NOT X360): one line per second per active NON-player driver, first 150
            // lines -- the car's pose/speed/state/behaviour and the controls this record carries. This is
            // the instrument that says whether a rival MOVES and, if not, which link is dead.
            {
                static s32 saiRivalWitnessTick[E_ACTIVE_RACE_CAR_INDEX_COUNT] = { 0 };
                static s32 siRivalWitnessLines = 0;
                if (!lbIsPlayer && lpCar != 0 && CgsDev::Log::gpDebugPrint != 0 && siRivalWitnessLines < 150
                    && (saiRivalWitnessTick[leSlot]++ % 60) == 0)
                {
                    ++siRivalWitnessLines;
                    const Vector3 lPos = lpCar->GetPosition();
                    // aiwave2 2026-09-05: + distance to the player, aggression state / victim, fan bias mode
                    // and whether a speed-match is active -- the contact chain's observables.
                    const AICar* lpWitnessPlayer = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
                    f32 lfDistToPlayer = -1.0f;
                    if (lpWitnessPlayer != 0)
                    {
                        const Vector3 lPlayerPos = lpWitnessPlayer->GetPosition();
                        const f32 lfDx = lPos.x - lPlayerPos.x;
                        const f32 lfDy = lPos.y - lPlayerPos.y;
                        const f32 lfDz = lPos.z - lPlayerPos.z;
                        lfDistToPlayer = std::sqrt(lfDx * lfDx + lfDy * lfDy + lfDz * lfDz);
                    }
                    *CgsDev::Log::gpDebugPrint
                        << "[rival] slot " << static_cast<s32>(leSlot) << " global " << lpCar->GetRaceCarIndex()
                        << " pos (" << lPos.x << ", " << lPos.y << ", " << lPos.z << ")"
                        << " speed " << lpCar->GetSpeed() << " state " << static_cast<s32>(lpCar->GetState())
                        << " behaviour " << static_cast<s32>(lpCar->meBehaviour) << " route " << (lpCar->HasValidRoute() ? 1 : 0)
                        << " desired " << lpDriver->GetDesiredSpeed() << " gas " << lControls.mfGas << " brake " << lControls.mfBrake
                        << " steer " << lControls.mfSteering << " boost " << (lControls.mbBoost ? 1 : 0)
                        << " startline " << (lControls.mbIsOnStartLine ? 1 : 0)
                        << " dist " << lfDistToPlayer
                        << " agg " << static_cast<s32>(lpDriver->GetAggression()->GetAggressionState())
                        << " victim " << lpDriver->GetAggressionVictim()
                        << " bias " << static_cast<s32>(lpDriver->mSteeringFan.meBiasMode)
                        << " match " << (lpDriver->GetAggression()->IsSpeedMatchingType() ? 1 : 0)
                        << " prox " << lpCar->miProximityIndex
                        << " buzz " << lpCar->mfBuzzDistanceToPlayer
                        << " pDrv " << ((lpWitnessPlayer != 0 && lpWitnessPlayer->mbIsDrivenByPlayer) ? 1 : 0)
                        << " near " << lpDriver->GetNearbyVehicles()->GetCount()
                        << " aggLvl " << lpCar->GetAggressiveness()->GetAggressionLevel()
                        << " [FLAG PC witness]\n";
                }
            }

            ++liActiveRecords;
        }

        lpQueue->AddEvent<BrnAIDriverControls>(&lControls, BrnPhysics::Vehicle::E_DRIVER_TYPE_AI);
    }

    // [FLAG PC witness] first 3 frames with an active record. DELETE-WHEN rivals drive.
    if (liActiveRecords > 0 && siWitnessFrames < 3 && CgsDev::Log::gpDebugPrint != 0)
    {
        ++siWitnessFrames;
        *CgsDev::Log::gpDebugPrint
            << "[aidrive] ProcessAIVehicleInputs: " << liActiveRecords
            << " active BrnAIDriverControls record(s) exported (type E_DRIVER_TYPE_AI)\n";
    }
}

// =================================================================================================
// ProcessOutOfRangeVehicles @0x8276E910 (66 insns).
//
//   0x8276E930  assert(lpOutputBuffer :2196); r28 = out->GetAIRaceCarInterface() (W @0x8276DBC0);
//               assert(r28 "lpAIRaceCarInterface != NULL" :2200)
//   0x8276E994  for (i = 0; i < 35; i++)   (asserts <= 35 :84)
//   0x8276E9A4    car = GetAICar(i); if (car->meCarState (0x14C8) == OUT_OF_RANGE (1))
//   0x8276E9B8      v2 = car->GetDirection(); v1 = car->GetPosition()
//   0x8276E9E0      r28->UpdateInactiveRaceCarData(i, v1 = position, v2 = direction)
// =================================================================================================
void AIModule::ProcessOutOfRangeVehicles(AIModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer != NULL");   // X360 :2196
    if (lpOutputBuffer == 0)
    {
        return;   // [GUARD] host-only
    }

    AIModuleIO::AIRaceCarInterface* lpAIRaceCarInterface =
        reinterpret_cast<AIModuleIO::AIRaceCarInterface*>(lpOutputBuffer->GetAIRaceCarInterface());
    CGS_ASSERT(lpAIRaceCarInterface != 0, "lpAIRaceCarInterface != NULL");   // X360 :2200
    if (lpAIRaceCarInterface == 0)
    {
        return;   // [GUARD] host-only
    }

    for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0;
         leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leCar++)
    {
        const AICar* lpCar = GetAICar(static_cast<u32>(leCar));
        if (lpCar->GetState() == E_AI_CAR_STATE_OUT_OF_RANGE)
        {
            const Vector3 lDirection = lpCar->GetDirection();
            const Vector3 lPosition  = lpCar->GetPosition();
            lpAIRaceCarInterface->UpdateInactiveRaceCarData(leCar, lPosition, lDirection);
        }
    }
}

// =================================================================================================
// ProcessInRangeVehicles @0x8276EA18 (67 insns).
//
//   0x8276EA38  assert(lpOutputBuffer :2231); r27 = out->GetAIRaceCarInterface(); assert(r27 :2235)
//   0x8276EAA0  for (i = 0; i < 35; i++)   (asserts <= 35 :84)
//   0x8276EAAC    car = GetAICar(i);
//                 if (!car->mbIsPlayer (0x1549) && car->meCarState (0x14C8) == IN_RANGE (0))
//   0x8276EAC4      r27->UpdateAllRaceCarData(i, car->mpDriver (0x14B0)->mfDistanceToPlayer (0x1D08) > 100.0)
//                 i.e. "may pass through traffic" == the driver is more than 100 m from the player.
// =================================================================================================
void AIModule::ProcessInRangeVehicles(AIModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer != NULL");   // X360 :2231
    if (lpOutputBuffer == 0)
    {
        return;   // [GUARD] host-only
    }

    AIModuleIO::AIRaceCarInterface* lpAIRaceCarInterface =
        reinterpret_cast<AIModuleIO::AIRaceCarInterface*>(lpOutputBuffer->GetAIRaceCarInterface());
    CGS_ASSERT(lpAIRaceCarInterface != 0, "lpAIRaceCarInterface != NULL");   // X360 :2235
    if (lpAIRaceCarInterface == 0)
    {
        return;   // [GUARD] host-only
    }

    for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0;
         leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leCar++)
    {
        const AICar* lpCar = GetAICar(static_cast<u32>(leCar));
        if (!lpCar->IsPlayerCar() && lpCar->GetState() == E_AI_CAR_STATE_IN_RANGE)
        {
            const AIDriver* lpDriver = lpCar->GetDriver();
            if (lpDriver == 0)
            {
                // [GUARD] host-only: an IN_RANGE car with no driver is a null deref on the
                // console; here it is the "attach without SetDriver" signature -- witness once.
                static bool sbReported = false;
                if (WitnessOnce(sbReported))
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[aidrive] ProcessInRangeVehicles: IN_RANGE car " << static_cast<s32>(leCar)
                        << " has no AIDriver (mpDriver NULL) -- skipping its pass-through flag\n";
                }
                continue;
            }
            const bool lbCanPassThroughTraffic = lpDriver->mfDistanceToPlayer > KF_PASS_THROUGH_TRAFFIC_DISTANCE;
            lpAIRaceCarInterface->UpdateAllRaceCarData(leCar, lbCanPassThroughTraffic);
        }
    }
}

// =================================================================================================
// ExportCarData @0x8276EB28 (98 insns).
//
//   0x8276EB48  assert(lpOutputBuffer :2270)
//   0x8276EB78  lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex (0x4E9FC)); if (!lpPlayerCar) return
//   0x8276EB8C  r28 = out->GetAICarOutputInterface() (W @0x8276DC68); assert(r28 :2279)
//   0x8276EBC8  r29 = r28 + 0x1498 == &mauAISections[0]
//   0x8276EBD0  for (i = 0; i < 35; i++) {
//   0x8276EBE0    car = GetAICar(i); if (car->meRouteFindingStyle (0x14C0, lwz) != 0)
//   0x8276EBF8      r28->SetAICarDistanceToCheckpoint(i, car->mfDistanceToCheckpoint (0x14F0))
//   0x8276EBFC    section = car->muBestSectionIndex (0x1534) != 0x7FFF ? that : car->muDefaultSectionIndex (0x1532)
//                   == AICar::GetBestSectionIndex()
//   0x8276EC14    assert(0 <= i < 35 "liAICarIndex >= 0 && ..." BrnAICarOutputInterface.h:245)
//   0x8276EC40    mauAISections[i] = section  (sth)      == SetAISectionIndex(i, section) }
//   0x8276EC5C  Route::Construct(&r28->mPlayerRoute, lpPlayerCar->mRoute (car + 0));
//               r28->miPlayerRouteNodeIndex (0x14E0) = lpPlayerCar->miNextRouteNodeIndex (0x1524)
//                 == SetPlayerRoute(lpPlayerCar->GetRoute(), lpPlayerCar->miNextRouteNodeIndex)
//   0x8276EC64  r28->mbPlayerIsInShortcut (0x14E4) = lpPlayerCar->mbIsInShortcut (0x154C)
//                 == SetPlayerInShortcut(...)
//   0x8276EC6C  if (lpPlayerCar->mRoute.meStatus (0x1408) != 0 && mRoute.miNodeCount (0x1400) > 0)
//                 == HasValidRoute()
//   0x8276EC9C    out->GetAIRaceCarInterface()->SetPlayerRouteNodePositions(lpPlayerCar)
// =================================================================================================
void AIModule::ExportCarData(AIModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer != NULL");   // X360 :2270
    if (lpOutputBuffer == 0)
    {
        return;   // [GUARD] host-only
    }

    const AICar* lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
    if (lpPlayerCar == 0)
    {
        return;
    }

    AIModuleIO::AICarOutputInterface* lpCarOutputInterface =
        reinterpret_cast<AIModuleIO::AICarOutputInterface*>(lpOutputBuffer->GetAICarOutputInterface());
    CGS_ASSERT(lpCarOutputInterface != 0, "lpCarOutputInterface != NULL");   // X360 :2279
    if (lpCarOutputInterface == 0)
    {
        return;   // [GUARD] host-only
    }

    for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0;
         leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leCar++)
    {
        const AICar* lpCar = GetAICar(static_cast<u32>(leCar));
        if (lpCar->GetRouteFindingStyle() != static_cast<ERouteFindingStyle>(0))
        {
            lpCarOutputInterface->SetAICarDistanceToCheckpoint(static_cast<s32>(leCar), lpCar->mfDistanceToCheckpoint);
        }
        // The bounds assert (BrnAICarOutputInterface.h:245) lives inside SetAISectionIndex.
        lpCarOutputInterface->SetAISectionIndex(static_cast<s32>(leCar), lpCar->GetBestSectionIndex());
    }

    // [FLAG cross-home cast] the AICarOutputInterface models its embedded Route as the opaque
    // 5132-byte AICarOutputInterfaceRouteSlice (its header's own minimal slice); the console
    // passes the AICar's Route (car + 0) straight into Route::Construct. Same bytes, two names.
    // DELETE-WHEN BrnAICarOutputInterface.h adopts BrnRoute.h's Route.
    lpCarOutputInterface->SetPlayerRoute(
        reinterpret_cast<const AIModuleIO::AICarOutputInterfaceRouteSlice*>(lpPlayerCar->GetRoute()),
        lpPlayerCar->miNextRouteNodeIndex);
    lpCarOutputInterface->SetPlayerInShortcut(lpPlayerCar->mbIsInShortcut);

    // HasValidRoute(): `lwz 0x1408 != 0 && lwz 0x1400 > 0` on the car's embedded route.
    const Route* lpPlayerRoute = lpPlayerCar->GetRoute();
    if (lpPlayerRoute->GetStatus() != Route::E_STATUS_UNINITIALISED && lpPlayerRoute->GetNodeCount() > 0)
    {
        AIModuleIO::AIRaceCarInterface* lpAIRaceCarInterface =
            reinterpret_cast<AIModuleIO::AIRaceCarInterface*>(lpOutputBuffer->GetAIRaceCarInterface());
        lpAIRaceCarInterface->SetPlayerRouteNodePositions(lpPlayerCar);
    }
}

}   // namespace BrnAI
