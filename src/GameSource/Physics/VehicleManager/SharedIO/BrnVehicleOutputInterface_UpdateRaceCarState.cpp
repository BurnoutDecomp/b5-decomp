// =================================================================================================
// GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface_UpdateRaceCarState.cpp
//
// THE PHYSICS -> OUTPUT PUBLISH. This file is the console's ONLY writer of
// BrnPhysics::Vehicle::RaceCarState -- i.e. the only thing that ever puts a simulated car's pose,
// velocity, wheels and drive state where the world side can read them.
//
//   VehicleOutputInterface::UpdateRaceCarState  @0x825EC808  (535 insns)   [was ABSENT from the tree]
//   VehicleOutputInterface::SetEntityID                      (inlined)     DWARF :328
//   VehicleOutputInterface::SetRaceCarHidden                 (inlined)     DWARF :333
//   VehicleOutputInterface::SetWheelTransform                (inlined)     DWARF :339
//
// IT WAS NOT ON ANY GATE LIST. The conductor's three named deferrals for this leg were
// GetUpdatedVehicleBodies @0x82619340, UpdateVehiclePhysicsPostSimulation @0x826426E0 and
// WriteOutVehicleStats @0x8263F460. The first two do NOT touch VehicleOutputInterface at all
// (they are the sim-queue push and the post-sim suspension step); the third is the caller. The
// actual writer is this function, and it was absent from the tree entirely -- not gated, not
// stubbed, not declared. That is why the readback had nothing to read.
//
// SIGNATURE from the asm prologue + the DecFIGS DWARF (BrnVehicleOutputInterface.h:323):
//   void UpdateRaceCarState(int32_t, const RaceCarPhysics*, const VehicleDriver*, bool)
//   r3 = this, r4 = index, r5 = the RaceCarPhysics, r6 = the VehicleDriver, r7 = the bool.
//   Hex-Rays renders the last as `char a5`; the asm stores it straight into a bool member
//   (`stb r16, 0x454(r31)`), so it is the DWARF's bool.
//
// THE INDEX IS THE ENTITY INDEX. `mulli r11, r4, 0x460 ; add r11,r11,r3 ; addi r31,r11,0x10`
// == &maRaceCarStates[a2] (stride 1120, member base +16), and the caller passes
// maRaceCarEntityIDs[liRaceCar].GetEntityIndex(), not liRaceCar. Reproduced verbatim; see the
// FLAG on the declaration in BrnVehicleOutputInterface.h.
//
// -------------------------------------------------------------------------------------------------
// THE FIELD MAP, read off the ARTIST asm instruction by instruction (r31 == &maRaceCarStates[i],
// r30 == the RaceCarPhysics, r28 == the VehicleDriver, `spec` == mpAttribs). Every source is
// reached BY NAME here; the console offsets are the evidence, not the access path.
//
//   state offset  member                          <- console source
//   ------------  ------------------------------  ------------------------------------------------
//   +0x440 1088   miRaceCarID                     <- spec+0x50   mBaseAttribs.miRaceCarID
//   +0x350  848   mHalfExtent                     <- physics+0x6A0  (GetHalfExtent)
//   +0x360  864   mComOffset                      <- spec+0x20   mBaseAttribs.mCOMOffset
//   +0x370  880   mSlamEffect      (48, 6 x ld/std)  <- physics+0x1100
//   +0x3A0  928   mShuntEffect     (32, 4 x ld/std)  <- physics+0x1130
//   +0x43C 1084   mi8LastAttackersRaceCarIndex    <- physics+0x13E0 (extsb)
//   +0x445 1093   mi8LastContactedRaceCar         <- physics+0x1150
//   +0x430 1072   mfTimeSinceLastRaceCarContact   <- physics+0x1050 lane .z
//   +0x3C0  960   mCarAssetAttribKey (u64, ld/std)<- spec+0x358  mAttribsKey
//   +0x1F0  496   mTransform       (64, 4 x lvx/stvx) <- GetGraphicsVehicleTransform()
//   +0x330  816   mLinearVelocity                 <- physics+0x50
//   +0x340  832   mAngularVelocity                <- physics+0x60
//   +0x3CC  972   mfSpeedMPH                      <- physics+0x6C0 lane .x
//   +0x3D0  976   mfMaxSpeedMPH                   <- spec+0x70 lane .z  (…_MaxSpeed_…)
//   +0x3D4  980   mfMaxBoostSpeedMPH              <- spec+0x290 lane .y (…_MaxBoostSpeed_…)
//   +0x3FC 1020   mfAbsDriftScale                 <- physics+0x1000 lane .w, `vandc` sign clear
//   +0x404 1028   mfTimeInAir                     <- physics+0x1060 lane .z
//   +0x451 1105   mbIsWedgedInWorld               <- physics+0x135F
//   +0x450 1104   mbIsFrontRayOccluded            <- physics+0x1360
//   +0x453 1107   mbContactingWall                <- physics+0x1362
//   +0x455 1109   mbDeformedThisFrame             <- physics+0x135A
//   +0x400 1024   mfTimeDrifting                  <- drifting ? max(physics+0x1010 .z, 0) : 0.0f
//   +0x408 1032   mfGas / +0x40C Brake / +0x410 HandBrake <- driver+4 / +8 / +0xC
//   +0x458 1112   meDriverType                    <- driver+0xD0
//   +0x414 1044   mfSteering                      <- the vtable slot-0 call == GetSteeringAngle()
//   +0x418 1048   mfTimeBoosting                  <- physics+0x1040 lane .y
//   +0x44A 1098   mbCrashing                      <- physics+0x710
//   +0x44D 1101   mbStartedDeforming              <- physics+0x712
//   +0x44B 1099   mbIsFatalyCrashing              <- physics+0x711
//   +0x434 1076   mfTimeCrashing                  <- crashing ? physics+0xEF0 lane .y : 0.0f
//   +0x44E 1102   mbResetCarTransform             <- physics+0x135C
//   +0x44F 1103   mbJustBeenSlammed               <- physics+0x135D
//   +0x3D8  984   mfRPM                           <- physics+0xFB0 lane .y == mEngine+0xB0 .y
//   +0x444 1092   mi8Gear                         <- physics+0xFC0 == mEngine.mu8CurrentGear
//   +0x3DC  988   mfUpShiftRPM                    <- spec[(gear+0x1D)*16] lane .z == GetGearUpRPM(gear)
//   +0x3E0  992   mfDownShiftRPM                  <- physics+0xF10 lane .w == mEngine attribs GearDownRPM
//   +0x3E4..+0x3F8 996..1016  mafGearRatios[0..5] <- spec+0x1D0/0x1E0/…/0x220 lane .x == GetGearRatio(g)
//   +0x44C 1100   mbIsDriveable                   <- (physics+0x1436 == 0), then AND-folded per wheel
//   +0x000..      maWheels[0..3]                  <- the per-wheel loop (below)
//   +0x1C0  448   mAboveGroundTestResult          <- the rebased copy (below)
//   +0x454 1108   mbForceReset                    <- the bool argument
//   +0x456 1110   mbFullyDrivableFromCrash        <- 0, then the crash-recovery test (below)
//
// THE WHEEL LOOP (4 iterations, source stride 224 == sizeof(Wheel), dest stride 112 ==
// sizeof(WheelLite)); source register r10 walks &maWheels[i]+0x30, dest r11 walks
// &maWheels[i]+0x44, so every offset below is re-expressed by name:
//   mRoadContact.mPosition          <- mTransform.TransformPoint(GetLocalTractionPoint(i))
//                                      (`vmaddfp` chain row0*p.x + row1*p.y + row2*p.z + pos)
//   mRoadContact.mNormal            <- wheel.mRoadContact.mNormal
//   mRoadContact.mfLineDistanceToRoad / mCollisionTag / mbIsOnGround /
//   mbWasOnGroundLastUpdate / mbLineTestIsValid  <- the same fields of wheel.mRoadContact
// mbIsCloseToGround (+42 / wheel +0x2A) is NOT copied by the console. Reproduced as-is.
//   mVelocity                       <- wheel.mBodyPointVelocity            (+0xA0)
//   mfRadiansPerSecond              <- wheel.mIntegrationVariables.x       (+0x30 .x)
//   mfRotation                      <- wheel.mIntegrationVariables.z       (+0x30 .z)
//   mfRadius                        <- wheel.mSlipVariables.w              (+0x40 .w)
//   mfSkidFactor                    <- wheel.mSlipVariables.z              (+0x40 .z)
//   mfWheelLongSpeed                <- wheel.mSpeedAndMassOnWheelVariables.w (+0x70 .w)
//   mfRoadLongSpeed                 <- wheel.mSpeedAndMassOnWheelVariables.x (+0x70 .x)
//   mfRoadLatSpeed                  <- wheel.mSpeedAndMassOnWheelVariables.y (+0x70 .y)
//   mbAttached                      <- (wheel.mu8State != 2)
//   mbHasTraction                   <- wheel.mbHasTraction
//   mfSuspensionHeight              <- |mPosition.y| / (mPosition.y >= 0 ? suspZ.y : suspZ.x)
//        The console emits a `vrefp` + two Newton-Raphson refinement steps (the standard VMX
//        reciprocal sequence: vnmsubfp/vmaddfp/vnmsubfp/vmaddfp against a 1.0f built by
//        `vspltisw v0,1 ; vcfsx v13,v0,0`) then one `vmulfp`. That IS a divide; it is written as
//        a divide here per the de-optimization rule. The branch also selects WHICH suspension
//        travel lane divides (mSuspensionAndInertiaVariables .y above the rest position, .x
//        below) and negates the height on the below branch (`vxor v12,v12,0x80000000`).
//   and mbIsDriveable &= (wheel.mu8State == 0), folded once per wheel.
//
// THE ABOVE-GROUND REBASE (0x825ECEC0..0x825ECF94). The above-ground test result was captured
// against an OLDER pose, so both of its position-like fields are corrected before publishing:
//   delta      = mTransform.wAxis - mPreviousTransform.wAxis          (== GetFrameDisplacement())
//   state.pos  = physics.mAboveGroundTestResult.mIntersectionPosition + delta
//   state.dist = physics.mAboveGroundTestResult.mfVerticalDistance    + delta.y
//   normal / mCollisionTag / mbValid: straight copies
// then a SECOND correction, against the GRAPHICS transform this function just published:
//   state.dist -= (physics.GetPosition().y - state.mTransform.wAxis.y)
// (`lvx r30+0x40` minus `lvx r31+0x220`, i.e. the physics pose's translation row minus the
// published transform's translation row, .y lane only).
//
// THE CRASH-RECOVERY TEST (0x825ECF98..0x825ED04C), all gates AND-ed, result -> mbFullyDrivableFromCrash:
//   mbCrashing && mfTimeCrashing > 0.5f && all four maWheels[].mbHasTraction && mbIsDriveable
//   && |dot3(mAngularVelocity, physicsTransform.xAxis)| < 0.5f
//   && |dot3(mAngularVelocity, physicsTransform.zAxis)| < 0.5f
// Both 0.5f literals are flt_82F2A24C / flt_82F2A250, read off the image with headless IDA 9.3
// (u32 0x3F000000 each). The two dot products are computed UNCONDITIONALLY on the console
// (`vmsum3fp128` before the branch); the short-circuit here is behaviour-identical because they
// are pure.
//
// ONE DIVERGENCE, NAMED. The console dispatches GetSteeringAngle through vtable slot 0 with a
// 16-byte sret (`lwz r11,0(r30) ; lwz r11,0(r11) ; bctrl` then `lfs f0,0(r3)`), and the DWARF
// declares it `virtual VecFloat GetSteeringAngle() const` on SimpleVehiclePhysics. This tree
// declares it non-virtual, returning f32, on VehiclePhysics. Calling it by name is semantically
// identical for every concrete type in the ledger (RaceCarPhysics does not override it), so the
// call site below is correct as written -- but the declaration itself is a real divergence and is
// left for the verifier rather than silently retyped here (retyping it would touch every caller
// of a virtual the vtable-closure work already pinned).
// =================================================================================================

