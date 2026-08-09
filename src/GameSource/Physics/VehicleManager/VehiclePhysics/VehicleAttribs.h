#pragma once

// BrnPhysics::Vehicle::VehicleAttribs -- THE per-car handling attribute block.
//
// This is the canonical home. Before it existed the type was forked THREE ways:
//   * VehicleAttribs.cpp     -- a full, offset-faithful definition, private to that TU, which
//                               also re-declared rw::math::vpu::VecFloat/Vector3/Vector4,
//                               BrnPhysics::InterpedParam3 and Wheel::TireAttribs (an ODR fork
//                               of five types the tree already owns elsewhere).
//   * VehiclePhysics.h       -- a "by-name, not offset-faithful" slice of ~14 registers.
//   * Engine.h               -- a NAMESPACE-SCOPE `BrnPhysics::Vehicle::EngineAttribs`, which is
//                               a different symbol from the console's nested
//                               VehicleAttribs::EngineAttribs and is why Engine.cpp could never
//                               link (its Engine::Construct emitted a call to a symbol the
//                               console never had).
// All three are retired in favour of this file.
//
// ---------------------------------------------------------------------------------------------
// LAYOUT PROVENANCE -- and a CORRECTION to the layout that was committed in VehicleAttribs.cpp.
//
// Member ORDER is the DWARF's (references/DecFIGS/dwarfdump/GameSource/Physics/VehicleManager/
// VehiclePhysics/VehicleAttribs.h, the flattened `struct BrnPhysics::Vehicle::VehicleAttribs`):
//     mBaseAttribs mSteeringAttribs mDriftAttribs mEngineAttribs mSuspensionAttribs
//     mBodyRollAttribs mCollisionAttribs mBoostAttribs mFrontTireAttribs mRearTireAttribs
//     miRaceCarID mAttribsKey mbIsValid
//
// ⚠️ The previously-committed VehicleAttribs.cpp had **mEngineAttribs and mDriftAttribs
// transposed** (engine @0x100, drift @0x1A0) and five static_asserts that baked the error in.
//
// The referee is BrnPhysics::Vehicle::VehicleAttribs::SetupAttribsForDonutAI @0x825F6298, which
// writes exactly four places (asm, ARTIST):
//     addi r11, r7, 0x1B0  ; [this+0x1B0].x = flt_8205820C
//     addi r10, r7, 0x110  ; [this+0x110].x = 0.0f
//     addi r3,  r7, 0x2D0  ; PrepareFrontTireForDonutAI
//     addi r3,  r7, 0x310  ; PrepareRearTireForDonutAI
//     addi r11, r7, 0x70   ; [this+0x70].x  = flt_8205820C   (the SAME float, loaded once)
// Under the transposed layout that reads as "MinSpeedForDrift = 700, Differential = 0" -- a car
// that must reach 700 mph before it may drift and whose differential is zero (no drive at all).
// Under the DWARF layout it reads as "MinSpeedForDrift = 0, MaxTorque = 700" -- a donut AI that
// can drift at any speed with the torque cranked, matching the mass it sets at +0x70.
//
// Independent confirmation: every `lwz rX,0x720(this)` + `addi rA,rX,off` (the mpAttribs idiom)
// across ALL BrnPhysics::Vehicle::VehiclePhysics::/RaceCarPhysics:: functions yields 32 distinct
// offsets, and under the layout below every one of them lands on a DWARF field whose name matches
// what the reading function does. A sample of the ones that would NOT match under the old layout:
//     +0x060 mBrakeScaleToFactorCurve                     <- UpdateBrakesAndGetBrakingFactor
//     +0x100 mDriftAttribs.mDriftScaleToYawTorque         <- ApplyDriftYaw
//     +0x110 mvMinSpeedForDrift_...                       <- UpdateDriftState, UpdateDriftScale
//     +0x150 mvSideForcePeakDriftAngle_SideForceMagnitude <- ApplyDriftLatForce, ApplyNaturalDriftForces
//     +0x230 mvRestDisplacement_Dampening_...             <- SetupSuspension, UpdateSuspensionSprings
//     +0x250 mvMaxYawDampingOnLanding_..._TimeToDampAfterLanding <- StabiliseAfterHardLanding
//     +0x260 mvWeightTransferDecayX_...                   <- CalculateWeightTransfer
//     +0x270 mvWheelLongForceHeightOffset_WheelLatForceHeightOffset <- HandleWheelPairFriction
//     +0x280 mvCrashSpeedMPS_CarAngularImpulseScale_...   <- ApplyCrashedContactImpulse (.y)
//
// ⭐ Consequence: VehiclePhysics.h's retired by-name slice had its OFFSETS right all along; it
// was VehicleAttribs.cpp that was wrong. The two sub-block sizes the old file did get right --
// `mPadAfterDrift[0xA0]` @0x230 and the 0x350/0x358/0x360 tail -- are exactly
// SuspensionAttribs+BodyRollAttribs+CollisionAttribs+BoostAttribs (0x30+0x20+0x10+0x40 == 0xA0)
// and miRaceCarID / Attribute::Key mAttribsKey / mbIsValid.
// ---------------------------------------------------------------------------------------------

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, Vector4, VecFloat
#include "GameSource/Physics/PhysicsUtilities/InterpedParam3.h"        // BrnPhysics::InterpedParam3 (canonical home)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"   // Wheel::TireAttribs (canonical home)

