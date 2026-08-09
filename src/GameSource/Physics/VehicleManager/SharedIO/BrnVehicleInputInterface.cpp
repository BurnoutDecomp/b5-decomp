#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
// ⭐ 2026-08-03 (task #113): this TU's own `static const u32 KU_ENTITYTYPE_TRAFFIC_VEHICLE = 2;`
// was the THIRD copy of that constant at BrnPhysics::Vehicle namespace scope (the others were
// BrnPhysicalTrafficManager.h:272 and BrnArticulatedJoint.h:42). It is owned by
// BrnVehicleConstants.h now; see the note there.
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"  // KU_ENTITYTYPE_TRAFFIC_VEHICLE

// BrnPhysics::Vehicle::VehicleInputInterface -- the bodied ledger functions homed by this group.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Each enqueue builds its event inline in declaration
// order (matching the DWARF field/param order) then AddEvent()s it into the owning queue.

namespace BrnPhysics
{
namespace Vehicle
{
    // ========================================================================
    // @0x822E66A0  VehicleInputInterface::Construct
    //   Point every embedded EventQueue's mpEvents at its own inline storage and clear the
    //   two non-queue members. The console's call order (asm 0x822E66A0..) is the member
    //   order with the four late-added queues appended; order is irrelevant (each Construct
    //   only touches its own sub-object) so the members are walked in DECLARATION order
    //   here, which is what the DWARF gives and what operator= above already assumes.
    //   The two scalars: the triangle-cache manager pointer (a1+128016) and the
    //   added-for-collision BitArray (a1+142160) are both zeroed.
    // ========================================================================
    void VehicleInputInterface::Construct()
    {
        mLineTestResultsQueue.Construct();
        mCreateRaceCarEventQueue.Construct();
        mRemoveRaceCarEventQueue.Construct();
        mResetRaceCarEventQueue.Construct();
        mValidateRaceCarEventQueue.Construct();
        mSetRaceCarCollisionEventQueue.Construct();
        mSetRaceCarCullingGroupEventQueue.Construct();
        mNetworkCarsAddedRemovedForCollisionQueue.Construct();
        mCreateTrafficEventQueue.Construct();
        mCreateArticulatedTrafficEventQueue.Construct();
        mSetTrafficCrashingEventQueue.Construct();
        mRemoveCrashedTrafficEventQueue.Construct();
        mUpdateNetworkTrafficEventQueue.Construct();
        mImpactEventQueue.Construct();

        // asm `*(a1 + 128016) = 0` -- mTriangleCacheInterface's manager pointer. [FLAG] the
        // interface's own type is committed but has no Construct; the console clears exactly
        // this one word.
        // asm `*(a1 + 142160) = 0` -- mRaceCarsAddedForCollision (BitArray<8>).
        mRaceCarsAddedForCollision.UnSetAll();
    }

    // @0x822CC1E8  VehicleInputInterface::CreateRaceCar
    //   Enqueues a spawn-race-car request and returns the index of the freshly-appended slot
    //   (queue.miLength - 1). The event's field order is identical to this method's parameter
    //   order (DWARF BrnVehicleInputInterface.h:73).
    s32 VehicleInputInterface::CreateRaceCar(
            VolumeInstanceId lVolumeInstanceId,
            Matrix44Affine   lInitialTransform,
            Vector3          lInitialVelocity,
            Vector3          lAngularVelocity,
            u64              lCarAssetAttribKey,
            ResourceHandle   lModelHandle,
            ResourceHandle   lGraphicsHandle,
            BrnWorld::ERaceCarType leRaceCarType,
            f32              lfDeformAmount,
            BrnPhysics::Deformation::DeformationResetType leBaseDeformationType,
            bool             lbDisablePhysicsStateReset,
            s32              liCarStrengthStat)
    {
        CreateRaceCarEvent lEvent;
        lEvent.mVolumeInstanceID          = lVolumeInstanceId;
        lEvent.mInitialTransform          = lInitialTransform;
        lEvent.mInitialVelocity           = lInitialVelocity;
        lEvent.mAngularVelocity           = lAngularVelocity;
        lEvent.mCarAssetAttribKey         = lCarAssetAttribKey;
        lEvent.mModelHandle               = lModelHandle;
        lEvent.mGraphicsHandle            = lGraphicsHandle;
        lEvent.meRaceCarType              = leRaceCarType;
        lEvent.mfDeformAmount             = lfDeformAmount;
        lEvent.meBaseDeformationType      = leBaseDeformationType;
        lEvent.mbDisablePhysicsStateReset = lbDisablePhysicsStateReset;
        lEvent.miCarStrengthStat          = liCarStrengthStat;

        mCreateRaceCarEventQueue.AddEvent(lEvent);
        return mCreateRaceCarEventQueue.GetLength() - 1;
    }