#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Engine.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cmath>

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // The console builds the point transform with the standard `vmaddfp` chain
    //     row0 * p.x + row1 * p.y + row2 * p.z + translation
    // (0x825ECE8C..0x825ECEB0). Written out as the same affine point transform.
    inline Vector3 TransformPoint(const Matrix44Affine& lrTransform, const Vector3& lvPoint)
    {
        Vector3 lvResult;
        lvResult.x = lrTransform.xAxis.x * lvPoint.x + lrTransform.yAxis.x * lvPoint.y
                   + lrTransform.zAxis.x * lvPoint.z + lrTransform.wAxis.x;
        lvResult.y = lrTransform.xAxis.y * lvPoint.x + lrTransform.yAxis.y * lvPoint.y
                   + lrTransform.zAxis.y * lvPoint.z + lrTransform.wAxis.y;
        lvResult.z = lrTransform.xAxis.z * lvPoint.x + lrTransform.yAxis.z * lvPoint.y
                   + lrTransform.zAxis.z * lvPoint.z + lrTransform.wAxis.z;
        lvResult.w = 0.0f;
        return lvResult;
    }

    // `vmsum3fp128 vD, vA, vB` -- the 3-lane dot product the crash-recovery test uses twice.
    inline f32 Dot3(const Vector3& lvA, const Vector3& lvB)
    {
        return lvA.x * lvB.x + lvA.y * lvB.y + lvA.z * lvB.z;
    }

    // flt_82F2A24C / flt_82F2A250 -- both 0x3F000000 == 0.5f, read out of the ARTIST image.
    // They are the same value at adjacent words and the console loads them separately, so they
    // are kept as two named constants rather than folded into one.
    const f32 KF_FULLY_DRIVABLE_MIN_TIME_CRASHING = 0.5f;   // flt_82F2A24C
    const f32 KF_FULLY_DRIVABLE_MAX_SPIN_RATE     = 0.5f;   // flt_82F2A250

    // The wheel state byte the console compares against 2 and 0 (`lbz r7,0xA7(r10)` == Wheel+0xD7
    // == mu8State). 2 == detached (mbAttached is its negation); 0 == the only state that leaves the
    // car driveable.
    const u8 KU8_WHEEL_STATE_DETACHED  = 2;
    const u8 KU8_WHEEL_STATE_DRIVEABLE = 0;
}