#include <cstddef>

namespace BrnPhysics
{
// InterpedParam3 used to be declared privately HERE. It is retired in favour of its DecFIGS home,
// GameSource/Physics/PhysicsUtilities/InterpedParam3.h, which is also where its two console leaves
// (Construct @0x8259CD30 / Prepare @0x8259CDC0) are now bodied. The private slice also had the
// member as a `Vector4`; the DWARF says `Vector3` -- the asm writes three lanes and never lane 3.

namespace Vehicle
{
    // The `physicsvehiclehandling` data wrapper VehicleAttribs::SetupAttribs streams from. Its
    // own type is un-homed; forward-declared so the SetupAttribs signature can name it.
    struct PhysicsVehicleHandling;

    struct VehicleAttribs
    {
        // ---- +0x000 (0xE0) -------------------------------------------------------------------
        struct VehicleBaseAttribs
        {
            Vector3        mFrontRightWheelPos;                                              // +0x00
            Vector3        mRearRightWheelPos;                                               // +0x10
            Vector3        mCOMOffset;                                                       // +0x20
            Vector3Plus    mCrashExtraVelocityFactors;                                       // +0x30
            Vector4        mDrivetimeDeformLimits;                                           // +0x40
            s32            miRaceCarID;                                                      // +0x50
            u8             mPad0054[12];                                                     // +0x54 (align the curve to 16)
            InterpedParam3 mBrakeScaleToFactorCurve;                                         // +0x60
            Vector4        mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce;                  // +0x70
            Vector4        mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels; // +0x80
            Vector4        mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl; // +0x90
            Vector4        mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass;   // +0xA0
            Vector4        mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo;          // +0xB0
            Vector4        mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff; // +0xC0
            Vector4        mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor; // +0xD0

            void SetMass(VecFloat lValue) { mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x = lValue.x; }
        };

        // ---- +0x0E0 (0x20) -------------------------------------------------------------------
        struct SteeringAttribs
        {
            Vector4 mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle;   // +0x00
            Vector4 mvMaxAngle_StraightReactionBias;                                    // +0x10
        };

        // ---- +0x100 (0x90) -------------------------------------------------------------------
        struct DriftAttribs
        {
            InterpedParam3 mDriftScaleToYawTorque;                                                                                  // +0x00
            Vector4 mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor; // +0x10
            Vector4 mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale;                                        // +0x20
            Vector4 mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift;                                         // +0x30
            Vector4 mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff;        // +0x40
            Vector4 mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower;                           // +0x50
            Vector4 mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping;                     // +0x60
            Vector4 mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime;                                 // +0x70
            Vector4 mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor;                                                   // +0x80

