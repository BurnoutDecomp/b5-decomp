// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ProcessCrashingNetworkCars.cpp
//
// The network crash-state stage of the physics conductor, plus the crash sink it calls.
//
//   VehicleManager::ProcessCrashingNetworkCars -- re-drains the driver-input queue at the end of
//   the step and looks ONLY at the network driver records. Each record carries the remote peer's
//   authoritative crash bit, and this stage reconciles it against the local car:
//       remote says "crashing", local car is not  -> commit the crash (SetNetworkCarCrashing)
//       remote says "not crashing", local car is  -> snap the car back onto the remote pose and
//                                                    un-crash it (deactivate the deformation
//                                                    model, reset the vehicle physics)
//   Nothing happens when the two agree. A record whose car is not a NETWORK car ends the drain.
//
//   VehicleManager::SetNetworkCarCrashing -- the remote-crash twin of SetRaceCarCrashing. It
//   latches the car's physics into the crash state, publishes the crash to the game side, and
//   allocates the RaceCarCrashData slot the scoring/UI layer reads. It is deliberately NOT the
//   local sink: there is no invulnerability/suppression gate, no takedown classification and no
//   contact geometry -- the remote peer already decided, so the crash normal it publishes is the
//   basis x-axis constant rather than a measured contact normal.
//
// DORMANT BY DATA ON A SOLO RUN. Both bodies key off E_DRIVER_TYPE_NETWORK records, and nothing
// produces those without a session, so a single-player case walks the queue and finds nothing.
// They are on the frame path regardless -- the stage runs every step.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"                 // maTrafficEntityIDs (the traffic-owned id remap)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"   // the driver queue + the base-deformation accessors
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"         // E_DRIVER_TYPE + BrnNetworkDriverControls
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"        // AddRaceCarCrashEvent / GetGameEventQueue
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"             // the per-car physics record
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h" // DeactivateDeformationModel
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                               // BitArray<8> / BitArray<32>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                         // GetFirstEvent / GetNextEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                                       // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                                               // GetVector3_XAxis

namespace BrnPhysics
{
namespace Vehicle
{
    // The crash record pushed onto the game-side event queue at the output interface's +0x65F0.
    // FLAG: only the byte SIZE (32) and the five written seats are attested; the gaps are not
    // modelled. The seats the commit writes are the remapped victim and crasher ids at +0x00/+0x04,
    // a zero flag byte at +0x10, a zero float at +0x14 and the victim's active-race-car index at
    // +0x18. Note that BrnVehicleManager.cpp's CrashIoEventRecord models the SAME record with the
    // three tail seats packed at +0x08/+0x0C/+0x10; this one follows the seats as written.
    struct NetworkCrashIoEventRecord : public CgsModule::Event
    {
        u32 mEntityIdValue;     // +0x00
        u32 mCrasherIdValue;    // +0x04
        u32 muPad08;            // +0x08 (not written)
        u32 muPad0C;            // +0x0C (not written)
        u32 mbFlag;             // +0x10 (byte seat; zero)
        f32 mfReserved;         // +0x14 (zero)
        u32 muVictimIndex;      // +0x18
        u32 muPad1C;            // +0x1C (not written)
    };

    // The crash-data pool is 32 slots wide and the free-list bitset is the same width.
    static const s32 KI_RACE_CAR_CRASH_DATA_COUNT = 32;

    // EntityId packing: (index << 10) | (E_ENTITYTYPE_RACECAR << 24). The index field is 14 bits.
    static inline EntityId PackRaceCarEntityId(u32 luEntityIndex)
    {
        return EntityId{ (luEntityIndex << 10) | 0x1000000u };
    }