    // @0x822CC2A0  VehicleInputInterface::ResetRaceCar
    //   Enqueues a reset-vehicle request (transform/velocity/deformation reset after a wreck).
    //   NOTE: the DWARF signature (:161) carries a u8 + THREE bool params but ResetVehicleEvent has
    //   only three bool fields (and the asm stores exactly three bytes), so one boolean parameter is
    //   unused. The three struct bools are filled in declaration order from the leading boolean
    //   params; the trailing bool is accepted for signature fidelity but not stored.
    void VehicleInputInterface::ResetRaceCar(
            u32              luRaceCarIndex,
            Matrix44Affine   lInitialTransform,
            Vector3          lInitialVelocity,
            Vector3          lAngularVelocity,
            u8               lu8ResetTransform,
            bool             lbResetDeformation,
            bool             lbResettingAfterWreck,
            f32              lfRoadRageHowCloseToWrecked,
            bool             lbResetTransform,
            BrnPhysics::Deformation::DeformationResetType leDeformationResetType)
    {
        ResetVehicleEvent lEvent;
        lEvent.miRaceCarIndex              = luRaceCarIndex;
        lEvent.mInitialTransform           = lInitialTransform;
        lEvent.mInitialVelocity            = lInitialVelocity;
        lEvent.mAngularVelocity            = lAngularVelocity;
        lEvent.mbResetTransform            = (lu8ResetTransform != 0);
        lEvent.mbResetDeformation          = lbResetDeformation;
        lEvent.mbResettingAfterWreck       = lbResettingAfterWreck;
        lEvent.mfRoadRageHowCloseToWrecked = lfRoadRageHowCloseToWrecked;
        lEvent.meDeformationResetType      = leDeformationResetType;
        (void)lbResetTransform;

        mResetRaceCarEventQueue.AddEvent(lEvent);
    }

    // @0x822B4770  VehicleInputInterface::SetRaceCarAddedForCollision
    //   Marks the given active-race-car slot as added-for-collision by setting its bit in the
    //   mRaceCarsAddedForCollision BitArray<8>. The inlined bounds guard is reduced to the static
    //   expression per the committed convention. DWARF return type is void.
    void VehicleInputInterface::SetRaceCarAddedForCollision(EActiveRaceCarIndex leRaceCarIndex)
    {
        CGS_ASSERT(static_cast<u32>(leRaceCarIndex) < 8u, "Index < Number of bits");
        mRaceCarsAddedForCollision.SetBit(static_cast<u32>(leRaceCarIndex));
    }