            void SetMinSpeedForDrift(VecFloat lValue)
            {
                mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x = lValue.x;
            }
        };

        // ---- +0x190 (0xA0) -------------------------------------------------------------------
        struct EngineAttribs
        {
            enum
            {
                KI_MAX_NUM_GEARS = 6   // DWARF: `extern const RwInt32 knMaxNumGears = 6`
            };

            // @0x825B7B90 (267 instrs) -- build the default engine attributes. Bodied in
            // VehicleAttribs.cpp. This is the symbol Engine::Construct @0x825F3EE8 calls; the
            // console spells it NESTED (VehicleAttribs::EngineAttribs::Construct), which is why
            // Engine.h's old namespace-scope declaration could never resolve.
            void Construct();

            // @0x825CF278 -- stream the engine tuning out of a loaded `physicsvehicleengineattribs`
            // data wrapper into the packed lanes. Bodied in VehicleAttribs.cpp.
            void InitializeFromAttribs(const void* lpSourceWrapper);

            void SetDifferential(VecFloat lValue)             { mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.x = lValue.x; }
            void SetTransmissionEfficiency(VecFloat lValue)   { mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.y = lValue.x; }
            void SetEngineResistance(VecFloat lValue)         { mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.z = lValue.x; }
            void SetGearDownRPM(VecFloat lValue)              { mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.w = lValue.x; }
            void SetMaxTorque(VecFloat lValue)                { mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.x = lValue.x; }
            void SetTorqueFallOffRPM(VecFloat lValue)         { mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.y = lValue.x; }
            void SetMaxRPM(VecFloat lValue)                   { mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.z = lValue.x; }
            void SetLSDMSpeedToAllowGearChanges(VecFloat lValue) { mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.w = lValue.x; }
            void SetFlyWheelInertia(VecFloat lValue)          { mvFlyWheelInertia_FlyWheelFriction_GearChangeTime.x = lValue.x; }
            void SetFlyWheelFriction(VecFloat lValue)         { mvFlyWheelInertia_FlyWheelFriction_GearChangeTime.y = lValue.x; }
            void SetGearChangeTime(VecFloat lValue)           { mvFlyWheelInertia_FlyWheelFriction_GearChangeTime.z = lValue.x; }
            void SetGearRatio(s32 liGear, VecFloat lValue)    { mavGearRatios_TorqueScales_GearUpRPMs[liGear].x = lValue.x; }
            void SetTorqueScale(s32 liGear, VecFloat lValue)  { mavGearRatios_TorqueScales_GearUpRPMs[liGear].y = lValue.x; }
            void SetGearUpRPM(s32 liGear, VecFloat lValue)    { mavGearRatios_TorqueScales_GearUpRPMs[liGear].z = lValue.x; }

            // The gearing lanes Engine::ComputeGear / Engine::GetMaxWheelAngularVelocity read.
            f32 GetDifferential() const        { return mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.x; }
            f32 GetMaxRPM() const              { return mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.z; }
            f32 GetGearRatio(s32 liGear) const { return mavGearRatios_TorqueScales_GearUpRPMs[liGear].x; }
            f32 GetGearUpRPM(s32 liGear) const { return mavGearRatios_TorqueScales_GearUpRPMs[liGear].z; }

            InterpedParam3 mTorqueCurve;                                                        // +0x00
            Vector4 mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM;          // +0x10
            Vector4 mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges;             // +0x20
            Vector4 mvFlyWheelInertia_FlyWheelFriction_GearChangeTime;                           // +0x30
            Vector3 mavGearRatios_TorqueScales_GearUpRPMs[KI_MAX_NUM_GEARS];                     // +0x40 .. +0xA0
        };

        // ---- +0x230 (0x30) -------------------------------------------------------------------
        struct SuspensionAttribs
        {
            Vector4 mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement;                        // +0x00
            Vector4 mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding; // +0x10
            Vector4 mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding; // +0x20
        };

