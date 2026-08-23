// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_UpdateDrivers.cpp
//
// THE DRIVER-CONTROLS CONSUMER -- VehicleManager::UpdateDrivers @0x82642C68 (120 insns).
// This is the function that moves the frame's driver-control records out of the shared
// VehicleDriverInputInterface queue and into the per-car physics. It is called LIVE, every frame,
// from BrnPhysicsModuleUpdateFunctions.cpp (PhysicsModule::Update's driver stage) with the
// interface the world bridge filled -- so before this wave the controls the player pushed reached
// a logged, inert gate and stopped there.
//
// SHAPE, AND THE PPC FLOAT-ABI TRAP IT SITS ON. The DWARF spells six parameters
// (references/DecFIGS/.../BrnVehicleManager.h:872):
//     void UpdateDrivers(float32_t, const VehicleDriverInputInterface *,
//                        VehicleOutputRequestInterface *, VehicleManagerOutputInterface *,
//                        DeformationInputInterface *, VehicleOutputInterface *);
// and the X360 prologue confirms it EXACTLY -- including the fact that r4 is never read:
//     r3  = this            (r31)
// f1  = lfTimeStep      (f31)   the float SKIPS its GPR slot, so r4 is a hole
//     r5  = lpDriverInputInterface        (r29)
//     r6  = lpRequestOutputInterface      (r25)
//     r7  = lpManagerOutputInterface      (r24)
//     r8  = lpDeformationInterface        (r23)
//     r9  = lpVehicleOutputInterface      (r22)
// Hex-Rays renders this as `(int a1, double a2, int a3 .. int a8)` with a3 == the phantom r4; the
// declaration below is the DWARF's, and every argument this body forwards is checked against the
// REGISTER, not against the pseudocode's numbering.
//
// WHAT IT DOES, store for store:
//   1. clear mbUpdatedPlayerDriver (+172316) and mbPlayerAiDriverValid (+172192)
//   2. build a zeroed BitArray<8> on the stack -- the "which race cars did a driver record touch
//      this frame" set every arm receives by reference (DWARF types all four arms with it)
// 3. default the player's own car to world-invulnerable (see the below)
//   4. drain the interface's VariableEventQueue<5040,16> with GetFirstEvent/GetNextEvent and
//      switch on the event TYPE, which is E_DRIVER_TYPE:
//          0 PLAYER -> UpdatePlayerDriver    1 AI      -> UpdateAIDriver
//          2 NETWORK-> UpdateNetworkDriver   3 TRAFFIC -> mPhysicalTrafficManager.UpdateTrafficDriver
//      (case 3's `this` is `r31 + 0xAEE0` == +44768 == the embedded traffic manager, not the
//       vehicle manager -- reached BY NAME here), default -> "Unrecognised event in update queue"
//   5. advance the fixed-step steering decay: mfSteeringUpdateRemainder += dt, then while it is
//      >= 0.02 s halve mfPlayerRecentSteering and subtract 0.02
//   6. DoHornTakedowns(requests, vehicleOut, managerOut, deformationIn)
//   7. publish this frame's target-assist list into the showtime singleton
//
// THE ONE STORE A VERIFIER SHOULD RE-DERIVE. Step 3 is
//     `stb 1, 0x7D(this + 224 * mePlayerActiveRaceCarIndex)`   (0x82642CD8..0x82642CE4)
// and 0x7D == 125 == 64 + 61: maRaceCarDrivers is at +64 (independently pinned -- UpdatePlayerDriver
// @0x825EA1E8 computes a maRaceCarDrivers element with the SAME base and stride, `idx*0xE0 + this +
// 0x40`; note its `idx` is the record's miVehicleIDToMerge -- the OTHER driver on the same car, on
// the two-drivers merge path -- not the miVehicleID this store uses, so it corroborates the base and
// the stride, not the index), and in-record 61 == 0x3D is
// mControls.mbIsInvulnerableToWorld per this tree's committed BrnPlayerDriverControls map. So the
// console DEFAULTS the local player's car to "world collisions do not count" at the top of every
// frame, and any incoming player record overwrites the byte via UpdatePlayerDriver's 0x48-byte
// controls memcpy. The address is asm-literal; the ROLE rests on the committed +0x3D naming, which
// is exactly the "address right, meaning wrong" class this project keeps paying for -- flagged, and
// reproduced BY NAME rather than by offset so a correction there corrects this too.
//
// ALL FIVE OF ITS CALLEES ARE STILL BODYLESS -- PARKED, NOT FAKED. Costed by address first, as
// the standing rule demands (every one of them is a real export with full assembly; none is a hole):
//     VehicleManager::UpdatePlayerDriver              @0x825E9F38  401
//     VehicleManager::UpdateNetworkDriver             @0x825C4D08  258
//     VehicleManager::UpdateAIDriver                  @0x825C5110  185
//     PhysicalTrafficManager::UpdateTrafficDriver     @0x825CA8A0  169
//     VehicleManager::DoHornTakedowns                 @0x8263CC68  333
//                                                     ------------ 1,346 instructions
// Each is above the ~100-instruction bar this wave was scoped to, and each is a separate behaviour
// (control application / network catch-up interpolation / AI mixing / traffic species routing /
// takedown scoring), so they are five named BRN_CONDUCTOR_GATEs in BrnPhysicsConductorGates.cpp
// rather than five invented bodies. Their own closures are shallow -- the only non-assert callees
// are memcpy, BrnTraffic::GetVehicleSpecies, VolumeInstanceId::SetEntityIDEntityIndex and
// VehicleManager::InstantTakedown -- so they are a well-scoped follow-up wave, not a web.
//
// ⇒ HONEST STATE AFTER THIS WAVE: the queue is drained (records are consumed, not left to pile up),
// the steering decay and the target-assist publish are real, and the four dispatch arms log once and
// do nothing. A control still does not reach a wheel. That is a named seam at a console function
// boundary, not a partial body.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"                 // the traffic arm
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"   // the queue + GetTargetAssistParams
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"         // E_DRIVER_TYPE + the four payloads
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"             // msPlayerParams (the showtime singleton)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                               // CgsContainers::BitArray<8>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                         // GetFirstEvent / GetNextEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                                       // CGS_ASSERT