    // @0x8271D138  VehicleInputInterface::SetTrafficCrashing
    //   Enqueues a 'traffic vehicle is now crashing' event. Asserts the entity is a traffic vehicle
    //   (owner byte == 2), then appends a SetTrafficCrashingEvent with mbCrashing = true.
    void VehicleInputInterface::SetTrafficCrashing(EntityId lEntityId)
    {
        CGS_ASSERT((lEntityId.muValue >> 24) == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
                   "lEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

        SetTrafficCrashingEvent lEvent;
        lEvent.mEntityId  = lEntityId;
        lEvent.mbCrashing = true;
        mSetTrafficCrashingEventQueue.AddEvent(lEvent);
    }

    // @0x8271D1B8  VehicleInputInterface::SetTrafficNotCrashing
    //   As SetTrafficCrashing but with mbCrashing = false, routed through the SAME queue.
    void VehicleInputInterface::SetTrafficNotCrashing(EntityId lEntityId)
    {
        CGS_ASSERT((lEntityId.muValue >> 24) == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
                   "lEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

        SetTrafficCrashingEvent lEvent;
        lEvent.mEntityId  = lEntityId;
        lEvent.mbCrashing = false;
        mSetTrafficCrashingEventQueue.AddEvent(lEvent);
    }

    // @0x823C87C0  VehicleInputInterface::Append
    //
    // ⭐ RECONSTRUCTED 2026-08-09 (feed wave). This was DECLARATION-ONLY -- no body existed
    // anywhere -- while two committed callers already named it
    // (UpdateInputBuffer::AppendVehicleInputInterface @0x823C8AD0 and
    // WorldModule::BridgeInputToPhysicsModule @0x827AB830).
    //
    // The 79-instruction X360 body is a flat fan-out: FOURTEEN per-queue Append merges and
    // nothing else. Unlike operator= it does NOT Clear first, and it does NOT touch the two
    // non-queue members (mTriangleCacheInterface, mRaceCarsAddedForCollision) -- the console
    // emits no store for either. Each `bl` names its element type, which pins the member
    // unambiguously; the member order below is the console's CALL order (which is not the
    // declaration order), with each console byte offset quoted for cross-reference.
    void VehicleInputInterface::Append(const VehicleInputInterface& lrOther)
    {
        mLineTestResultsQueue.Append(lrOther.mLineTestResultsQueue);                            // +0
        mCreateRaceCarEventQueue.Append(lrOther.mCreateRaceCarEventQueue);                      // +0x1F420
        mRemoveRaceCarEventQueue.Append(lrOther.mRemoveRaceCarEventQueue);                      // +0x1F930
        mResetRaceCarEventQueue.Append(lrOther.mResetRaceCarEventQueue);                        // +0x1F980
        mCreateTrafficEventQueue.Append(lrOther.mCreateTrafficEventQueue);                      // +0x20770
        mSetTrafficCrashingEventQueue.Append(lrOther.mSetTrafficCrashingEventQueue);            // +0x22040
        mRemoveCrashedTrafficEventQueue.Append(lrOther.mRemoveCrashedTrafficEventQueue);        // +0x22118
        mUpdateNetworkTrafficEventQueue.Append(lrOther.mUpdateNetworkTrafficEventQueue);        // +0x221F0
        mImpactEventQueue.Append(lrOther.mImpactEventQueue);                                    // +0x22840
        mValidateRaceCarEventQueue.Append(lrOther.mValidateRaceCarEventQueue);                  // +0x20190
        mSetRaceCarCollisionEventQueue.Append(lrOther.mSetRaceCarCollisionEventQueue);          // +0x202A0
        mSetRaceCarCullingGroupEventQueue.Append(lrOther.mSetRaceCarCullingGroupEventQueue);    // +0x202FC
        mCreateArticulatedTrafficEventQueue.Append(lrOther.mCreateArticulatedTrafficEventQueue);// +0x21590
        mNetworkCarsAddedRemovedForCollisionQueue.Append(
            lrOther.mNetworkCarsAddedRemovedForCollisionQueue);                                 // +0x20358
    }

    // @0x82592FD0  VehicleInputInterface::operator=
    //   Hand-written copy-assignment (called from BrnNetworkModule::ProcessBeforeSimulation). For
    //   each embedded EventQueue member it Clear()s this side then Append()s the source's live
    //   events ('replace with a copy of the source's queue'). The two non-queue members (the
    //   TriangleCacheInterface manager pointer and the race-cars-added-for-collision BitArray) are
    //   copied directly. The X360 walks the members in layout order (TriangleCache between the
    //   line-test and CreateRaceCar queues, the BitArray last). Returns *this.
    VehicleInputInterface& VehicleInputInterface::operator=(const VehicleInputInterface& lrOther)
    {
        mLineTestResultsQueue.Clear();
        mLineTestResultsQueue.Append(lrOther.mLineTestResultsQueue);

        mTriangleCacheInterface = lrOther.mTriangleCacheInterface;   // single-pointer copy

        mCreateRaceCarEventQueue.Clear();
        mCreateRaceCarEventQueue.Append(lrOther.mCreateRaceCarEventQueue);

        mRemoveRaceCarEventQueue.Clear();
        mRemoveRaceCarEventQueue.Append(lrOther.mRemoveRaceCarEventQueue);

        mResetRaceCarEventQueue.Clear();
        mResetRaceCarEventQueue.Append(lrOther.mResetRaceCarEventQueue);

        mValidateRaceCarEventQueue.Clear();
        mValidateRaceCarEventQueue.Append(lrOther.mValidateRaceCarEventQueue);

        mSetRaceCarCollisionEventQueue.Clear();
        mSetRaceCarCollisionEventQueue.Append(lrOther.mSetRaceCarCollisionEventQueue);

        mSetRaceCarCullingGroupEventQueue.Clear();
        mSetRaceCarCullingGroupEventQueue.Append(lrOther.mSetRaceCarCullingGroupEventQueue);

        mNetworkCarsAddedRemovedForCollisionQueue.Clear();
        mNetworkCarsAddedRemovedForCollisionQueue.Append(lrOther.mNetworkCarsAddedRemovedForCollisionQueue);

        mCreateTrafficEventQueue.Clear();
        mCreateTrafficEventQueue.Append(lrOther.mCreateTrafficEventQueue);

        mCreateArticulatedTrafficEventQueue.Clear();
        mCreateArticulatedTrafficEventQueue.Append(lrOther.mCreateArticulatedTrafficEventQueue);

        mSetTrafficCrashingEventQueue.Clear();
        mSetTrafficCrashingEventQueue.Append(lrOther.mSetTrafficCrashingEventQueue);

        mRemoveCrashedTrafficEventQueue.Clear();
        mRemoveCrashedTrafficEventQueue.Append(lrOther.mRemoveCrashedTrafficEventQueue);

        mUpdateNetworkTrafficEventQueue.Clear();
        mUpdateNetworkTrafficEventQueue.Append(lrOther.mUpdateNetworkTrafficEventQueue);

        mImpactEventQueue.Clear();
        mImpactEventQueue.Append(lrOther.mImpactEventQueue);

        mRaceCarsAddedForCollision = lrOther.mRaceCarsAddedForCollision;   // BitArray<8> copy

        return *this;
    }
}
}