        // ---- +0x260 (0x20) -------------------------------------------------------------------
        struct BodyRollAttribs
        {
            Vector4 mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ;  // +0x00
            Vector4 mvWheelLongForceHeightOffset_WheelLatForceHeightOffset;                       // +0x10
        };

        // ---- +0x280 (0x10) -------------------------------------------------------------------
        struct CollisionAttribs
        {
            Vector4 mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare;   // +0x00
        };

        // ---- +0x290 (0x40) -------------------------------------------------------------------
        // The DWARF's four members are 3 x Vector4 + int32_t; the trailing s32 pads the block out
        // to 0x40, which is what puts mFrontTireAttribs at the asm-literal +0x2D0.
        struct BoostAttribs
        {
            Vector4 mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset;                    // +0x00
            Vector4 mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime;   // +0x10
            Vector4 mvBoostKickAcceleration_BoostKickHeightOffset;                                        // +0x20
            s32     miBoostRule;                                                                          // +0x30
            u8      mPad0034[12];                                                                         // +0x34
        };

        // --- functions -------------------------------------------------------------------------

        // @0x825F3FB8 (840 instrs) -- the console constructor. ⚠️ ABSENT from `.ida-exports` (an
        // export-set hole: Engine::Prepare @0x825F3F38 is 31 instrs and ends at 0x825F3FB4, and the
        // next indexed symbol is 0x825F4CD8); pulled from the .i64 with headless IDA and replayed
        // through a symbolic VMX128 simulator. ⚠️ The "declared only -- no body in the tree yet"
        // that stood here is STALE: it is BODIED in VehicleAttribs.cpp (commit 05456841), and
        // VehiclePhysics::Construct calls it twice (mAIVehicleAttribs / mPlayerVehicleAttribs).
        void Construct();

        // @0x825F4CD8 (770 instrs) -- the streamed-attribute loader. Owned by a future TU;
        // declared only.
        void SetupAttribs(const PhysicsVehicleHandling& lrHandling);

        // @0x825F58E0 (622 instrs) -- derive the plain-AI set from a source set. ⭐ BODIED in
        // VehicleAttribs.cpp (attribs-setup wave, 2026-08-09).
        void SetupAttribsForAI(VehicleAttribs* lpSource);

        // @0x825F6298 (40 instrs) -- bodied in VehicleAttribs.cpp.
        void SetupAttribsForDonutAI();

        // @0x825B2B68 (92 instrs) -- bodied in VehicleAttribs.cpp.
        VehicleAttribs& operator=(const VehicleAttribs& lrSource);

        bool IsValid() const { return mbIsValid; }

        // --- members, in DWARF order ------------------------------------------------------------
        VehicleBaseAttribs mBaseAttribs;         // +0x000
        SteeringAttribs    mSteeringAttribs;     // +0x0E0
        DriftAttribs       mDriftAttribs;        // +0x100
        EngineAttribs      mEngineAttribs;       // +0x190
        SuspensionAttribs  mSuspensionAttribs;   // +0x230
        BodyRollAttribs    mBodyRollAttribs;     // +0x260
        CollisionAttribs   mCollisionAttribs;    // +0x280
        BoostAttribs       mBoostAttribs;        // +0x290
        Wheel::TireAttribs mFrontTireAttribs;    // +0x2D0
        Wheel::TireAttribs mRearTireAttribs;     // +0x310
        s32                miRaceCarID;          // +0x350  (asm: lwz/stw, 4-byte)
        u8                 mPad0354[4];          // +0x354
        // DWARF `Attribute::Key mAttribsKey`. 8 bytes on this build -- operator= @0x825B2B68
        // copies it with a single ld/std pair. (This tree's AttribSys header typedefs
        // Attribute::Key to u32; the console record here is plainly 64-bit, so it is spelled as
        // a u64 rather than dragging in a contradictory typedef.)
        u64                mAttribsKey;          // +0x358
        bool               mbIsValid;            // +0x360  (operator= sets it to a literal 1)
    };