    // =============================================================================================
    // ProcessCrashingNetworkCars
    // =============================================================================================
    void VehicleManager::ProcessCrashingNetworkCars(
            const VehicleDriverInputInterface* lpDriverInputInterface,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            VehicleOutputInterface* lpVehicleOutputInterface)
    {
        const VehicleDriverInputInterface::UpdateDriverEventQueue* const lpQueue =
            lpDriverInputInterface->GetUpdateDriverQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (liEventType >= 0)
        {
            const CgsModule::Event* const lpThisEvent = lpEvent;

            if (liEventType == E_DRIVER_TYPE_NETWORK)
            {
                const BrnNetworkDriverControls* const lpControls =
                    static_cast<const BrnNetworkDriverControls*>(lpThisEvent);
                const s32 liDriverActiveRaceCarIndex = lpControls->miVehicleID;

                CGS_ASSERT(maeRaceCarTypes[liDriverActiveRaceCarIndex]
                               != BrnWorld::E_RACE_CAR_TYPE_PLAYER,
                           "maeRaceCarTypes[leDriverActiveRaceCarIndex] != "
                           "BrnWorld::E_RACE_CAR_TYPE_PLAYER");

                // A record for a car that is no longer a network car ENDS the drain -- the console
                // leaves the function here rather than skipping the record.
                if (maeRaceCarTypes[liDriverActiveRaceCarIndex] != BrnWorld::E_RACE_CAR_TYPE_NETWORK)
                {
                    break;
                }

                CGS_ASSERT(liDriverActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leDriverActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
                CGS_ASSERT(mUsedRaceCars.IsBitSet(static_cast<u32>(liDriverActiveRaceCarIndex)),
                           "mUsedRaceCars.IsBitSet( leDriverActiveRaceCarIndex )");

                RaceCarPhysics& lrCar = maRaceCarVehicles[liDriverActiveRaceCarIndex];

                if (lpControls->mbCrash)
                {
                    // The remote peer says this car is wrecked. Commit it once.
                    if (!lrCar.mbCrashing)
                    {
                        EntityId lCrasherEntityId = EntityId{ 0u };
                        if (lpControls->meCrasherRaceCarIndex != -1)
                        {
                            CGS_ASSERT(static_cast<u32>(lpControls->meCrasherRaceCarIndex) < (1u << 14),
                                       "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
                            lCrasherEntityId = PackRaceCarEntityId(
                                static_cast<u32>(lpControls->meCrasherRaceCarIndex));
                        }

                        CGS_ASSERT(static_cast<u32>(liDriverActiveRaceCarIndex) < (1u << 14),
                                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
                        SetNetworkCarCrashing(
                            PackRaceCarEntityId(static_cast<u32>(liDriverActiveRaceCarIndex)),
                            lCrasherEntityId,
                            0.0f,
                            lpRequestOutputInterface,
                            lpManagerOutputInterface,
                            lpVehicleOutputInterface,
                            lpDeformationInterface);
                    }
                }
                else if (lrCar.mbCrashing)
                {
                    // The remote peer says this car is driving again: snap it onto the record's
                    // pose, drop the deformation model and reset the vehicle physics.
                    lrCar.SetTransform(lpControls->mTransform);
                    lrCar.SetLinearVelocity(lpControls->mLinearVelocity);
                    lrCar.SetAngularVelocity(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
                    lrCar.SetMass(lrCar.GetAttribs()
                                      ->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x);

                    const EActiveRaceCarIndex leRaceCarIndex =
                        static_cast<EActiveRaceCarIndex>(liDriverActiveRaceCarIndex);
                    lpDeformationInterface->DeactivateDeformationModel(
                        maRaceCarHandlingBodyIDs[liDriverActiveRaceCarIndex],
                        lpDriverInputInterface->GetBaseDeformationAmount(leRaceCarIndex),
                        static_cast<BrnPhysics::Deformation::DeformationResetType>(
                            lpDriverInputInterface->Ge(leRaceCarIndex)));

                    // Both resets run, base first, with the frozen flag cleared between them.
                    // Explicitly qualified: RaceCarPhysics declares its own no-argument Reset, which
                    // would otherwise hide both velocity overloads.
                    lrCar.SimpleVehiclePhysics::Reset(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
                    lrCar.SetFrozen(false);
                    lrCar.VehiclePhysics::Reset(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
                }
            }

            liEventType = lpQueue->GetNextEvent(lpThisEvent, &lpEvent, &liEventSize);
        }
    }

    // =============================================================================================
    // SetNetworkCarCrashing
    //
    // lfCrashTime, the request interface and the deformation interface are passed through by the
    // conductor but are not read on this path -- the remote crash carries no contact geometry and
    // queues no deformation work of its own.
    // =============================================================================================
    void VehicleManager::SetNetworkCarCrashing(
            EntityId lVictimEntityId,
            EntityId lCrasherEntityId,
            f32 lfCrashTime,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        (void)lfCrashTime;
        (void)lpRequestOutputInterface;
        (void)lpDeformationInterface;

        CGS_ASSERT(((lVictimEntityId.muValue >> 24) & 0xFF) == 1, "Bad Entity id");

        const s32 liVictimIndex = static_cast<s32>((lVictimEntityId.muValue >> 10) & 0x3FFF);
        CGS_ASSERT(liVictimIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "Bad Entity id");

        // A crash for a slot that is not live is dropped outright.
        if (!mUsedRaceCars.IsBitSet(static_cast<u32>(liVictimIndex)))
        {
            return;
        }

        CGS_ASSERT(maeRaceCarTypes[liVictimIndex] == BrnWorld::E_RACE_CAR_TYPE_NETWORK,
                   "maeRaceCarTypes[leActiveRaceCarIndex] == BrnWorld::E_RACE_CAR_TYPE_NETWORK");

        // A traffic-owned id is republished as the traffic slot's GLOBAL entity id; a race-car-owned
        // one passes through unchanged. BOTH ids are remapped, each by its OWN index.
        EntityId lRemappedVictimId = lVictimEntityId;
        if (((lVictimEntityId.muValue >> 24) & 0xFF) == 2)
        {
            lRemappedVictimId = mPhysicalTrafficManager.maTrafficEntityIDs[liVictimIndex];
        }

        EntityId lRemappedCrasherId = lCrasherEntityId;
        if (((lCrasherEntityId.muValue >> 24) & 0xFF) == 2)
        {
            const s32 liCrasherIndex = static_cast<s32>((lCrasherEntityId.muValue >> 10) & 0x3FFF);
            lRemappedCrasherId = mPhysicalTrafficManager.maTrafficEntityIDs[liCrasherIndex];
        }

        CGS_ASSERT(((lRemappedVictimId.muValue >> 24) & 0xFF) == 1, "Bad Remapped Entity id");
        CGS_ASSERT(static_cast<s32>((lRemappedVictimId.muValue >> 10) & 0x3FFF)
                       < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "Bad Remapped Entity id");

        // Already recorded? The same victim/crasher pair is committed once: a repeated network
        // record must not re-fire the event or burn a second crash-data slot.
        for (s32 liUsed = mUsedRaceCarCrashesList.GetFirstNonZeroBit();
             liUsed >= 0;
             liUsed = mUsedRaceCarCrashesList.GetNextNonZeroBit(liUsed))
        {
            if (maRaceCarCrashes[liUsed].mEntityId == lVictimEntityId.muValue
                && maRaceCarCrashes[liUsed].meType == lCrasherEntityId.muValue)
            {
                return;
            }
        }

        RaceCarPhysics& lrVictimRecord = maRaceCarVehicles[liVictimIndex];

        // The event + physics latch run only on the first commit; the slot allocation below is a
        // common tail that runs either way.
        if (!lrVictimRecord.mbCrashing)
        {
            lrVictimRecord.SetCrashing();

            // Re-read after the latch: the setter stores the flag unconditionally, so the taken arm
            // is the one that always runs. Kept as the console spells it, not constant-folded.
            const bool lbCrashingAfterLatch = lrVictimRecord.mbCrashing;
            if (lbCrashingAfterLatch)
            {
                lrVictimRecord.mbDeformationModelIsActive = 1;
                lrVictimRecord.ResetDeformableAABB();
            }

            // The crash normal is the basis x-axis constant -- a remote crash has no measured
            // contact normal to publish.
            // FLAG: the id seat of the interface's modelled argument list carries the remapped
            // CRASHER id at this site, not the victim -- the local sink puts its own remapped id
            // in the same seat, which is why the modelled parameter is named for the victim.
            lpManagerOutputInterface->AddRaceCarCrashEvent(
                lRemappedCrasherId,
                /*lbLocalPhysicalCrash=*/true,
                rw::math::vpu::GetVector3_XAxis(),
                lbCrashingAfterLatch,
                lrVictimRecord.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.x);

            NetworkCrashIoEventRecord lEventRecord;
            lEventRecord.mEntityIdValue   = lRemappedVictimId.muValue;
            lEventRecord.mCrasherIdValue  = lRemappedCrasherId.muValue;
            lEventRecord.mbFlag           = 0;
            lEventRecord.mfReserved       = 0.0f;
            lEventRecord.muVictimIndex    = static_cast<u32>(liVictimIndex);
            lpVehicleOutputInterface->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEventRecord), 63, 32);
        }

        // ---- allocate (or overwrite) a RaceCarCrashData slot ----------------------------------
        // The free slot is the first CLEAR bit of the allocation bitset.
        s32 liSlot = -1;
        for (s32 liScan = 0; liScan < KI_RACE_CAR_CRASH_DATA_COUNT; ++liScan)
        {
            if (!mUsedRaceCarCrashesList.IsBitSet(static_cast<u32>(liScan)))
            {
                liSlot = liScan;
                break;
            }
        }
        if (liSlot < 0)
        {
            // Pool full: reuse the OLDEST occupied slot -- the largest mfTimeSinceImpact, the timer
            // UpdateCrashes ages every frame. Only occupied slots are candidates.
            f32 lfOldestTime = 0.0f;
            for (s32 liUsed = mUsedRaceCarCrashesList.GetFirstNonZeroBit();
                 liUsed >= 0;
                 liUsed = mUsedRaceCarCrashesList.GetNextNonZeroBit(liUsed))
            {
                if (maRaceCarCrashes[liUsed].mfTimeSinceImpact > lfOldestTime)
                {
                    lfOldestTime = maRaceCarCrashes[liUsed].mfTimeSinceImpact;
                    liSlot = liUsed;
                }
            }
            // FLAG: the console carries the -1 sentinel straight into the stores below, so a full
            // pool whose every timer is still exactly zero indexes one slot short of the array.
            // Clamped here rather than reproduced -- the write is out of bounds, not merely odd.
            if (liSlot < 0)
            {
                liSlot = 0;
            }
        }

        maRaceCarCrashes[liSlot].mfTimeSinceImpact = 0.0f;
        maRaceCarCrashes[liSlot].meType            = lCrasherEntityId.muValue;
        maRaceCarCrashes[liSlot].mEntityId         = lVictimEntityId.muValue;
        mUsedRaceCarCrashesList.SetBit(static_cast<u32>(liSlot));
    }
}
}