namespace BrnPhysics
{
namespace Vehicle
{
    // ---------------------------------------------------------------------------------------------
    // The fixed steering-decay step and its factor. Both are .rdata float-pool immediates the X360
    // decompiler resolved directly out of the image (flt_82005574 / flt_82001DA0); flt_82001DA0 ==
    // 0.5f is the same constant a dozen other bodies in this tree already home.
    // ---------------------------------------------------------------------------------------------
    static const f32 KF_STEERING_DECAY_STEP  = 0.02f;   // flt_82005574
    static const f32 KF_STEERING_DECAY_SCALE = 0.5f;    // flt_82001DA0

    // ==============================================================================================
    // @0x82642C68  BrnPhysics::Vehicle::VehicleManager::UpdateDrivers  (120 insns)
    // ==============================================================================================
    void VehicleManager::UpdateDrivers(
            f32 lfTimeStep,
            const VehicleDriverInputInterface* lpDriverInputInterface,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            VehicleOutputInterface* lpVehicleOutputInterface)
    {
        // 0x82642C8C..0x82642CA8: the stack bitset is zeroed BEFORE the queue is opened.
        CgsContainers::BitArray<8> lUpdatedCars;
        lUpdatedCars.UnSetAll();                                    // `li r11,0 ; std r11,0(r10)`

        mbUpdatedPlayerDriver = false;                              // stbx 0, this + 0x2A11C
        mbPlayerAiDriverValid = false;                              // stbx 0, this + 0x2A0A0

        const VehicleDriverInputInterface::UpdateDriverEventQueue* const lpQueue =
            lpDriverInputInterface->GetUpdateDriverQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpQueue->GetFirstEvent(&lpEvent, &liEventSize);   // bl @0x82642CC4

        // Issued AFTER GetFirstEvent and BEFORE the loop -- the console interleaves it into the
        // call's shadow (0x82642CC8..0x82642CE4), so the order is preserved rather than tidied.
        maRaceCarDrivers[mePlayerActiveRaceCarIndex].mControls.mbIsInvulnerableToWorld = true;

        while (liEventType >= 0)
        {
            const CgsModule::Event* const lpThisEvent = lpEvent;     // `lwz r30, var_70(r1)`

            switch (liEventType)
            {
                case E_DRIVER_TYPE_PLAYER:                                   // jumptable case 0
                    UpdatePlayerDriver(
                        static_cast<const BrnPlayerDriverControls*>(lpThisEvent), lUpdatedCars);
                    break;

                case E_DRIVER_TYPE_AI:                                       // jumptable case 1
                    UpdateAIDriver(
                        static_cast<const BrnAIDriverControls*>(lpThisEvent), lUpdatedCars);
                    break;

                case E_DRIVER_TYPE_NETWORK:                                  // jumptable case 2
                    UpdateNetworkDriver(
                        static_cast<const BrnNetworkDriverControls*>(lpThisEvent), lUpdatedCars);
                    break;

                case E_DRIVER_TYPE_TRAFFIC:                                  // jumptable case 3
                    // `add r3, r31, r28` with r28 == 0xAEE0 == 44768 == the embedded manager.
                    mPhysicalTrafficManager.UpdateTrafficDriver(
                        static_cast<const BrnTrafficDriverControls*>(lpThisEvent), lUpdatedCars);
                    break;

                default:
                    CGS_ASSERT(false, "Unrecognised event in update queue");   // :3644 (0xE3C)
                    break;
            }

            liEventType = lpQueue->GetNextEvent(lpThisEvent, &lpEvent, &liEventSize);
        }

        // ---- the fixed-step steering decay (0x82642DBC..0x82642E10) -------------------------------
        // mfSteeringUpdateRemainder accumulates real time; every whole 0.02 s of it halves the
        // player's recent-steering average. The console emits this as a do/while behind an entry
        // compare -- one loop here, same trip count.
        mfSteeringUpdateRemainder += lfTimeStep;
        while (mfSteeringUpdateRemainder >= KF_STEERING_DECAY_STEP)
        {
            mfPlayerRecentSteering    *= KF_STEERING_DECAY_SCALE;
            mfSteeringUpdateRemainder -= KF_STEERING_DECAY_STEP;
        }

        // ---- 0x82642E14..0x82642E28: r4..r7 == requests / vehicleOut / managerOut / deformationIn,
        //      which is the DWARF's own declaration order for this callee.
        DoHornTakedowns(lpRequestOutputInterface, lpVehicleOutputInterface,
                        lpManagerOutputInterface, lpDeformationInterface);

        // ---- 0x82642E2C..0x82642E44: publish this frame's target-assist candidates into the
        //      showtime singleton. The three arguments are `lbBounceBoosting + 0x50 / +0xD0 / +0xF0`
        //      -- msPlayerParams.maTargetPositions, .maTargetIds and &.miNumTargets, all three now
        //      named members (the position array was carved out of the reserved run this wave).
        lpDriverInputInterface->GetTargetAssistParams(msPlayerParams.maTargetPositions,
                                                      msPlayerParams.maTargetIds,
                                                      &msPlayerParams.miNumTargets);
    }
}
}