    // ---- THE ASSERT SET ----------------------------------------------------------------------
    // Every number below is asm-literal or DWARF-ordered.
    //
    // ⚠️ A `sizeof` assert is PERMUTATION-INVARIANT -- it cannot see two members swapped, which is
    // exactly the defect this file exists to correct. Tamper-tested 2026-08-03: with sizeof +
    // top-level offsetof asserts only, transposing two registers INSIDE DriftAttribs compiled
    // clean. The per-member offsetof block below is what closes that, and it is the part to keep
    // extending, not the sizeofs.
    static_assert(sizeof(VehicleAttribs::VehicleBaseAttribs) == 0xE0, "VehicleBaseAttribs size drift");
    static_assert(sizeof(VehicleAttribs::SteeringAttribs)    == 0x20, "SteeringAttribs size drift");
    static_assert(sizeof(VehicleAttribs::DriftAttribs)       == 0x90, "DriftAttribs size drift");
    static_assert(sizeof(VehicleAttribs::EngineAttribs)      == 0xA0, "EngineAttribs size drift");
    static_assert(sizeof(VehicleAttribs::SuspensionAttribs)  == 0x30, "SuspensionAttribs size drift");
    static_assert(sizeof(VehicleAttribs::BodyRollAttribs)    == 0x20, "BodyRollAttribs size drift");
    static_assert(sizeof(VehicleAttribs::CollisionAttribs)   == 0x10, "CollisionAttribs size drift");
    static_assert(sizeof(VehicleAttribs::BoostAttribs)       == 0x40, "BoostAttribs size drift");
    static_assert(sizeof(Wheel::TireAttribs)                 == 0x40, "TireAttribs size drift");

    // top-level placement
    static_assert(offsetof(VehicleAttribs, mBaseAttribs)       == 0x000, "mBaseAttribs @0x000");
    static_assert(offsetof(VehicleAttribs, mSteeringAttribs)   == 0x0E0, "mSteeringAttribs @0x0E0");
    static_assert(offsetof(VehicleAttribs, mDriftAttribs)      == 0x100, "mDriftAttribs @0x100 (DWARF order: drift BEFORE engine)");
    static_assert(offsetof(VehicleAttribs, mEngineAttribs)     == 0x190, "mEngineAttribs @0x190 (SetupAttribsForDonutAI writes MaxTorque at 0x1B0 == +0x20)");
    static_assert(offsetof(VehicleAttribs, mSuspensionAttribs) == 0x230, "mSuspensionAttribs @0x230 (SetupSuspension reads it)");
    static_assert(offsetof(VehicleAttribs, mBodyRollAttribs)   == 0x260, "mBodyRollAttribs @0x260 (CalculateWeightTransfer reads it)");
    static_assert(offsetof(VehicleAttribs, mCollisionAttribs)  == 0x280, "mCollisionAttribs @0x280 (ApplyCrashedContactImpulse reads .y)");
    static_assert(offsetof(VehicleAttribs, mBoostAttribs)      == 0x290, "mBoostAttribs @0x290");
    static_assert(offsetof(VehicleAttribs, mFrontTireAttribs)  == 0x2D0, "mFrontTireAttribs @0x2D0 (SetupAttribsForDonutAI: addi r3,r7,0x2D0)");
    static_assert(offsetof(VehicleAttribs, mRearTireAttribs)   == 0x310, "mRearTireAttribs @0x310 (SetupAttribsForDonutAI: addi r3,r7,0x310)");
    static_assert(offsetof(VehicleAttribs, miRaceCarID)        == 0x350, "miRaceCarID @0x350 (operator=: lwz/stw)");
    static_assert(offsetof(VehicleAttribs, mAttribsKey)        == 0x358, "mAttribsKey @0x358 (operator=: ld/std)");
    static_assert(offsetof(VehicleAttribs, mbIsValid)          == 0x360, "mbIsValid @0x360 (operator=: li 1; stb)");
    static_assert(sizeof(VehicleAttribs)                       == 0x370, "VehicleAttribs size drift");

