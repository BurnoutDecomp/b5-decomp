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
// takedown scoring), each landed as a named conductor gate here rather than as an invented body.
// All five were bodied in the follow-up wave -- four in BrnVehicleManager_DriverArms.cpp, the
// traffic one in BrnPhysicalTrafficManager_UpdateTrafficDriver.cpp -- and the gates are gone.
// Their closures are shallow: the only non-assert callees are memcpy, BrnTraffic::
// GetVehicleSpecies, VolumeInstanceId::SetEntityIDEntityIndex and VehicleManager::InstantTakedown.
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

// ---------------------------------------------------------------------------------------------
// The fixed steering-decay step and its factor. DWARF lists file-scope constants
// KF_PLAYER_RECENT_STEERING_DAMPING_FACTOR / KF_STEERING_UPDATE_STEP (BrnVehicleManager.cpp:137/138),
// and ARTIST emits their storage (0x8208FA18 = 0.5f, 0x8208FA1C = 0.02f, in the unit's constant
// block beside KF_MIN_GRIND_SPEED 0x8208FA0C) -- but no function reads either address: UpdateDrivers
// loads the shared float pool instead (0x82005574 = 0.02f, used by 29 functions in other units;
// 0x82001DA0 = 0.5f). The body below therefore uses the literal values; a named file-scope
// const here would be loaded from its own storage (our compiler addresses both through one anchor).
// ---------------------------------------------------------------------------------------------

// The unity path ARTIST's asserts name (string 0x82091150).
#define BRN_VEHICLEMANAGER_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Physics/VehicleManager/BrnVehicleManager.cpp"

namespace BrnPhysics
{
namespace Vehicle
{

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
        // DWARF locals (BrnVehicleManager.cpp:3579..3585): lpDriverQueue, liId, liSize, lpEvent,
        // lUpdatedVehicles; ARTIST stores nothing into liSize / lpEvent before GetFirstEvent.
        mbUpdatedPlayerDriver = false;                              // stbx 0, this + 0x2A11C
        mbPlayerAiDriverValid = false;                              // stbx 0, this + 0x2A0A0

        const VehicleDriverInputInterface::UpdateDriverEventQueue* lpDriverQueue =
            lpDriverInputInterface->GetUpdateDriverQueue();
        s32 liId;
        s32 liSize;
        const CgsModule::Event* lpEvent;

        CgsContainers::BitArray<8> lUpdatedVehicles;
        lUpdatedVehicles.UnSetAll();                                // `li r11,0 ; std r11,0(r10)`

        liId = lpDriverQueue->GetFirstEvent(&lpEvent, &liSize);    // bl @0x82642CC4

        maRaceCarDrivers[mePlayerActiveRaceCarIndex].SetInvulnerableToWorld(true);

        while (liId >= 0)
        {
            // Case blocks in the DWARF's line order (player :3608, network :3616, AI :3624,
            // traffic :3632) -- ARTIST's block order behind the jump table.
            switch (liId)
            {
                case E_DRIVER_TYPE_PLAYER:
                {
                    const BrnPlayerDriverControls* lpControls =
                        static_cast<const BrnPlayerDriverControls*>(lpEvent);
                    UpdatePlayerDriver(lpControls, lUpdatedVehicles);
                    break;
                }

                case E_DRIVER_TYPE_NETWORK:
                {
                    const BrnNetworkDriverControls* lpControls =
                        static_cast<const BrnNetworkDriverControls*>(lpEvent);
                    UpdateNetworkDriver(lpControls, lUpdatedVehicles);
                    break;
                }

                case E_DRIVER_TYPE_AI:
                {
                    const BrnAIDriverControls* lpControls =
                        static_cast<const BrnAIDriverControls*>(lpEvent);
                    UpdateAIDriver(lpControls, lUpdatedVehicles);
                    break;
                }

                case E_DRIVER_TYPE_TRAFFIC:
                {
                    // `add r3, r31, r28` with r28 == 0xAEE0 == 44768 == the embedded manager.
                    const BrnTrafficDriverControls* lpControls =
                        static_cast<const BrnTrafficDriverControls*>(lpEvent);
                    mPhysicalTrafficManager.UpdateTrafficDriver(lpControls, lUpdatedVehicles);
                    break;
                }

                default:
                    CGS_FAIL_AT("Unrecognised event in update queue",
                                  BRN_VEHICLEMANAGER_CPP_FILE, 3644);   // li r5,0xE3C
                    break;
            }

            liId = lpDriverQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }

        // ---- the fixed-step steering decay (0x82642DBC..0x82642E10) -------------------------------
        // mfSteeringUpdateRemainder accumulates real time; every whole 0.02 s of it halves the
        // player's recent-steering average.
        mfSteeringUpdateRemainder += lfTimeStep;
        while (mfSteeringUpdateRemainder >= 0.02f)                  // 0x82005574
        {
            mfPlayerRecentSteering    *= 0.5f;                      // 0x82001DA0
            mfSteeringUpdateRemainder -= 0.02f;
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