// -------------------------------------------------------------------------------------------------
// The three inlined DWARF setters. Each one is exactly the store WriteOutVehicleStats emits.
// -------------------------------------------------------------------------------------------------

// DWARF :328. WriteOutVehicleStats' `stw r11, 0x3D8(r31)` where r31 == &maRaceCarStates[liRaceCar].
void VehicleOutputInterface::SetEntityID(s32 liRaceCarIndex, EntityId lEntityID)
{
    maRaceCarStates[liRaceCarIndex].mEntityId = lEntityID;
}

// DWARF :333. WriteOutVehicleStats' `stb r11, 0x462(r31)`.
void VehicleOutputInterface::SetRaceCarHidden(s32 liRaceCarIndex, bool lbHidden)
{
    maRaceCarStates[liRaceCarIndex].mbIsHidden = lbHidden;
}

// DWARF :339. WriteOutVehicleStats' four-row `lvx128/stvx128` copy at
// `((wheel + 9) << 6) + 0x460*liRaceCar + interface` -- (wheel+9)*64 == 560 + 64*wheel ==
// &maWheelTransforms[wheel], which is what taking the argument by value and assigning expresses.
void VehicleOutputInterface::SetWheelTransform(u8 lu8RaceCarIndex, u8 lu8Wheel, Matrix44Affine lTransform)
{
    maRaceCarStates[lu8RaceCarIndex].maWheelTransforms[lu8Wheel] = lTransform;
}