    // the two absolute offsets SetupAttribsForDonutAI writes, spelled through the members
    static_assert(offsetof(VehicleAttribs, mDriftAttribs)
                  + offsetof(VehicleAttribs::DriftAttribs,
                             mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor)
                  == 0x110, "MinSpeedForDrift must land on the asm's [this+0x110]");
    static_assert(offsetof(VehicleAttribs, mEngineAttribs)
                  + offsetof(VehicleAttribs::EngineAttribs,
                             mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges)
                  == 0x1B0, "MaxTorque must land on the asm's [this+0x1B0]");
    static_assert(offsetof(VehicleAttribs, mBaseAttribs)
                  + offsetof(VehicleAttribs::VehicleBaseAttribs, mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce)
                  == 0x070, "Mass must land on the asm's [this+0x70]");

    // the registers the (mounted) VehiclePhysics.cpp force pipeline reads through mpAttribs
    static_assert(offsetof(VehicleAttribs, mBaseAttribs)
                  + offsetof(VehicleAttribs::VehicleBaseAttribs, mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo)
                  == 0x0B0, "GetDownForce reads +0xB0 .w");
    static_assert(offsetof(VehicleAttribs, mBaseAttribs)
                  + offsetof(VehicleAttribs::VehicleBaseAttribs, mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor)
                  == 0x0D0, "GetSurfaceGrip/Roughness/LinearDrag read +0xD0");
    static_assert(offsetof(VehicleAttribs, mSteeringAttribs)
                  + offsetof(VehicleAttribs::SteeringAttribs, mvMaxAngle_StraightReactionBias)
                  == 0x0F0, "GetMaxSteeringAngleDuringDrift reads +0xF0 .x");
    static_assert(offsetof(VehicleAttribs, mDriftAttribs)
                  + offsetof(VehicleAttribs::DriftAttribs, mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor)
                  == 0x180, "ApplyDriftLatForce reads +0x180");
    static_assert(offsetof(VehicleAttribs, mSuspensionAttribs)
                  + offsetof(VehicleAttribs::SuspensionAttribs, mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding)
                  == 0x250, "StabiliseAfterHardLanding reads +0x250");
    static_assert(offsetof(VehicleAttribs, mBoostAttribs)
                  + offsetof(VehicleAttribs::BoostAttribs, mvBoostKickAcceleration_BoostKickHeightOffset)
                  == 0x2B0, "ApplyBoostKickForce reads +0x2B0");

    // ---- PER-REGISTER PLACEMENT (the permutation test) ---------------------------------------
    // One assert per data member of every sub-block, so that reordering ANY two registers fails
    // the build. Offsets are the DWARF declaration order at 16 bytes per register.
    #define BP_VA_OFF(BLOCK, MEMBER) offsetof(VehicleAttribs::BLOCK, MEMBER)