// -------------------------------------------------------------------------------------------------
// @0x825EC808  VehicleOutputInterface::UpdateRaceCarState   (DWARF :323)
// -------------------------------------------------------------------------------------------------
void VehicleOutputInterface::UpdateRaceCarState(s32 liRaceCarIndex,
                                                const RaceCarPhysics* lpRaceCarPhysics,
                                                const VehicleDriver*  lpDriver,
                                                bool                  lbForceReset)
{
    // DIVERGENCE (named): the console has no null guard here -- WriteOutVehicleStats is its only
    // caller and always passes live objects. On this build the create path is young enough that a
    // null would be an AV inside a per-frame publish, so the two pointers are checked. Nothing else
    // about the body is conditional.
    CGS_ASSERT(lpRaceCarPhysics != 0, "lpRaceCarPhysics != NULL");
    CGS_ASSERT(lpDriver != 0, "lpDriver != NULL");
    if (lpRaceCarPhysics == 0 || lpDriver == 0)
    {
        return;
    }

    const RaceCarPhysics&  lrPhysics = *lpRaceCarPhysics;
    const VehicleDriver&   lrDriver  = *lpDriver;
    const VehicleAttribs&  lrSpec    = *lrPhysics.mpAttribs;
    RaceCarState&          lrState   = maRaceCarStates[liRaceCarIndex];

    // ---- the attribute-sourced identity/extent block (0x825EC828..0x825EC8F4) ------------------
    lrState.miRaceCarID        = lrSpec.mBaseAttribs.miRaceCarID;
    lrState.mHalfExtent        = lrPhysics.GetHalfExtent();
    lrState.mComOffset         = lrSpec.mBaseAttribs.mCOMOffset;
    lrState.mSlamEffect        = lrPhysics.mSlamEffect;
    lrState.mShuntEffect       = lrPhysics.mShuntEffect;
    lrState.mi8LastAttackersRaceCarIndex = lrPhysics.mi8LastAttackersRaceCarIndex;
    lrState.mi8LastContactedRaceCar      = lrPhysics.mi8LastContactedRaceCar;
    lrState.mfTimeSinceLastRaceCarContact =
        lrPhysics.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z;
    lrState.mCarAssetAttribKey = lrSpec.mAttribsKey;

    // ---- THE POSE (0x825EC8F8..0x825EC944) ------------------------------------------------------
    // This single call is what the whole publish leg exists for: the graphics-frame transform the
    // world side turns into ActiveRaceCar::mRenderParams.mBodyTransform.
    lrState.mTransform       = lrPhysics.GetGraphicsVehicleTransform();
    lrState.mLinearVelocity  = lrPhysics.GetLinearVelocity();
    lrState.mAngularVelocity = lrPhysics.GetAngularVelocity();

    // ---- the speed / drive-envelope block (0x825EC958..0x825EC9E0) -------------------------------
    lrState.mfSpeedMPH         = lrPhysics.GetSpeedMPH().x;
    lrState.mfMaxSpeedMPH      = lrSpec.mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z;
    lrState.mfMaxBoostSpeedMPH =
        lrSpec.mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.y;
    lrState.mfAbsDriftScale    =
        std::fabs(lrPhysics.mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w);
    lrState.mfTimeInAir        =
        lrPhysics.mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z;

    // ---- the four contact/deformation latches (0x825EC9E4..0x825ECA00) ---------------------------
    lrState.mbIsWedgedInWorld    = lrPhysics.mbIsWedgedInWorld;
    lrState.mbIsFrontRayOccluded = lrPhysics.mbIsFrontRayOccluded;
    lrState.mbContactingWall     = lrPhysics.mbContactingWall;
    lrState.mbDeformedThisFrame  = lrPhysics.mbDeformedThisFrame;

    // ---- drift time, gated on the drift state (0x825ECA04..0x825ECA40) ---------------------------
    // `lbz r11,0x1352 ; beq -> stfs 0.0f` else `vspltw lane 2 ; vmaxfp v0,v0,0 ; stfs`.
    if (lrPhysics.mu8DriftState != VehiclePhysics::eDriftState_None)
    {
        const f32 lfTimeDrifting =
            lrPhysics.mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z;
        lrState.mfTimeDrifting = (lfTimeDrifting > 0.0f) ? lfTimeDrifting : 0.0f;   // vmaxfp against 0
    }
    else
    {
        lrState.mfTimeDrifting = 0.0f;
    }

    // ---- the driver channel (0x825ECA44..0x825ECA68) ---------------------------------------------
    lrState.mfGas       = lrDriver.mControls.mfGas;
    lrState.mfBrake     = lrDriver.mControls.mfBrake;
    lrState.mfHandBrake = lrDriver.mControls.mfHandBrake;
    lrState.meDriverType = lrDriver.meDriverType;

    // ---- steering: the vtable slot-0 dispatch (0x825ECA6C..0x825ECA84) ---------------------------
    lrState.mfSteering = lrPhysics.GetSteeringAngle().x;

    // ---- boost + crash latches (0x825ECA88..0x825ECAE8) -------------------------------------------
    lrState.mfTimeBoosting =
        lrPhysics.mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.y;
    lrState.mbCrashing          = lrPhysics.IsCrashing();
    lrState.mbStartedDeforming  = lrPhysics.HasStartedDeforming();
    lrState.mbIsFatalyCrashing  = lrPhysics.IsFatallyCrashing();

    if (lrPhysics.IsCrashing())
    {
        lrState.mfTimeCrashing =
            lrPhysics.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y;
    }
    else
    {
        lrState.mfTimeCrashing = 0.0f;
    }

    lrState.mbResetCarTransform = lrPhysics.mbResetCarTransform;
    lrState.mbJustBeenSlammed   = lrPhysics.mbJustBeenSlammed;

    // ---- the powertrain block (0x825ECB14..0x825ECC3C) --------------------------------------------
    lrState.mfRPM  = lrPhysics.mEngine.GetRPM();
    const s32 liGear = static_cast<s32>(lrPhysics.mEngine.GetCurrentGear());
    lrState.mi8Gear = static_cast<s8>(liGear);
    lrState.mfUpShiftRPM   = lrSpec.mEngineAttribs.GetGearUpRPM(liGear);
    lrState.mfDownShiftRPM =
        lrSpec.mEngineAttribs.mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.w;
    for (s32 liRatio = 0; liRatio < VehicleAttribs::EngineAttribs::KI_MAX_NUM_GEARS; ++liRatio)
    {
        lrState.mafGearRatios[liRatio] = lrSpec.mEngineAttribs.GetGearRatio(liRatio);
    }

    // ---- mbIsDriveable: the deformation gate, then AND-folded per wheel ---------------------------
    // `lbz r11,0x1436 ; cntlzw ; extrwi ,1,26` == (byte == 0).
    lrState.mbIsDriveable = !lrPhysics.GetDeformedBeyondDriveTimeLimitsInCrash();

    // ---- THE WHEEL LOOP (0x825ECC6C..0x825ECEBC) ---------------------------------------------------
    const Matrix44Affine lPhysicsTransform = lrPhysics.GetTransform();

    for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
    {
        const Wheel&     lrWheel     = lrPhysics.GetWheel(static_cast<EVehicleDrivenWheel>(liWheel));
        WheelLite&       lrWheelLite = lrState.maWheels[liWheel];

        lrWheelLite.mVelocity          = lrWheel.mBodyPointVelocity;
        lrWheelLite.mfRadiansPerSecond = lrWheel.mIntegrationVariables.x;
        lrWheelLite.mfRotation         = lrWheel.mIntegrationVariables.z;
        lrWheelLite.mfRadius           = lrWheel.mSlipVariables.w;
        lrWheelLite.mfSkidFactor       = lrWheel.mSlipVariables.z;

        // The suspension height, normalised by whichever travel limit the wheel is inside.
        const f32 lfWheelHeight = lrWheel.mPosition.y;
        const f32 lfTravel      = (lfWheelHeight >= 0.0f)
                                      ? lrWheel.mSuspensionAndInertiaVariables.y
                                      : lrWheel.mSuspensionAndInertiaVariables.x;
        lrWheelLite.mfSuspensionHeight = std::fabs(lfWheelHeight) / lfTravel;

        lrWheelLite.mbAttached        = (lrWheel.mu8State != KU8_WHEEL_STATE_DETACHED);
        lrWheelLite.mfWheelLongSpeed  = lrWheel.mSpeedAndMassOnWheelVariables.w;
        lrWheelLite.mfRoadLatSpeed    = lrWheel.mSpeedAndMassOnWheelVariables.y;
        lrWheelLite.mfRoadLongSpeed   = lrWheel.mSpeedAndMassOnWheelVariables.x;
        lrWheelLite.mbHasTraction     = lrWheel.mbHasTraction;

        // `and r9, r9, r14` -- the car stays driveable only while every wheel is in state 0.
        lrState.mbIsDriveable = lrState.mbIsDriveable
                                && (lrWheel.mu8State == KU8_WHEEL_STATE_DRIVEABLE);

        // The road contact, with the position replaced by the world-space traction point.
        lrWheelLite.mRoadContact.mNormal                 = lrWheel.mRoadContact.mNormal;
        lrWheelLite.mRoadContact.mfLineDistanceToRoad    = lrWheel.mRoadContact.mfLineDistanceToRoad;
        lrWheelLite.mRoadContact.mCollisionTag           = lrWheel.mRoadContact.mCollisionTag;
        lrWheelLite.mRoadContact.mbIsOnGround            = lrWheel.mRoadContact.mbIsOnGround;
        lrWheelLite.mRoadContact.mbWasOnGroundLastUpdate = lrWheel.mRoadContact.mbWasOnGroundLastUpdate;
        lrWheelLite.mRoadContact.mbLineTestIsValid       = lrWheel.mRoadContact.mbLineTestIsValid;
        // mbIsCloseToGround is deliberately NOT copied -- the console does not copy it (there is
        // no store to WheelLite+42 anywhere in this function). Reproduced as-is.
        lrWheelLite.mRoadContact.mPosition = TransformPoint(
            lPhysicsTransform,
            lrPhysics.GetLocalTractionPoint(static_cast<u8>(liWheel)));
    }

    // ---- THE ABOVE-GROUND REBASE (0x825ECEC0..0x825ECF48) ------------------------------------------
    {
        const AboveGroundTestResult& lrSource = *lrPhysics.GetAboveGroundTestResult();

        // delta == this frame's translation minus the previous frame's (the console differences
        // mTransform.wAxis against mPreviousTransform.wAxis directly).
        Vector3 lvDelta;
        lvDelta.x = lPhysicsTransform.wAxis.x - lrPhysics.mPreviousTransform.wAxis.x;
        lvDelta.y = lPhysicsTransform.wAxis.y - lrPhysics.mPreviousTransform.wAxis.y;
        lvDelta.z = lPhysicsTransform.wAxis.z - lrPhysics.mPreviousTransform.wAxis.z;
        lvDelta.w = 0.0f;

        AboveGroundTestResult& lrDest = lrState.mAboveGroundTestResult;
        lrDest.mIntersectionPosition.x = lrSource.mIntersectionPosition.x + lvDelta.x;
        lrDest.mIntersectionPosition.y = lrSource.mIntersectionPosition.y + lvDelta.y;
        lrDest.mIntersectionPosition.z = lrSource.mIntersectionPosition.z + lvDelta.z;
        lrDest.mIntersectionPosition.w = lrSource.mIntersectionPosition.w;
        lrDest.mIntersectionNormal     = lrSource.mIntersectionNormal;
        lrDest.mCollisionTag           = lrSource.mCollisionTag;
        lrDest.mfVerticalDistance      = lrSource.mfVerticalDistance + lvDelta.y;
        lrDest.mbValid                 = lrSource.mbValid;

        // The second correction: from the PHYSICS pose to the GRAPHICS pose just published.
        lrDest.mfVerticalDistance -= (lPhysicsTransform.wAxis.y - lrState.mTransform.wAxis.y);
    }

    // ---- the two flags the console writes unconditionally (0x825ECF64 / 0x825ECF6C) ---------------
    lrState.mbForceReset              = lbForceReset;
    lrState.mbFullyDrivableFromCrash  = false;

    // ---- THE CRASH-RECOVERY TEST (0x825ECF98..0x825ED04C) -------------------------------------------
    if (lrState.mbCrashing && lrState.mfTimeCrashing > KF_FULLY_DRIVABLE_MIN_TIME_CRASHING)
    {
        bool lbAllWheelsHaveTraction = false;
        if (lrState.maWheels[0].mbHasTraction && lrState.maWheels[1].mbHasTraction
            && lrState.maWheels[2].mbHasTraction && lrState.maWheels[3].mbHasTraction)
        {
            lbAllWheelsHaveTraction = true;
        }

        // Roll and yaw rates about the car's own axes (the console computes both before branching).
        const f32 lfRollRate = Dot3(lrState.mAngularVelocity, lPhysicsTransform.xAxis);
        const f32 lfYawRate  = Dot3(lrState.mAngularVelocity, lPhysicsTransform.zAxis);

        if (lbAllWheelsHaveTraction
            && lrState.mbIsDriveable
            && std::fabs(lfRollRate) < KF_FULLY_DRIVABLE_MAX_SPIN_RATE
            && std::fabs(lfYawRate)  < KF_FULLY_DRIVABLE_MAX_SPIN_RATE)
        {
            lrState.mbFullyDrivableFromCrash = true;
        }
    }
}

}
}