    static_assert(BP_VA_OFF(VehicleBaseAttribs, mFrontRightWheelPos)        == 0x00, "VehicleBaseAttribs order drift");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mRearRightWheelPos)         == 0x10, "VehicleBaseAttribs order drift");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mCOMOffset)                 == 0x20, "VehicleBaseAttribs order drift");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mCrashExtraVelocityFactors) == 0x30, "VehicleBaseAttribs order drift");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mDrivetimeDeformLimits)     == 0x40, "VehicleBaseAttribs order drift");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, miRaceCarID)                == 0x50, "VehicleBaseAttribs order drift");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mBrakeScaleToFactorCurve)   == 0x60, "UpdateBrakesAndGetBrakingFactor reads base+0x60");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce) == 0x70, "Mass register at base+0x70");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels) == 0x80, "VehicleBaseAttribs order drift");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl) == 0x90, "VehicleBaseAttribs order drift");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass) == 0xA0, "VehicleBaseAttribs order drift");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo) == 0xB0, "GetDownForce reads base+0xB0 .w");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff) == 0xC0, "UpdateInAirBehaviour reads base+0xC0");
    static_assert(BP_VA_OFF(VehicleBaseAttribs, mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor) == 0xD0, "GetSurface* read base+0xD0");

    static_assert(BP_VA_OFF(SteeringAttribs, mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle) == 0x00, "SteeringAttribs order drift");
    static_assert(BP_VA_OFF(SteeringAttribs, mvMaxAngle_StraightReactionBias) == 0x10, "GetMaxSteeringAngleDuringDrift reads steering+0x10");

    static_assert(BP_VA_OFF(DriftAttribs, mDriftScaleToYawTorque) == 0x00, "ApplyDriftYaw reads drift+0x00");
    static_assert(BP_VA_OFF(DriftAttribs, mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor) == 0x10, "UpdateDriftState reads drift+0x10 .x");
    static_assert(BP_VA_OFF(DriftAttribs, mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale) == 0x20, "DriftAttribs order drift");
    static_assert(BP_VA_OFF(DriftAttribs, mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift) == 0x30, "DriftAttribs order drift");
    static_assert(BP_VA_OFF(DriftAttribs, mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff) == 0x40, "DriftAttribs order drift");
    static_assert(BP_VA_OFF(DriftAttribs, mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower) == 0x50, "ApplyDriftLatForce reads drift+0x50");
    static_assert(BP_VA_OFF(DriftAttribs, mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping) == 0x60, "ApplyNaturalDriftForces reads drift+0x60");
    static_assert(BP_VA_OFF(DriftAttribs, mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime) == 0x70, "DriftAttribs order drift");
    static_assert(BP_VA_OFF(DriftAttribs, mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor) == 0x80, "DriftAttribs order drift");

    static_assert(BP_VA_OFF(EngineAttribs, mTorqueCurve) == 0x00, "EngineAttribs order drift");
    static_assert(BP_VA_OFF(EngineAttribs, mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM) == 0x10, "Engine::ComputeGear reads engine+0x10 .x");
    static_assert(BP_VA_OFF(EngineAttribs, mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges) == 0x20, "SetupAttribsForDonutAI writes engine+0x20 .x");
    static_assert(BP_VA_OFF(EngineAttribs, mvFlyWheelInertia_FlyWheelFriction_GearChangeTime) == 0x30, "EngineAttribs order drift");
    static_assert(BP_VA_OFF(EngineAttribs, mavGearRatios_TorqueScales_GearUpRPMs) == 0x40, "EngineAttribs order drift");

    static_assert(BP_VA_OFF(SuspensionAttribs, mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement) == 0x00, "SetupSuspension reads suspension+0x00");
    static_assert(BP_VA_OFF(SuspensionAttribs, mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding) == 0x10, "SuspensionAttribs order drift");
    static_assert(BP_VA_OFF(SuspensionAttribs, mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding) == 0x20, "StabiliseAfterHardLanding reads suspension+0x20");

    static_assert(BP_VA_OFF(BodyRollAttribs, mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ) == 0x00, "CalculateWeightTransfer reads bodyroll+0x00");
    static_assert(BP_VA_OFF(BodyRollAttribs, mvWheelLongForceHeightOffset_WheelLatForceHeightOffset) == 0x10, "HandleWheelPairFriction reads bodyroll+0x10");

    static_assert(BP_VA_OFF(CollisionAttribs, mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare) == 0x00, "ApplyCrashedContactImpulse reads collision+0x00 .y");

    static_assert(BP_VA_OFF(BoostAttribs, mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset) == 0x00, "UpdateBoost reads boost+0x00");
    static_assert(BP_VA_OFF(BoostAttribs, mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime) == 0x10, "ApplyNormalBoostForce reads boost+0x10");
    static_assert(BP_VA_OFF(BoostAttribs, mvBoostKickAcceleration_BoostKickHeightOffset) == 0x20, "ApplyBoostKickForce reads boost+0x20");
    static_assert(BP_VA_OFF(BoostAttribs, miBoostRule) == 0x30, "BoostAttribs order drift");

    #undef BP_VA_OFF
}
}
