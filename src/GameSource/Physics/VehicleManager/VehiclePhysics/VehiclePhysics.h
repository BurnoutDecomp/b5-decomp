#pragma once

// BrnPhysics::Vehicle::VehiclePhysics -- MINIMAL reconstruction.
//
// The full VehiclePhysics class (a SimpleVehiclePhysics subclass with engine, drift,
// boost, slam/shunt/airram/spin state and several hundred methods) is a separate future
// TU. Here we reconstruct ONLY the two nested effect structs that RaceCarState embeds by
// value: SlamEffect (offset 880) and ShuntEffect (offset 928). Both are nested in
// `struct VehiclePhysics` per references/DecFIGS/dwarfdump/.../VehiclePhysics.h, giving the
// fully-qualified names BrnPhysics::Vehicle::VehiclePhysics::SlamEffect / ::ShuntEffect that
// RaceCarState's member declarations use.
//
// ShuntEffect is its own ledger TU (ShuntEffect::IsActive @0x8236AFC0); SlamEffect methods
// are likewise owned elsewhere. Every method on both structs is therefore declared-only so
// those TUs can define the bodies later without an ODR clash. GetDesiredSpeed/GetLife/etc.
// return VecFloat in the SDK; per the project de-SIMD convention they are spelled f32 here.
//
// ADDITIVE GROW (Vehicle-physics group): the original file reconstructed ONLY the nested
// SlamEffect / ShuntEffect. This group bodies two header-homed VehiclePhysics methods --
// GetShowtimeDeformationScale @0x827E24E8 and IsCounterSteeringAtLowSpeed @0x825BFEF0 -- so the
// two methods (and the single velocity member IsCounterSteeringAtLowSpeed reads) are ADDED to
// the same struct. The pre-existing nested structs are untouched; nothing is reordered/retyped.
//
// ADDITIVE GROW (Vehicle-physics class TU): three out-of-line scalar predicates -- bodied in
// VehiclePhysics.cpp -- are ADDED to the struct: GetNumberOfWheelsOnTheGround @0x825B2FE0,
// IsBeingSlamedOrShunted @0x825E6D50 and IsBeingSlamedOrShuntedByRaceCar @0x82615290, together
// with the slam/shunt state they read (mfSlamLife @+0x111C, mShuntEffect @+0x1130,
// mi8SlammingRaceCarId @+0x13E0), each pinned BY NAME. The pre-existing members/methods are
// untouched; nothing is reordered/retyped/removed.
//
// ===== RE-PARENTED onto the real base chain (physics wave 2b) =====
// The DecFIGS DWARF gives the ORIGINAL declaration verbatim:
//     references/DecFIGS/dwarfdump/.../VehiclePhysics.h:810
//         struct BrnPhysics::Vehicle::VehiclePhysics : public SimpleVehiclePhysics
// and the complete non-static data-member list of VehiclePhysics (62 members) and of the three
// base classes (28). This file used to be a FLAT struct with no base, which meant a member the
// base already owned had to be re-declared here to be usable -- 25 of the 53 members it declared
// were duplicates of a base/own member, 20 of them under a DIFFERENT NAME. Every one of those is
// a silent-split hazard: two copies of one console member, written through one and read through
// the other, with nothing to grep.
//
// All 25 are now removed. The base subobject offset is VERIFIED from the asm -- every
// VehiclePhysics method that calls a base entry passes `addi r3,this,0x10` (ApplyNormalBoostForce
// @0x825D3174, ApplyBoostKickForce @0x825D3294, ApplySuspensionForces @0x825D2108), so the
// vptr occupies +0x00..+0x0F and the ExternallySimulatedBody subobject starts at +0x10. With
// sizeof(ExternalPhysicsBody) == 0x120 that puts SimpleVehiclePhysics's own members at +0x130,
// which GetHeightAboveRoad @0x825B3998 confirms directly (`addi r11,r4,0x130` for maWheels[0],
// `addi r8,r4,0x210` for maWheels[1] -- stride 0xE0). BASE-frame offset X therefore appears as
// X+0x10 in every VehiclePhysics asm listing, and the annotations below are in the VP frame.
//
// The renames, each settled from the ASM (not from the name):
//   mLocalVelocity            +0x60  -> ExternallySimulatedBody::mAngularVelocity
//                                      (IsCounterSteeringAtLowSpeed @0x825BFF7C reads its .y lane
//                                      = the YAW RATE; ApplyBoostKickForce damps it as omega)
//   mUpAxis                   +0x20  -> mTransform.yAxis  (GetCarGroundDistanceCheck @0x825C0100
//                                      tests its .y lane < 0 == "car is inverted")
//   maLocalWheelPositions     +0x530 -> SimpleVehiclePhysics::maLocalTractionPoints
//   mGroundNormal             +0x580 -> mAboveGroundTestResult.mIntersectionNormal
//   mfWaterDepth              +0x590 -> mAboveGroundTestResult.mfVerticalDistance
//   mWaterContactTag          +0x594 -> mAboveGroundTestResult.mCollisionTag, BE-high half
//   mRepresentativeContactTag +0x596 -> mAboveGroundTestResult.mCollisionTag, BE-low  half
//   mbAboveGroundTestValid    +0x598 -> mAboveGroundTestResult.mbValid
//     (all five proved by SimpleVehiclePhysics::SetAboveGroundTestResult @0x826029D4, which takes
//      `addi r11,this,0x570` and then writes +0x00/+0x10/+0x24/+0x26/+0x20/+0x28 off it)
//   mfCarGroundCheckExtent    +0x6A4 -> SimpleVehiclePhysics::mHalfExtent .y lane
//   mLinearImpulseAccumulator +0x100 -> ExternalPhysicsBody::mTotalTorque      (see FLAG below)
//   mAngularImpulseAccumulatorRow +0x120 -> ExternalPhysicsBody::mTotalAngularImpulse
//   mfSlamSteering/-OriginalSteering/mfSlamLife/mfTotalSlamTime/mfRecoveryTime/mi8SlamNumber
//     +0x1114..+0x1128 -> the six scalar fields of this class's own `SlamEffect mSlamEffect`
//     @+0x1100 (AddSlam @0x825D4880/4889C/4904 reads +0x111C/+0x1120/+0x1128; the struct ends at
//     +0x1130, which is exactly where mShuntEffect begins -- the DWARF's very next member)
//   mbSlamActive              +0x135D -> mbJustBeenSlammed
//   mi8SlammingRaceCarId      +0x13E0 -> mi8LastAttackersRaceCarIndex
//     (both proved by VehicleOutputInterface::UpdateRaceCarState @0x825EC8C0/@0x825ECB0C, which
//      copies +0x13E0 to RaceCarState+0x43C == mi8LastAttackersRaceCarIndex and +0x135D to
//      RaceCarState+0x44F == mbJustBeenSlammed)
//   mWeightTransferMirror     +0x1330 -> mPreviousWorldSpaceVelocity
//     (UpdateSuspension @0x8261F6F4 does `lvx128 v0,this,0x50 ; stvx128 v0,this,0x1330` -- it
//      snapshots mLinearVelocity, NOT a "weight register", so CalculateWeightTransfer can
//      difference it against this frame's velocity. maSpinEffects[8] ends at exactly 0x1330.)
//
// ⚠️ CORRECTION -- the committed note on mLinearImpulseAccumulator was WRONG, and so was the
// wave-2 finding that repeated it ("mLinearImpulseAccumulator IS the base's mTotalLinearImpulse").
// Its "+0x100" is a VehiclePhysics-frame offset, so it is base+0xF0 == mTotalTorque, not
// base+0x100 == mTotalLinearImpulse. ApplyBoostKickForce @0x825D338C-0x825D3408 damps exactly
// three registers along mTransform.xAxis -- +0x60, +0x100 and +0x120 -- and reading those as
// {angular velocity, TORQUE, angular impulse} is the only physically coherent set for a wheelie
// limiter; {angular velocity, LINEAR impulse, angular impulse} is not. AddLocalForce @0x825A183C
// independently confirms base+0xE0 = mTotalLinearForce and base+0xF0 = mTotalTorque.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"   // the base chain: SimpleVehiclePhysics : ExternalPhysicsBody : ExternallySimulatedBody
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, Vector4
#include "types.hpp"          // f32, s8
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"   // Wheel + Wheel::RoadContact
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"   // VehicleAttribs (canonical home; replaces the by-name slice this file used to carry)
#include "rw/physics/rigidbody.h"   // rw::physics::InputSpace (AirRamEffect::meImpulseSpace)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<N> (air-ram/spin slot allocators)
// BrnPhysics::SuspensionSpring at its DWARF home. This file used to carry a MINIMAL OWNING SLICE
// of the same namespace-scope type (its own comment: "when the real Spring1D.h lands, REPLACE this
// slice with an include of the committed type"). Spring1D.h has since landed with the identical
// three-register storage (Spring1D.h:147-149, sizeof == 0x30 == the maSprings stride) plus the full
// setter/getter API, so the fork is RETIRED here in favour of the include -- the two definitions
// were an ODR clash for any TU that reached both (every camera-behaviour TU does, via BehaviourRig.h).
#include "GameSource/Physics/PhysicsUtilities/Spring1D.h"   // BrnPhysics::SuspensionSpring (canonical home)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Engine.h"   // BrnPhysics::Vehicle::Engine (mEngine @+0xF00)
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"   // CgsInput::Device::WheelFFSpring (mWheelFFSpring @+0x13D0)
// ⭐ own-block closure wave (2026-08-03): mPreviousControls @+0x1090 is the WHOLE 0x48-byte
// BrnPlayerDriverControls by value (DWARF VehiclePhysics.h:905), so the complete type is needed
// here. That header includes only BrnCommonTypes/types/CgsVariableEventQueue -- no cycle.
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"   // BrnPlayerDriverControls (0x48)

// ⚠️ CLASS-KEY FIX 2026-08-06 (UpdateVehiclePhysics wave): this fwd-decl said `struct Random`
// while CgsRandom.h defines `class Random` -- the silent ?AU/?AV mangling fork the moment any
// TU references Random through only this declaration (Update now takes a Random&). Keyed to
// match the definition.
namespace CgsNumeric { class Random; }   // UpdateRoadNoise draws from the shared Random ring (CgsRandom.h)

namespace BrnPhysics
{
// Forward decl: the streamed deformation spec Prepare and the seat bring-up leg consume by
// pointer. Full type owned by BrnStreamedDeformationSpec.h. ⚠️ RETIRED (seat wave 2026-08-05):
// this used to be forward-declared INSIDE namespace Vehicle -- a namespace FORK of the real
// BrnPhysics::Deformation::StreamedDeformationSpec, so any body written against the fork could
// never link against the real spec ([[odr-forks-link-silently]] pattern, caught at compile here
// only because the seat leg dereferences it).
namespace Deformation { struct StreamedDeformationSpec; }

namespace Vehicle
{
    // ⚠️ RETIRED 2026-08-03 (own-block closure wave): this file used to forward-declare
    // `class BrnPlayerDriverControls;` (also mis-keyed as `class` where the DWARF says `struct`).
    // The type is now INCLUDED above, because mPreviousControls @+0x1090 embeds it BY VALUE.

    // The spec name Vehicle-scope code uses; resolves to the REAL Deformation-namespace type.
    using Deformation::StreamedDeformationSpec;

    // Forward decl (own-block closure wave): the per-car physics debug component mpDebugComponent
    // @+0x13E4 points at (DWARF VehiclePhysics.h:982 types it
    // `BrnPhysics::Vehicle::DebugComponent *`). Only ever null-checked and forwarded here; the
    // component's own type is a separate TU.
    struct DebugComponent;

    // ----- RETIRED SLICE (2026-08-03). This file used to carry a "by-name, NOT offset-faithful"
    //       `struct VehicleAttribs` of ~14 registers, with a standing note to replace it with an
    //       include of the real type once that landed. VehicleAttribs.h has now landed, at the
    //       DWARF member order, so the slice is gone and the include above supplies the type.
    //
    //       ⭐ For the record: the slice's OFFSETS were right and the full definition that used to
    //       live inside VehicleAttribs.cpp was wrong (it transposed mDriftAttribs and
    //       mEngineAttribs). The register->member map the bodies below were written against is
    //       preserved exactly -- every lane index is unchanged, only the register NAMES move to
    //       the DWARF's. The retired placeholder name, its console offset, and the DWARF member
    //       it became:
    //           "mvBaseParams"        +0x70  -> mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce
    //           "mvAeroParams"        +0xB0  -> mBaseAttribs.mvRearWheelMass_..._DownForceLiftCo
    //           "mvSurfaceBlend"      +0xD0  -> mBaseAttribs.mvFrontSurfaceGripFactor_..._SurfaceLinearDragFactor
    //           "mvSteeringParams"    +0xF0  -> mSteeringAttribs.mvMaxAngle_StraightReactionBias
    //           "mvDriftParams0".."7" +0x110..+0x180 -> mDriftAttribs' eight Vector4s, in order
    //           "mvCrashImpulseScale" +0x280 -> mCollisionAttribs.mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare
    //           "mBoostAttribs"       +0x290 -> mBoostAttribs (same name, same three registers)

    // BrnPhysics::Vehicle::VehiclePhysics -- the full race-car handling body. Derives from
    // SimpleVehiclePhysics per the DWARF (VehiclePhysics.h:810); see the re-parenting note at the
    // top of this file for the base-frame offset proof and the 25 duplicate members it retired.
    struct VehiclePhysics : public SimpleVehiclePhysics
    {
        // EVehicleDrivenWheel is NOT nested: the DWARF homes it at NAMESPACE scope
        // (BrnPhysics::Vehicle::EVehicleDrivenWheel, BrnSimpleVehiclePhysics.h:52) and the base
        // header above already provides it. The nested copy this file used to carry was a fork of
        // that type -- it shadowed the inherited one inside the class, so a value formed here could
        // not be passed to any base method that takes the real enum. Retired with the member
        // duplicates for the same reason.

        // Accessor for one driven wheel (DWARF VehiclePhysics.h:244). GetHeightAboveRoad reads
        // each wheel's road-contact result through this. maWheels is the BASE's array.
        const Wheel& GetWheel(EVehicleDrivenWheel leWheel) const { return maWheels[leWheel]; }

        // The vehicle "up" axis used to decide whether a wheel's road contact is on the ground
        // (the asm's lvx at _R4+0x20 + vmsum3fp128 dot vs 0.5). +0x20 is base+0x10 == the SECOND
        // row of mTransform, i.e. the body Y axis -- confirmed by GetCarGroundDistanceCheck
        // @0x825C0100, which splats that row's .y lane and tests it < 0 to detect an inverted car.
        // (Matrix44Affine::Up() is the same row.)
        const Vector3& GetUpAxis() const { return mTransform.yAxis; }

        // A directional "slam" impulse with decay/steering and a slam-index counter.
        struct SlamEffect
        {
            static const s8 KI8_MAX_NUM_SLAMS = 3;

            Vector3 mForce;
            f32     mfDecay;
            f32     mfSteering;
            f32     mfOriginalSteering;
            f32     mfSlamLife;
            f32     mfTotalSlamTime;
            f32     mfRecoveryTime;
            s8      mi8SlamNumber;

            // Owned by the VehiclePhysics/ShuntEffect ledger TUs -- declare only.
            bool IsActive() const;
            void Clear();
            void StopCurrentSlam();
        };

        // A "shunt" (sustained push toward a desired speed); state packed into two SIMD
        // registers (direction+desired-speed, life+speed-increase-to-quit).
        struct ShuntEffect
        {
            Vector3Plus mDirectionPlusDesiredSpeed;
            Vector4     mv4_Life_SpeedIncreaseToQuit;

            // ShuntEffect is its own ledger TU (IsActive @0x8236AFC0) -- declare only ALL.
            // SDK VecFloat returns are de-SIMD'd to f32 to match the project convention.
            bool IsActive() const;
            void Clear();
            f32  GetDesiredSpeed() const;
            bool ForceSlip() const;
            f32  GetLife() const;
            f32  GetSpeedIncreaseToQuit() const;
            void SetLife(f32);
            void SetSpeedIncreaseToQuit(f32);
        };

        // ----- ADDITIVE GROW (C08 airborne/water/freeze/spin group): the two queued-impulse effect
        //       records + their slot-allocator capacities. Nested in VehiclePhysics per the DWARF
        //       (references/DecFIGS/.../VehiclePhysics.h:188 AirRamEffect / :198 SpinEffect), member
        //       names + order verbatim. sizeof(AirRamEffect)==0x30 / sizeof(SpinEffect)==0x20, matching
        //       the mAirRamEffect[4] (stride 48) and maSpinEffects[8] (stride 32) strides the X360
        //       UpdateAirRam/UpdateSpinEffects/AddAirRam asm indexes. -----

        static const u8 KU_MAX_AIR_RAMS = 4;   // VehiclePhysics.h:46  (mUsedAirRams capacity)
        static const u8 KU_MAX_SPINS    = 8;   // VehiclePhysics.h:47  (mUsedSpins capacity)

        // A timed body- or world-space "air ram" impulse with decay. UpdateAirRam counts mfTimerTillFire
        // down by dt; when it reaches 0 it fires mImpulse (at mPosition, in meImpulseSpace) and then
        // scales the stored impulse by (1 - mfDecay) for the next fire. (DWARF VehiclePhysics.h:188.)
        struct AirRamEffect
        {
            Vector3                  mImpulse;        // :190  (+0x00) the impulse to apply when it fires
            Vector3                  mPosition;       // :191  (+0x10) the point of application
            f32                      mfDecay;         // :192  (+0x20) per-fire impulse decay fraction
            rw::physics::InputSpace  meImpulseSpace;  // :193  (+0x24) WORLD_SPACE / BODY_SPACE of mImpulse
            f32                      mfTimerTillFire;  // :194  (+0x28) countdown to the next fire
        };

        // A timed world-space angular impulse (spin). UpdateSpinEffects counts mfTimeRemaining down by
        // dt while applying mForce as an angular impulse, releasing the slot when it expires.
        // (DWARF VehiclePhysics.h:198.)
        struct SpinEffect
        {
            Vector3 mForce;           // :200  (+0x00) the angular impulse applied each frame
            f32     mfTotalTime;      // :201  (+0x10) the originally-requested duration
            f32     mfTimeRemaining;  // :202  (+0x14) remaining lifetime (counts to 0)
        };

        // ----- ADDITIVE GROW (C06 steering/drift/handbrake group): the drift sub-force gate bitfield
        //       (DWARF VehiclePhysics::DriftFlags). One uint8 (mu8DriftFlags) whose bits gate the
        //       individual drift forces. EnterDrift/ExitDrift set it to KU_DRIFT_FLAG_DO_ALL;
        //       ApplyDriftYaw gates on KU_DRIFT_FLAG_APPLY_TORQUE; MaintainDriftSpeed on
        //       KU_DRIFT_FLAG_MAINTAIN_SPEED. (Pinned @+0x10F4.) -----
        struct DriftFlags
        {
            static const u8 KU_DRIFT_FLAG_DO_ALL               = 255;
            static const u8 KU_DRIFT_FLAG_MAINTAIN_SPEED       = 1;
            static const u8 KU_DRIFT_FLAG_APPLY_TORQUE         = 2;
            static const u8 KU_DRIFT_FLAG_SCALE_LAT_BY_Z_SPEED = 4;
            static const u8 KU_DRIFT_FLAG_GRIP_FROM_CONTROLS   = 8;

            u8 mu8DriftFlags;

            bool DoMaintainSpeed() const { return (mu8DriftFlags & KU_DRIFT_FLAG_MAINTAIN_SPEED) != 0; }
            bool DoApplyTorque()   const { return (mu8DriftFlags & KU_DRIFT_FLAG_APPLY_TORQUE) != 0; }
        };

        // ----- Vehicle-physics group (class TU): three scalar predicates (bodies in VehiclePhysics.cpp) -----

        // @0x825B2FE0: the number of driven wheels currently on the ground (counts the per-wheel
        // road-contact on-ground flags maWheels[i].GetRoadContact().mbIsOnGround). The X360 reads the
        // four flags at +0x158/+0x238/+0x318/+0x3F8 (= maWheels stride 0xE0, RoadContact +0x28).
        s32 GetNumberOfWheelsOnTheGround() const;

        // @0x825E6D50: true while the vehicle is being slammed (mfSlamLife > 0) OR shunted
        // (mShuntEffect.IsActive()).
        bool IsBeingSlamedOrShunted() const;

        // @0x82615290: true when this vehicle is being slammed/shunted specifically by the race car
        // with the queried id (mi8SlammingRaceCarId == liRaceCarId AND IsBeingSlamedOrShunted()).
        bool IsBeingSlamedOrShuntedByRaceCar(s8 li8RaceCarId) const;

        // ----- ADDITIVE GROW (C10 showtime/aftertouch group): the base physics-body force/impulse
        //       operations the RaceCarPhysics C10 functions funnel into, plus AddAirRam (a queued
        //       body-space impulse), the per-frame Update/UpdateSteering spine, and the linear-
        //       velocity accessors. DECLARE-ONLY -- bodies owned by the ExternalPhysicsBody / base
        //       VehiclePhysics TUs (the integrator + driving spine). Declared here so the C10 leaf TU
        //       resolves them BY NAME under the per-TU compile gate. The world-space force/impulse
        //       args arrive in a VMX register on X360; spelled as a Vector3 arg in the de-SIMD'd form.
        //       GetLinearVelocity/SetLinearVelocity/GetWorldUpRow are inline over already-pinned
        //       members. -----

        // @0x826412C0: the base per-frame tick (RaceCarPhysics::Update chains here).
        // Carries the same two pass-through vector arguments its caller restores into v1/v2
        // before chaining (`vmr128 v2,v126 ; vmr128 v1,v127` @0x8264185C in RaceCarPhysics::Update);
        // lrTimeStep.x is the frame dt -- see RaceCarPhysics.h for the recovery.
        //
        // ⭐⭐ SIGNATURE CONFORMED 2026-08-06 (UpdateVehiclePhysics wave). The DWARF declares
        // this VIRTUAL (VehiclePhysics.h:1084, vtable slot +0xC -- exactly the slot
        // VehicleManager::UpdateVehiclePhysics dispatches through at 0x82645A34/0x82645A5C)
        // with `(VecFloat, VecFloat, const Matrix44Affine*, const BrnPlayerDriverControls*,
        // bool, bool, bool, Random&)`. The four `s32 a2/a5/a6/a7` placeholders are now
        // RECOVERED from that call site: a2 = the manager's camera matrix, a5 = the
        // player-aftertouch-additive flag, a6 = (meShowtimeBehaviour == 2), a7 = the
        // manager's CgsNumeric::Random. The two vector args KEEP this tree's trailing
        // position (an established documented deviation: the console passes them in v1/v2
        // regardless of declaration order) and lbApplyAftertouch is re-named to what the
        // caller actually passes (the manager's mbImpactTime byte, r6). Kept NON-virtual as
        // modelled -- every call site's static type is the exact dynamic type, and the
        // vtable head carries its own open flags (do not grow it silently).
        void Update(const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                    const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                    bool lbPlayerAftertouchForceAdditive, bool lbShowtimeAllowed,
                    CgsNumeric::Random& lrRandom,
                    Vector3 lrPassThroughV1, Vector3 lrTimeStep);

        // ----- ADDITIVE GROW (C11 group): the crash master-gate accessors the TrafficPhysics layer
        //       consults. (DWARF BrnSimpleVehiclePhysics.h :285 IsCrashing / .cpp:805 SetCrashing.)
        //
        // ⚠️ RETIRED 2026-08-02 (physics wave 3): this home used to RE-DECLARE
        //   `bool IsCrashing() const;`
        // as "DECLARE-ONLY ... bodied by the SimpleVehiclePhysics/VehiclePhysics TU". That comment
        // described an intention, not C++ semantics: a redeclaration in the derived class HIDES the
        // base's inline `SimpleVehiclePhysics::IsCrashing() { return mbCrashing; }` and mangles to
        // `?IsCrashing@VehiclePhysics@...` -- a symbol no TU defines and no console function
        // corresponds to. MEASURED: it was one of six such declarations that made VehiclePhysics.cpp
        // unlinkable (LNK2019). The name is now simply INHERITED. SetCrashing stays: it is a real
        // virtual override with a body in this TU. -----
        virtual void SetCrashing();

        // @0x825D3720 -- the per-frame steering model (577 X360 insns, standalone; also called by
        // UpdateDriving @0x82638354 and UpdateCrashing @0x82638F6C, and by RaceCarPhysics::Update
        // @0x826418A4 on the frozen/engine-only path).
        // ⭐⭐ SIGNATURE CONFORMED 2026-08-07 (orchestrator wave). The committed 2-arg
        // `(s8 li8CarType, f32 lfSteer)` was an arg-map guess off one call site. The DWARF
        // (VehiclePhysics.h:1499) and the PS3 out-of-line copy
        // (`_ZN...14UpdateSteeringEffN2rw4math3vpu8VecFloatEb` @0x6DB90C) both spell
        // (f32, f32, VecFloat, bool), and BOTH X360 call sites agree on the arg map:
        //   UpdateDriving @0x82638344: f1 = copy+0x10 (mfSteering), f2 = copy+0x04 (mfGas),
        //     r6 = copy+0x41 (mbIsSteeringWheel), v1 = the frame dt vector;
        //   RaceCarPhysics::Update @0x82641890: f1 = controls->mfSteering, f2 = f31 == 0.0f
        //     (flt_82001CC0 -- no gas on the frozen path), r6 = mbIsSteeringWheel, v1 = v127.
        // BODIED in VehiclePhysics.cpp (this wave).
        void UpdateSteering(f32 lfSteering, f32 lfGas, VecFloat lvfTimeStep,
                            bool lbIsSteeringWheel);

        // @0x825FE118: queue an air-ram impulse. BODIED by the C08 airborne/water/freeze/spin group
        // (see VehiclePhysics.cpp). The DWARF-authoritative signature is the 6-arg form below
        // (AddAirRam(uint32_t, float32_t, float32_t, Vector3, Vector3, float32_t)); luFlags selects the
        // direction source (custom world/body bits 0x1/0x4, or body-axis seed bits 0x8/0x10/0x20) and
        // the position source (custom bit 0x100, or transform-column bits 0x200..0x1000). RECONCILE:
        // this REPLACES the earlier C10 4-arg DECLARE-ONLY stub `AddAirRam(s32,f32,f32,f32)`; C10's
        // call site (SetPlayerVehicleInShowtime / UpdateShowtimePhysics, input-space dir bits 5130/3082)
        // must pass the 6-arg form (flags, factor, decay, customImpulse, customPosition, timerTillFire).
        void AddAirRam(u32 luFlags, f32 lfFactor, f32 lfDecay,
                       Vector3 lvCustomImpulse, Vector3 lvCustomPosition, f32 lfTimerTillFire);

        // ⚠️ RETIRED 2026-08-02 (physics wave 3). This home used to re-declare all four
        // ExternalPhysicsBody accumulators:
        //     void AddWorldSpaceForce(const Vector3&);          // @0x825BE710
        //     void AddWorldSpaceImpulse(const Vector3&);        // @0x825BE8F8
        //     void AddWorldSpaceAngularImpulse(const Vector3&); // @0x825BEAA8
        //     void AddWorldSpaceTorque(const Vector3&);         // @0x825BE9D0
        // each commented "(ExternalPhysicsBody ...) DECLARE-ONLY here (owned by the base TU)".
        // ExternalPhysicsBody.h had already written down the rule these violated:
        //     "When VehiclePhysics is re-parented onto this chain those local declarations must be
        //      deleted, not left to shadow these."
        // The re-parenting (wave 2b) landed; the deletions did not. A derived redeclaration HIDES
        // the base name, and these even differ in signature (base takes Vector3 BY VALUE, these took
        // `const Vector3&`), so all 29 call sites in this TU mangled to VehiclePhysics-scoped symbols
        // that no TU can ever define. MEASURED as four of the LNK2019s that made this TU unmountable.
        // The names are now INHERITED from ExternalPhysicsBody -- which is also what the console
        // does: ApplyCarContactImpulse @0x825D4D24/@0x825D4D34 calls
        // `BrnPhysics__ExternalPhysicsBody__AddWorldSpaceImpulse` / `...AngularImpulse` with
        // r3 == this + 0x10 (the ExternalPhysicsBody sub-object), not a VehiclePhysics symbol.

        // @0x825FA448: the drift-entry test UpdateDriftState chains into to LATCH a new drift (it
        // tail-calls EnterDrift when the entry conditions are met).
        // ⭐ BODIED 2026-08-03 -- and it was the LAST unresolved external standing between
        // VehiclePhysics.cpp and the build. It had been declare-only ("bodied by its own TU") for as
        // long as this header existed, with a FIVE-f32 signature that no console function ever had.
        // ⚠️ It is ABSENT from `.ida-exports/BURNOUT_X360_ARTIST.XEX/` (third confirmed export-set
        // hole). It is a perfectly ordinary named function in the IDB -- headless IDA 9.3 reports
        // `0x825FA448..0x825FA748`, 192 instructions -- and `fn_index.txt` shows the gap directly
        // (EnterDrift @0x825FA268 +120 instrs ends exactly at 0x825FA448; the next indexed symbol is
        // UpdateDriftScale @0x825FA748). The PS3 DecFIGS set also carries it, at 0x6C8924.
        void CheckForEnteringDrift(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                                   f32 lfAbsDriftScale, f32 lfSpeedMPS, VecFloat lvfTimeStep);

        // ⭐⭐ RETIRED 2026-08-07 (orchestrator wave). This home used to declare a 2-argument
        // stand-in `void AddTractionPoint(s32, u32);` that HID the base's real 4-argument
        // SimpleVehiclePhysics::AddTractionPoint(EVehicleDrivenWheel, Vector3, Vector3, u32).
        // The X360 asm settles it: RaceCarPhysics::AddTractionPoint @0x825FFB04 does `bl
        // SimpleVehiclePhysics::AddTractionPoint` with EVERY incoming register untouched
        // (r4=wheel, r5=tag, v1=position, v2=normal pass straight through) -- there is no
        // 2-argument entry anywhere in the image, and the PS3 DecFIGS carries only the 4-arg
        // pair (RCP @0x6E77E4 -> SVP @0x6E742C). The base body is now reconstructed in
        // BrnSimpleVehiclePhysics.cpp and the name is simply INHERITED here (same rule as the
        // IsCrashing/AddWorldSpace* retirements above: a derived redeclaration would shadow it).

        // @ ExternallySimulatedBody: the world-space linear velocity (mLinearVelocity, base +0x40,
        // == VehiclePhysics frame +0x50). The member now comes from the base.
        const Vector3& GetLinearVelocity() const { return mLinearVelocity; }
        void SetLinearVelocity(const Vector3& lvVelocity) { mLinearVelocity = lvVelocity; }

        // The body world-transform up row. The C10 launch builds its push direction from worldUp.
        // Same row as GetUpAxis (mTransform.yAxis) -- these two were separately-named views of the
        // one console register (+0x20) and are now both expressed over the base's mTransform.
        const Vector3& GetWorldUpRow() const { return mTransform.yAxis; }

        // ----- ADDITIVE GROW (stunt-offences group): three transform/velocity accessors over the
        //       already-pinned members, mirroring GetLinearVelocity/GetWorldUpRow. BrnPhysics::
        //       StuntOffencesManager reads the car transform (+0x10), its position column (wAxis,
        //       +0x40) and the +0x60 angular-velocity register through these (the host vptr is 8
        //       bytes wide -- the stunt code must NOT touch raw console byte offsets). -----
        const Matrix44Affine& GetTransform() const { return mTransform; }
        const Vector3& GetPosition() const { return mTransform.wAxis; }
        // The +0x60 angular-velocity register. This slice used to call it mLocalVelocity and declare
        // its own copy; +0x60 is base+0x50 == ExternallySimulatedBody::mAngularVelocity, so the
        // duplicate is retired and this reads the base member. UpdateInAirRotations integrates it
        // (transposed into car space) to score rolls/spins.
        const Vector3& GetAngularVelocity() const { return mAngularVelocity; }

        // ----- @0x825FD218: re-seed every wheel's body-point velocity and spin rate from the body's
        //       current motion, then re-seed the engine from their average. Called on the three
        //       car-PLACEMENT paths: Reset (mpAttribs != NULL), TrafficPhysics::PreparePhysical, and
        //       VehicleManager::HandleRaceCarRaceCarContact (once per car, after a slam/shunt
        //       impulse). BODIED 2026-08-03 in VehiclePhysics.cpp -- see the block comment there.
        //
        //       ⭐ THE SIGNATURE IS NOW ATTESTED, NOT INFERRED. This declaration used to carry
        //       "FLAG: arg shape inferred from the call site". The PS3 ELF export 0x3E10FC is
        //       `_ZN10BrnPhysics7Vehicle14VehiclePhysics18SetWheelVelocitiesEN2rw4math3vpu7Vector3E`
        //       == SetWheelVelocities(rw::math::vpu::Vector3). Flag removed.
        //
        //       ⚠️ The parameter is DEAD in the console body (see the .cpp) -- kept because it is
        //       the console's own signature and both surviving call sites pass it.
        void SetWheelVelocities(Vector3 lvVelocity);

        // ==========================================================================================
        // ⭐ Construct @0x8262DBD0 / Reset(Vector3) @0x825FDD78 -- BOTH BODIED IN VehiclePhysics.cpp
        //    as of 2026-08-03. Reset was decoded out of BURNOUT_X360_ARTIST.XEX.i64 with headless
        //    IDA 9.3 and replayed through a symbolic VMX128 simulator (it IS an `.ida-exports`
        //    hole); the decode is preserved below because it is the record of that work.
        //
        // ⚠️ CORRECTION (Construct wave): the sentence that stood here said "Both are `.ida-exports`
        //    holes". **Construct is not one.** `.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8262DBD0.json`
        //    exists and carries the full `assembly` plus a complete `xrefs_from`; only its
        //    `pseudocode` is degenerate (the bare prototype), which is what got mis-read as a hole.
        //    The whole function was re-pulled from that JSON, without opening IDA at all. The
        //    boundary check the project rule prescribes passes: 0x8262DBD0..0x8262DD50 is one
        //    prologue + one epilogue, and VehiclePhysics::Destruct starts at 0x8262DD58.
        //
        // ---- Reset(Vector3 lvVelocity) @0x825FDD78, 0x825FDD78..0x825FE118 (232 instrs) --------
        //   SimpleVehiclePhysics::Reset();                       // 0-arg overload -- @0x825D9A58
        //                                                        // builds its OWN zero (vspltisw128)
        //   if (mpAttribs == NULL) {                             // lwz r11,0x720(this) ; beq
        //       maWheels[0..3].Reset(0);                         // this+0x130/0x210/0x2F0/0x3D0
        //       mEngine.Reset(0);                                // this+0xF00
        //   } else {
        //       SetWheelVelocities(lvVelocity);                  // ⛔ THE BLOCKER -- see below
        //   }
        //   maLocalTractionPoints[0..3]                       = 0     // +0x530/540/550/560
        //   mvSpeedOnLastCrashMPH_TimeCrashing_...            = 0     // +0xEF0 (whole register)
        //   mWeightTransfer                                   = 0     // +0xEE0
        //   mvSteeringAngle_...      .x/.y/.z = 0 (.w untouched)      // +0xFE0
        //   mvSpare_MaintainedSpeed_...   .y/.z/.w = 0 (.x Spare)     // +0x1000
        //   mvTimeToReachTargetDriftSlipRecip_...  all four = 0       // +0x1010
        //   mvDesiredDriftAngleScale_...           all four = 0       // +0x1020
        //   mvLatDriftForceFactor_...  .x = 1.0f, .y/.z/.w = 0        // +0x1030  (vspltisw 1 + vcfsx)
        //   mvSideForceMag_...  .x/.y/.w = 0 (.z TimeSinceLastBoostKick untouched)  // +0x1040
        //   mvPropSpeedMaintainAlongZ_...  .x = -0.1f (unk_8208FAE4), .y = 1.0f
        //                                  (unk_8208FAE8), .z = 100.0f (flt_820049E0)   // +0x1050
        //   mvTimeStandingStill_...   all four = 0                    // +0x1060
        //   mvDampRollVel_...  .x = 0, .z = 0, .w = 10000.0f          // +0x1080  ⚠️ SEE BELOW
        //   mSteeringDirection                                = 0     // +0x10E0
        //   mfTimeUntilStuckInCollisionTest                   = 5.0f  // +0x10F0 (flt_8200426C ==
        //                                                             //   KF_STUCK_IN_COLLISION_TEST_INTERVAL)
        //   mDriftFlags = 0xFF (DO_ALL) ; mbInBoostKick = false       // +0x10F4 / +0x10F5
        //   mbForceFrozen = false ; mbGivenAftertouchAirBoost = false // +0x10F6 / +0x10F8
        //   mSlamEffect: mfSteering = mfOriginalSteering = mfSlamLife = mfTotalSlamTime = 0,
        //                mi8SlamNumber = -1                           // +0x1114/18/1C/20 and +0x1128
        //                (mForce, mfDecay and mfRecoveryTime are NOT written -- so this is an
        //                 inlined PARTIAL clear, not SlamEffect::Clear())
        //   mShuntEffect: mDirectionPlusDesiredSpeed = 0,
        //                 mv4_Life_SpeedIncreaseToQuit.x = -1.0f, .y = 0   // +0x1130 / +0x1140
        //   mi8LastContactedRaceCar = -1                              // +0x1150
        //   mUsedAirRams = 0 ; mUsedSpins = 0                         // +0x1158 / +0x1220 (std)
        //   maSprings[0..3].Reset()                                   // +0xE10 stride 0x30, x4
        //   mPreviousWorldSpaceVelocity = lvVelocity                  // +0x1330 (the argument)
        //   mNormLinearVelocityMag = 0                                // +0x1340
        //   mbHasAir = mbHadAirLastFrame = false ; mu8DriftState = 0 ; miNumCollisions = 0 ;
        //   mbHandBrake = false ; mbAllWheelsHaveTraction = false ; mbResetCarTransform = TRUE ;
        //   mbJustBeenSlammed = false ; mbDoingBurnout = false        // +0x1350..+0x1361
        //   mPreviousTransform = mTransform                           // +0x1370 <- this+0x10, 4 rows
        //   mWheelFFSpring.mfSpringCoefficient = mfSpringSaturation = 0   // +0x13D0 / +0x13D4
        //   mi8LastAttackersRaceCarIndex = -1                         // +0x13E0
        //
        // ⚠️⚠️ THE SILENT-ZERO CONSTANT IN THAT LIST. `TimeSinceLastHandBrake` (+0x1080 .w) is
        //   seeded from `unk_82FB9080`, which reads **all-zero in the shipped image** (.data,
        //   perm=6). It is NOT zero: an IDA-UNMARKED static-init thunk at 0x82C5C398 does
        //       lis r11, flt_82005D9C@ha ; lfs f0 ; stfs ; lvlx ; vspltw v0,v0,0 ;
        //       stvx128 v0, r0, unk_82FB9080
        //   and flt_82005D9C (.rdata perm=4) == **10000.0f**. So the reset seeds "the handbrake
        //   was last used 10000 seconds ago" -- i.e. never. Left at the image's 0.0f it would read
        //   as "the handbrake was released THIS INSTANT" on every reset, which is what the two
        //   UpdateHandBrake reads of the same slot gate on. (Same shape as the +0x1050 .z seed
        //   TimeSinceLastRaceCarContact = 100.0f: "long ago".)
        //
        // ---- Construct() @0x8262DBD0, 97 instrs + 1 pad -- BODIED, see VehiclePhysics.cpp -------
        //   for (i = 0..3) { maWheels[i].Clear();                        // this+0x130 stride 0xE0
        //                    maSprings[i].Prepare(0, 0, 0); }            // this+0xE10 stride 0x30
        //   mvSpringMassScalers = 0;                                     // +0xED0
        //   mEngine.Construct();                                         // +0xF00 -- the X360 INLINES
        //     Engine::Construct here (EngineAttribs::Construct(+0xF00) then Engine::Reset(+0xF00, 0)),
        //     which is that function instruction-for-instruction @0x825F3EE8. The PS3 build of the
        //     same source calls `Engine::Construct` OUT OF LINE, which settles the identification.
        //   mpAttribs = NULL;                                            // +0x720 (stw, 4-byte)
        //   mAIVehicleAttribs.Construct();  mPlayerVehicleAttribs.Construct();   // +0x730 / +0xAA0
        //   mUsedAirRams = 0 ; mUsedSpins = 0                            // +0x1158 / +0x1220 (std)
        //   mHandlingBodyOffset = 0 ; mHalfExtent = 0                    // +0x690 / +0x6A0 (base's)
        //   mPreviousWorldSpaceVelocity = 0                              // +0x1330
        //   mAboveGroundTestResult: mIntersectionPosition = 0, mIntersectionNormal = 0,
        //         mfVerticalDistance = 0, mCollisionTag.muValue = 0xFFFF8000, mbValid = false
        //                                                                // +0x570/580/590/594+596/598
        //   mvSideForceMag_..._TimeSinceLastBoostKick_....z          = 0 // +0x1040 LANE Z ONLY
        //   mvTimeSinceHardLanding_..._CarCarResponse_....z          = 0 // +0x1070 LANE Z ONLY
        //   mvSpeedOnLastCrashMPH_TimeCrashing_...                   = 0 // +0xEF0 (whole register)
        //   SimpleVehiclePhysics::Reset();                               // 0-arg base @0x825D9A58
        //   mbFrozen = false;                                            // +0x70   <- SEE CORRECTION
        //   Reset(Vector3(0,0,0));                                       // the Vector3 overload
        //
        //   ⚠️⚠️ TWO CORRECTIONS TO THE DECODE THAT USED TO SIT HERE. It ended in an elided line,
        //   `"... zero +0x690/+0x6A0/+0x1330/+0x570 ; mSlamEffect partial seed at +0x590+0x20 ..."`,
        //   followed by `mbCrashing(+0x70) = false;`. BOTH are wrong, and both were caught by
        //   re-pulling the asm rather than transcribing the note:
        //     1. There is NO mSlamEffect store in this function. +0x590/+0x594/+0x596/+0x598 are the
        //        TAIL OF THE +0x570 BLOCK -- mAboveGroundTestResult's mfVerticalDistance, the two
        //        CollisionTag halfwords and mbValid -- exactly as this header's own +0x570 map (and
        //        SimpleVehiclePhysics::SetAboveGroundTestResult @0x826029D4) already say. mSlamEffect
        //        is at +0x1100.
        //     2. `stb r30, 0x70(r31)` is **mbFrozen**, NOT mbCrashing. VP frame +0x70 ==
        //        ExternallySimulatedBody frame +0x60, and ExternallySimulatedBody::Construct
        //        @0x8259CFA4 stores its zero byte at exactly `0x60(r3)`. mbCrashing is at +0x710
        //        (this file's own HandleWheelFrictionCrashing note pins it there). Independent
        //        confirmation: UpdateFreezing @0x825CFFA0/FFB8/FFC8/FFE8 reads and writes 0x70(r31)
        //        alongside `lbz r9,0x10F6(r31)` == mbForceFrozen -- a freezing routine touching the
        //        pair {+0x70, +0x10F6} is {mbFrozen, mbForceFrozen}, not the crash flag.
        //   Clearing +0x710 instead of +0x70 would have been an invented store in mounted code.
        //
        //   ⭐ `VehicleAttribs::Construct(this+0x730)` and `(this+0xAA0)` are an INDEPENDENT
        //   confirmation of the DWARF member sequence: mpAttribs @0x720, then two VehicleAttribs
        //   of 0x370 each land mAIVehicleAttribs at 0x730 and mPlayerVehicleAttribs at 0xAA0
        //   exactly, and maSprings then falls on its asm-pinned +0xE10.
        //
        //   ⭐⭐ THE LANE-Z INSERT IS PROVEN TWICE, ON TWO ISAs. X360 does
        //   `lvx128 ; vrlimi128 v0,v127,2,0 ; stvx128` (mask 2 == lane z under the 8/4/2/1 ==
        //   x/y/z/w convention). The PS3 build of the same two statements does
        //   `lvx ; vperm v1,v1,v31,<VectorPermuteConstant<0,1,6,3>> ; stvx` -- selector lanes
        //   {x, y, SECOND-operand z, w}, i.e. the zero lands in z and x/y/w survive. Two different
        //   compilers, two different instructions, the same lane. Both registers keep every other
        //   lane, so these are read-modify-writes and NOT whole-register clears.
        //
        // ---- THE BLOCKER IS GONE (2026-08-03) ---------------------------------------------------
        // The note that used to sit here said neither could be bodied because of ONE symbol,
        // `VehiclePhysics::SetWheelVelocities` @0x825FD218, "which the ledger marks BLOCKED
        // (degenerate VMX128 + un-committed helpers)". That label was INHERITED, never checked.
        // All 728 instructions have now been disassembled first-hand and **all three clauses of the
        // label are false**: it is one whole function (one prologue, one epilogue, and it ends
        // exactly where Reset begins), its callee set is `Engine::Reset` plus the assert
        // message-builder and NOTHING else, and the "degenerate VMX128" is an inlined
        // `XMVectorSinCos` over the rodata table THREE earlier waves had already decoded
        // (BrnBehaviourRoadRunner.cpp:988) plus a Rodrigues rotation and a cross product.
        // It is BODIED in VehiclePhysics.cpp as of this wave, so Reset/Construct are unblocked.
        //
        // Reset(Vector3) is BODIED in VehiclePhysics.cpp as of this wave, from the decode above.
        // ⚠️ It HIDES SimpleVehiclePhysics::Reset() (the 0-arg base overload) -- which is exactly
        // what the console does, and the body calls the base one explicitly-qualified, as the X360
        // does out of line at 0x825D9A58.
        //
        // ⭐ Construct() is BODIED TOO as of 2026-08-03. The three members it needed --
        // mAIVehicleAttribs / mPlayerVehicleAttribs (+0x730/+0xAA0) and mvSpringMassScalers
        // (+0xED0) -- are now declared below, each name and type verbatim from the DecFIGS DWARF
        // (VehiclePhysics.h:840 / :843 / :849). EVERY callee of Construct was already bodied
        // (Wheel::Clear, SuspensionSpring::Prepare, VehicleAttribs::EngineAttribs::Construct,
        // Engine::Reset, VehicleAttribs::Construct, SimpleVehiclePhysics::Reset, Reset(Vector3)),
        // so bodying it adds ZERO new link closure -- verified against the function's own
        // `xrefs_from`, not against the mnemonic text.
        // ==========================================================================================
        void Reset(Vector3 lvVelocity);

        // @0x8262DBD0: the console constructor -- clear the four wheels + springs, build the engine
        // and both embedded attribute sets, clear the above-ground test / half-extent / air-ram +
        // spin allocators, then base-Reset, unfreeze and Reset(0). Bodied in VehiclePhysics.cpp.
        // Its callers are VehicleManager::Construct, VehicleManager::PrepareData and
        // TrafficPhysics::Construct.
        void Construct();

        // ==========================================================================================
        // ⛔⛔ SIGNATURE CORRECTED 2026-08-11 (the create-drain wave). THIS DECLARATION WAS A FORK.
        //
        // What was here, since the C11_simple_traffic_attribs group:
        //     bool Prepare(const Matrix44Affine*, const StreamedDeformationSpec*, VehicleAttribs*,
        //                  const Vector3*, const f32*);
        // Its own banner said "arg shapes inferred from the forwarding call". They were wrong: five
        // parameters instead of nine, and a `StreamedDeformationSpec*` standing where the console
        // has FOUR Vector3s and an AxisAlignedBox&. That is the [[shadowing-redeclarations]] shape --
        // it mangles to a symbol no TU can ever define, and no per-TU compile gate can see it,
        // because the only caller (TrafficPhysics::PreparePhysical) lives in an UNMOUNTED TU. It
        // would have surfaced as an LNK2019 the first time anything mounted that file.
        //
        // ⭐ THE REAL SIGNATURE IS DWARF-ATTESTED, not inferred. PS3 export 0x735DEC:
        //     _ZN10BrnPhysics7Vehicle14VehiclePhysics7PrepareE
        //       N2rw4math3vpu14Matrix44AffineE NS4_7Vector3E S6_ S6_ S6_
        //       RKN12CgsGeometric14AxisAlignedBoxE PNS0_14VehicleAttribsE PKS6_ PKf
        // and it is identical, parameter for parameter, to the SimpleVehiclePhysics::Prepare
        // declaration this tree has carried correctly all along (PS3 0x734D58) -- which is the
        // strongest possible cross-check, since VehiclePhysics::Prepare's whole job is to forward
        // into it. The X360 call site inside TrafficPhysics::PreparePhysical @0x82639380 agrees
        // register for register: r4 = &transform, v1..v4 = the four vectors in declaration order,
        // r5 = the AABB, r6/r7/r8 = attribs / wheel positions / wheel radii.
        //
        // ⭐⭐ BODIED 2026-08-11 (prepare-chain wave). The 306-insn body @0x82637C80 is in
        // VehiclePhysics.cpp, store for store: VehicleAttribs::operator= into
        // mPlayerVehicleAttribs, mpAttribs, the nine-parameter forward into
        // SimpleVehiclePhysics::Prepare, the 3-arg SetAttributes @0x8262E140 (also landed this
        // wave -- the console's unnamed `sub_8262E140`), Construct + SetupAttribsForAI +
        // mAttribsKey into mAIVehicleAttribs, Reset(lLinearVelocity), then the ~40 own-block
        // seeds at +0xFD0/+0xFF0/+0x1050/+0x1060/+0x1070/+0x10D4/+0x1114..+0x1140/+0x1158/
        // +0x1220/+0x1359..+0x1362/+0x1370/+0x13B0/+0x13DC. Every one lands on a member this
        // header already names; nothing new was invented to hold a store.
        // ⭐ THE +0x1050 .w SEED IS NOW GROUND TRUTH: `unk_8208FB18` was read out of the ARTIST
        // database by the conductor's targeted IDA export (2026-08-11, same day) -- 0x3F800000
        // == exactly 1.0f, confirming the role-derived stand-in. See the .cpp.
        // ⭐⭐ AND IT WAS DERIVED TWICE. The sibling wave reached the same body independently by
        // cross-reading the PS3 build of the same source (export 0x735DEC, 424 insns) store for
        // store and lane for lane -- X360 `vrlimi128` masks 8/4/2/1 against PS3
        // `VectorPermuteConstant<4,1,2,3>/<0,5,2,3>/<0,1,6,3>/<0,1,2,7>`. The two agree on every
        // store, every callee and every lane. What the PS3 added: the eighth callee's NAME
        // (`sub_8262E140` == VehiclePhysics::SetAttributes, exported at 0x735D20), PS3 names for
        // four of the five rodata constants, and the fact that the two `std` at +0x1158/+0x1220
        // are `BitArray<N>::Prepare()` calls rather than raw zero stores. All folded into the body.
        //
        // ⚠️⚠️ ODR FORK FLAGGED, NOT FIXED (found by this correction). The PS3 mangle spells the
        // AABB parameter `RKN12CgsGeometric14AxisAlignedBoxE` == `const CgsGeometric::
        // AxisAlignedBox&`, but this class hierarchy carries its OWN
        // `BrnPhysics::Vehicle::AxisAlignedBox` (BrnSimpleVehiclePhysics.h:147, a "MINIMAL OWNING
        // SLICE" of {Vector3 mMin, Vector3 mMax}) and types mDeformableAABB / mOriginalAABB with it.
        // The two are the same 32 bytes and the same two fields; the real one
        // (CgsAxisAlignedBox.h:28) spells them Vector4 mMin/mMax and adds Set/ContainsPoint. The
        // parameter below is spelled with the IN-NAMESPACE slice so that it matches the sibling
        // SimpleVehiclePhysics::Prepare declaration this function forwards into -- a mismatch there
        // would be the [[odr-forks-link-silently]] defect, where a body written against one class
        // links cleanly against a call site using the other. Re-homing the slice onto
        // CgsGeometric::AxisAlignedBox is a geometry-group change and is recorded here rather than
        // done inside a physics wave.
        // ==========================================================================================
        bool Prepare(Matrix44Affine lTransform, Vector3 lLinearVelocity, Vector3 lAngularVelocity,
                     Vector3 lHandlingBodyOffset, Vector3 lHalfExtent,
                     const AxisAlignedBox& lrAABB, VehicleAttribs* lpAttribs,
                     const Vector3* lpaWheelPositions, const f32* lpafWheelRadii);
        // ⭐⭐ SIGNATURE CONFORMED 2026-08-09 (crash/shunt wave). The committed 1-arg const form
        // was a slice artifact off the delegation guess. The real @0x825FC748 prologue consumes
        // v1 (saved into v127 and SUBTRACTED from the shunt life lane at 0x825FC888 -- the dt
        // splat) and WRITES the control block through r4 (`stfs` to +4/+8/+0xC -- mfGas floored
        // to 0.8, mfBrake/mfHandBrake zeroed), so the pointer is non-const. The DWARF spells it
        // exactly (references\DecFIGS\...\VehiclePhysics.h:1505):
        //     void UpdateShunt(BrnPlayerDriverControls *, VecFloat);
        // BODIED 2026-08-09 in VehiclePhysics.cpp (100 insns, store-for-store); the LOUD TRAP in
        // VehiclePhysicsLinkStubs.cpp is deleted in the same commit.
        void UpdateShunt(BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep);

        // ⭐ SIGNATURE CONFORMED 2026-08-07 (orchestrator wave). The committed 2-arg form
        // `(f32, const BrnPlayerDriverControls*)` came off TrafficPhysics's PC-side delegation,
        // not the console. The real @0x82638810 prologue consumes f1=dt (r4 slot), r5=camera
        // matrix (saved r21), r6=controls (r22), r8=lbPlayerAftertouchForceAdditive (r24),
        // r9=lbShowtimeAllowed (r20), with the r7 slot (lbImpactTime) carried by the caller
        // (VehiclePhysics::Update @0x826414F8 `mr r7, r23`).
        // ⭐⭐ BODIED 2026-08-09 (crash/shunt wave) in VehiclePhysics.cpp -- the 732-insn
        // crash-state orchestrator, read line-by-line from the X360 asm; the LinkStubs trap is
        // deleted in the same commit. lbImpactTime (the r7 slot) is DEAD in the body (never
        // read -- the first r7 mention is the block-local `li r7, 0x390`); it exists so the
        // caller's register map stays 1:1.
        void UpdateCrashing(f32 lfTimeStep, const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                            const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                            bool lbPlayerAftertouchForceAdditive, bool lbShowtimeAllowed);

        // ==========================================================================================
        // @0x825D1C00 -- THE ANALYTIC REST SEAT (seat wave 2026-08-05). Bodied in VehiclePhysics.cpp.
        //
        // Copies the four rows of lrTransform into mTransform (this+0x10..0x40), then OVERWRITES the
        // translation row with the analytic at-rest seat above the supplied road point:
        //     newPos = pos + up * (maWheels[1].mSlipVariables.w                    // wheel 1 radius
        //                          - maWheels[1].mStreamedPositionPlusTwistAmount.y // wheel 1 local Y
        //                          - 0.035f)                                       // flt_8208FB0C
        //                  + zAxis * mpAttribs->mBaseAttribs.mCOMOffset.z
        // (asm 0x825D1E34..0x825D1EDC; flt_8208FB0C == 0.035f read from the image, the tyre-
        // compression allowance the suspension settles out). Console callers:
        // VehicleManager::ProcessResetEvents (gated on the reset event's mbResetTransform) and
        // RaceCarPhysics::Prepare @0x82639CB8 (the create leg -- an export-set hole, decoded from
        // image bytes: bl VehiclePhysics::Prepare @0x82637C80 then bl 0x825D1C00). The create leg is
        // reached from VehicleManager::ProcessCreateEvents @0x82616770 via the vcall at vtable
        // slot +0x30 on maRaceCarPhysics[i] (VERIFIED in the pseudocode: `(*(vtbl+48))(...)` on the
        // 0x1460-stride array at VehicleManager+0x740).
        // ==========================================================================================
        void SetTransformFromPositionOnRoad(const Matrix44Affine& lrTransform);

        // ==========================================================================================
        // [FLAG PC bring-up] SeatTransformFromCreateLegBringUp -- NOT an X360 function. Bodied in
        // VehiclePhysics.cpp. The PC stand-in for the create-event leg above (ProcessCreateEvents ->
        // vcall slot +0x30 -> RaceCarPhysics::Prepare -> VehiclePhysics::Prepare -> the seat), used
        // by RaceCarEntityModule::ResetActiveRaceCar while no VehicleManager runs on this build.
        // Every seat input is derived from the RESIDENT streamed deformation spec exactly the way
        // the console's own create leg derives it -- see the body for the per-input provenance and
        // the one INFERRED step (the effective COM offset). Returns the seated transform.
        // DELETE-WHEN VehicleManager::ProcessCreateEvents + RaceCarPhysics::Prepare land.
        // ==========================================================================================
        static Matrix44Affine SeatTransformFromCreateLegBringUp(
                const StreamedDeformationSpec* lpSpec,
                const Matrix44Affine&          lrPlacementTransform);

        // ----- ADDITIVE GROW (C04 wheels/tire group): two per-frame wheel-geometry funcs bodied in
        //       VehiclePhysics.cpp (CalculateBodyVelocityAtWheelContact, StoreLocalWheelPositions).
        //       ⭐ 2026-08-07 (wheel-cluster wave): the wheels orchestrator UpdateWheels
        //       @0x8261E4F0 is BODIED (no longer BLOCKED -- see the driving spine below).
        //       ⚠️ This note used to lump SetWheelVelocities @0x825FD218 in with it as
        //       "un-recoverable degenerate VMX128 + a dozen un-committed helpers / un-homed rodata".
        //       That was wrong on all three counts -- it is bodied below as of 2026-08-03. Treat the
        //       remaining "BLOCKED" labels in this file as UNVERIFIED CLAIMS until disassembled. ----

        // @0x825FB200: the body velocity at one wheel's contact point:
        //   v_contact = mLinearVelocity + mAngularVelocity x (r_contact - bodyPos)
        // r_contact = the wheel's road-contact position when on the ground, else its streamed
        // position (mStreamedPositionPlusTwistAmount). Stored into maWheels[leWheel].mBodyPointVelocity
        // (+0xA0 within the wheel). (mAngularVelocity is the +0x60 register, here mLocalVelocity.)
        // ⭐ SIGNATURE CONFORMED 2026-08-07 (wheel-cluster wave): DWARF VehiclePhysics.cpp:5148
        // spells (EVehicleDrivenWheel, Vector3, VecFloat). Both extra args are DEAD in the callee
        // (the only v1 mention in @0x825FB200 is a WRITE at 0x825FB390) -- the SetWheelVelocities
        // dead-parameter precedent. UpdateWheels passes the pair roll direction + dt as shipped.
        void CalculateBodyVelocityAtWheelContact(EVehicleDrivenWheel leWheel,
                                                 Vector3 lvRollDirection, VecFloat lvfTimeStep);

        // @0x825B7FC0: project the four wheels' world positions into the body's local frame
        //   local_i = transpose(orthonormal3x3(mTransform)) * (worldWheelPos_i - mTransform.Pos())
        // and store them into maLocalWheelPositions[i] (+0x530/+0x540/+0x550/+0x560). The X360 builds
        // the inverse rotation inline (vmrglw/vmrghw transpose) and FMA-cascades the negated position.
        void StoreLocalWheelPositions();

        // ----- ADDITIVE GROW (C08 airborne/water/freeze/spin group): three more bodies in
        //       VehiclePhysics.cpp.
        //       ⭐ 2026-08-07 (orchestrator wave): the old claim here that UpdateFreezing
        //       @0x825CFD20 is "BLOCKED -- heavy un-recoverable VMX damping / un-committed
        //       control+vtable surface" was UNVERIFIED and is FALSE on both counts: the body is
        //       185 insns of abs/max/dot timer bookkeeping, every member it touches was already
        //       named, and the one vcall (+0x14) is IsPlayerVehicleActuallyInShowtime -- the same
        //       slot the committed RaceCarPhysics::Update had already identified. It is DECLARED
        //       below with the driving spine and BODIED in VehiclePhysics.cpp.
        //       ⭐ 2026-08-11 (driving-path wave): UpdateInAirBehaviour (809 insns) is no longer a
        //       trap either -- BODIED in VehiclePhysics.cpp, stub deleted from
        //       VehiclePhysicsLinkStubs.cpp. -----

        // @0x825B81A8: the water "hard kill". When the representative contact's surface id maps to a
        // water surface AND the depth scalar (mfWaterDepth, +0x590) is below a threshold, zeroes the
        // linear & angular velocity and the force/impulse accumulators -- the car stops dead and sinks
        // (no buoyancy). The DWARF signature takes the frame controls + dt though the X360 body reads
        // neither (they exist so it slots into the UpdateDriving phase chain uniformly).
        void UpdateInWaterBehaviour(const BrnPlayerDriverControls* lpControls, VecFloat lvfDeltaTime);

        // @0x825FC8D8: tick the queued air-ram impulses. Walks the SET bits of mUsedAirRams (active
        // slots); for each, decays mfTimerTillFire by dt and, when it crosses 0, fires the slot's
        // mImpulse via AddLocalImpulse (at mPosition) and scales the stored impulse by (1 - mfDecay),
        // or releases the slot (clears the bit) when the remaining magnitude is spent.
        void UpdateAirRam(VecFloat lvfDeltaTime);

        // @0x825FCCF8: tick the queued spin impulses. Walks the SET bits of mUsedSpins; for each,
        // applies mForce as a world-space angular impulse (AddWorldSpaceAngularImpulse) while
        // mfTimeRemaining > a small epsilon, and releases the slot when it expires.
        void UpdateSpinEffects(VecFloat lvfDeltaTime);

        // ----- Vehicle-physics group (class TU): three VMX128 funcs lowered to faithful scalar
        //       (bodies in VehiclePhysics.cpp) -----

        // @0x825C0100: a "ground distance" check used by UpdateInAirStats. Normally 0.5; but when
        // the car is inverted (the vehicle up axis points down, mUpAxis.y < 0 -- the asm splats the
        // up-axis .y lane and tests `0 > up.y` via vcmpgtfp.) it returns
        // `mfCarGroundCheckExtent * KF_CAR_GROUND_DISTANCE_INVERTED_SCALE + 0.5`, accounting for the
        // car's own vertical extent when it is upside down. Spelled f64 to match the X360 ABI (the
        // value comes back in f1 as a double; the source return type is float32_t).
        f64 GetCarGroundDistanceCheck() const;

        // ==========================================================================================
        // ⭐⭐ THE DRIVING SPINE (orchestrator wave, 2026-08-07). The per-car frame chain that
        // VehiclePhysics::Update @0x826412C0 conducts and UpdateDriving @0x82638148 orders.
        // Bodies in VehiclePhysics.cpp unless marked TRAP (loud stubs in
        // VehiclePhysicsLinkStubs.cpp -- delete the stub in the same commit as any body).
        // Signatures are the X360 call-site register maps, cross-checked against the PS3 DecFIGS
        // mangled names where an out-of-line copy exists.
        // ==========================================================================================

        // @0x825CFD20 (185): the freeze latch. Tracks time-standing-still (+0x1060.x, gated on
        // linVel^2+angVel^2 vs splat(0.5) == unk_82FB8460 <- flt_82001DA0) and
        // time-still-and-not-spinning (+0x1060.y, additionally gated on max |wheel spin| --
        // maWheels[i].mIntegrationVariables.x -- with a gas+brake/handbrake override window),
        // freezes at .y > 1.0s or on the start line, never while boosting or actually in
        // showtime, always when mbForceFrozen; zeroes both velocity registers when frozen.
        void UpdateFreezing(const BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep);

        // @0x8262E848 (77): fold the debug gas-boost (flt_82FB7E24, ships 0.0 -- written only by
        // VehicleManagerDebugComponent::OnActivate) into clamp01(mfGas), force full throttle when
        // boosting without handbrake, then hand the five control scalars + forward speed to
        // ApplyEngineForces. The forward speed is dot3(mTransform.zAxis, mLinearVelocity).
        void UpdateEngine(const BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep);

        // @0x8261FC10 (178): the engine-force applier (BODIED). IsCounterSteeringAtLowSpeed traction
        // scaling of the drive, gas<->brake swap in reverse gear, then Engine::Update (the powertrain
        // torque core -- STILL A LOUD TRAP, its own wave) and, when not frozen,
        // ApplyEngineForcesOntoWheels. Register map recovered at the UpdateEngine call site: f1..f5 =
        // gas/brake/steering/fwdSpeed/mfBoostMaxSpeedScale, r6 = mbHandBrake, v1 = dt.
        void ApplyEngineForces(f32 lfGas, f32 lfBrake, f32 lfSteering, f32 lfForwardSpeed,
                               f32 lfBoostMaxSpeedScale, bool lbHandBrake, VecFloat lvfTimeStep);

        // @0x825FB000 (128): distribute the engine drive force onto the wheels' angular-velocity
        // integration accumulators (BODIED). Scales mEngine.GetEngineDrive() by the counter-steer
        // traction factor, zeroes it above the (boost-aware) max speed, and either adds it into all
        // four maWheels[i].mIntegrationVariables.z split by PowerToFront/PowerToRear, or -- under
        // handbrake above 5 mph -- locks the drive into the rear pair only. Register map from the
        // ApplyEngineForces call site: f1 = drive scale, f2 = forward speed, f3 = boost max-speed
        // scale, r5 = handbrake.
        void ApplyEngineForcesOntoWheels(f32 lfDriveScale, f32 lfForwardSpeed,
                                         f32 lfBoostMaxSpeedScale, bool lbHandBrake);

        // @0x825D0A50 (102): shift mbHasAir into mbHadAirLastFrame, then re-derive mbHasAir: no
        // wheel has traction, the (in-water && above-ground-valid) depth test passes
        // (mfWaterDepth > GetCarGroundDistanceCheck()), and |mNormLinearVelocityMag.w| >
        // splat(1.8) == unk_82FB8390 <- flt_82013A80. Ticks the air timers (+0x1060.z up in air /
        // .w up on ground, each zeroing the other).
        void UpdateInAirStats(f32 lfTimeStep);

        // @0x825F6338 (195): the aero pass. Airborne: an attribs-shaped local stabilising force
        // via AddLocalForce (asserts MaxSpeed != 0: "Zero max speed in attribsys data...").
        // Grounded: AddWorldSpaceForce of a purely vertical -GetDownForce() scaled by
        // 20.0*(|zAxis.y| + |dot3(unitVel, xAxis)|) while moving (the slip/pitch relief), and
        // mirrors the magnitude into the handling debug component when attached.
        void UpdateDownForce();

        // @0x82638148 (433): ⭐⭐ THE ORDERER. The exact phase chain is transcribed in the body;
        // every stage is bracketed by ExternalPhysicsBody::CheckState with the console's own
        // stage strings ("Before driving update" ... "End of driving update").
        void UpdateDriving(const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                           const BrnPlayerDriverControls* lpControls,
                           CgsNumeric::Random& lrRandom, VecFloat lvfTimeStep);

        // @0x8261E4F0 (1130): ⭐⭐ THE WHEEL ORCHESTRATOR -- BODIED 2026-08-07 (wheel-cluster
        // wave) in VehiclePhysics.cpp, with its four exclusive helper callees below. The full
        // stage list is transcribed at the body.
        void UpdateWheels(const BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep);

        // ----- The four helpers only UpdateWheels calls (X360 xref sets are exactly
        //       {UpdateWheels}), bodied with it 2026-08-07. DWARF signatures. -----

        // @0x825D05F0 (146): the burnout latch. Both pedals floored (> 0.97), both rear wheels
        // spinning forward, rear-pair average spin at least 1.0 rad/s over the front pair, and
        // the car (near) stationary (|v|^2 < 0.25) or already doing a burnout: scale the rear
        // spin up 3%/frame while the fronts are slow (< 70 rad/s) and latch mbDoingBurnout.
        void UpdateBurnout(const BrnPlayerDriverControls* lpControls);

        // @0x825F6648 (205): re-seed each wheel's spin inertia lanes
        // (mSuspensionAndInertiaVariables.z = 30.0, .w = 1/30 -- 200.0/(1/200) during a
        // burnout), then apply the handbrake lock: below 5 m/s the DRIVEN axle locks
        // (rear when PowerToRear != 0), above it the rear locks in forward gear and the
        // front locks in reverse (mIntegrationVariables.x = 0 + both inertia lanes zeroed;
        // Wheel::UpdateVelocity's mu8State lock path finishes the job).
        void UpdateWheelInertia();

        // @0x825D0238 (236): maintain the running brake amount (the +0x1010 .w BrakeScale lane)
        // from the handbrake/pedal state and return the braking factor -- the BrakeScaleToFactor
        // curve (attribs +0x60, InterpedParam3::GetInterped) evaluated at the ramped pedal.
        VecFloat UpdateBrakesAndGetBrakingFactor(const BrnPlayerDriverControls* lpControls,
                                                 VecFloat lvfTimeStep);

        // @0x825D0940 (67): the open-differential coupling -- clamp both driven wheels' spin
        // (mIntegrationVariables.x) into a +/-10% band around their average.
        void LimitDifferential(EVehicleDrivenWheel leWheelA, EVehicleDrivenWheel leWheelB);

        // @0x825D0BE8 (809): ⭐ BODIED 2026-08-11 (driving-path wave) in VehiclePhysics.cpp -- the
        // ACTIVE airborne attitude controller (the reason jumps feel good): a one-shot take-off
        // damp whose roll strength scales with how rolled the car already was, a per-frame landing
        // assist driven by steering-against-the-roll, and a restoring torque that pitches/rolls the
        // body towards its own velocity vector (or bleeds that component out of mAngularVelocity,
        // mTotalTorque AND mTotalAngularImpulse when diving). Early-returns on !mbHasAir.
        void UpdateInAirBehaviour(const BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep);

        // @0x8262DE58 (185): re-derive the attribs-dependent state after a reset: base 0-arg
        // SetAttributes, the wheel position/radius capture against mpAttribs, the AttribSys
        // handling re-stream (VehicleAttribs::SetupAttribs -- TRAP until its wave), the COM
        // nudge, the 2-arg chain, the engine re-prepare and SetupSuspension. ⭐ BODIED
        // 2026-08-09 in VehiclePhysics.cpp. ⚠️ Returns bool (DWARF VehiclePhysics.h:1072) --
        // the committed `void` was a slice artifact; the console returns a literal true.
        bool SetAttributes();

        // ⭐⭐ @0x8262E140 (48 insns) -- ADDED AND BODIED 2026-08-11 (prepare-chain wave). The
        // THREE-ARG SetAttributes: DWARF-attested (references/DecFIGS/.../VehiclePhysics.h:1075
        // `bool SetAttributes(VehicleAttribs *, const rw::math::vpu::Vector3 *, const float32_t *)`)
        // and the ONLY consumer of the console's `sub_8262E140` -- an export the IDA database leaves
        // UNNAMED, whose identity is settled by three independent facts rather than by its role:
        //   * its ONLY caller is VehiclePhysics::Prepare @0x82637CC8 (`bl` at 0x82637DC8), which is
        //     exactly where the DWARF's Prepare calls SetAttributes;
        //   * its second assert is baked with __FILE__ == VehiclePhysics.cpp and __LINE__ == 410,
        //     i.e. it IS a VehiclePhysics.cpp function, not a SimpleVehiclePhysics one;
        //   * its callee set (SimpleVehicleAttribs::SetupAttribs, SimpleVehiclePhysics::
        //     SetAttributes, Engine::Prepare, VehiclePhysics::SetupSuspension) is the 0-arg
        //     SetAttributes' tail with the attribs pointer supplied instead of chased.
        // ⭐⭐ AND THE IDENTIFICATION IS NOW BY SYMBOL, NOT BY ELIMINATION (2026-08-11 merge). The
        // PS3 build EXPORTS IT NAMED at 0x735D20 -- `_ZN10BrnPhysics7Vehicle14VehiclePhysics13Set`
        // `AttributesEPNS0_14VehicleAttribsEPKN2rw4math3vpu7Vector3EPKf` -- and the PS3 body of
        // VehiclePhysics::Prepare @0x735DEC calls it by that name with `this + 2704 ==
        // &mPlayerVehicleAttribs`, exactly the `addi r4,r31,0xAA0` the X360 passes. The three
        // circumstantial facts above are corroboration now.
        // Its first three statements are the console-INLINED SimpleVehiclePhysics::SetAttributes(
        // VehicleAttribs*, const Vector3*, const f32*). ⭐ THAT OVERLOAD IS NO LONGER DECLARE-ONLY:
        // it is bodied in BrnSimpleVehiclePhysics.cpp (recovered from this inline plus the matching
        // one in SimpleVehiclePhysics::Prepare, PS3-attested at 0x734B10), so the body below CALLS
        // it instead of spelling the block flat -- the inlining-reversal this project's rules ask
        // for, and it closes a declared-but-undefined symbol no per-TU gate could see.
        // ⚠️ The 2-arg base overload really is called TWICE (0x8262E1A0 inside the inlined 3-arg,
        // then 0x8262E1D8 after mpAttribs is re-seated) -- the same doubled-call shape
        // SimpleVehiclePhysics::Prepare already carries for SetupAttribs. Not a transcription slip.
        bool SetAttributes(VehicleAttribs* lpAttribs, const Vector3* lpaWheelPositions,
                           const f32* lpafWheelRadii);

        // @0x825D0008 (139): the debug reset/fly-around handler (gated on mbReset). ⭐ BODIED
        // 2026-08-09 in VehiclePhysics.cpp.
        void HackedResetAndFlyAround(const BrnPlayerDriverControls* lpControls,
                                     VecFloat lvfTimeStep);

        // @0x8261E498 (21): switch the active attribute set: SimpleVehiclePhysics::SwitchAttribs,
        // seat mpAttribs, copy the EngineAttribs block (attribs+0x190, 0xA0 bytes) into the
        // engine (+0xF00), re-derive the suspension. Bodied in VehiclePhysics.cpp.
        void SwitchAttribs(VehicleAttribs* lpAttribs);

        // @0x8261FED8 (29): enter/leave the donut-AI attribute regime. Entering: rebuild the AI
        // set for donutting and latch mbIsUsingAIDonutAttribs. Leaving: rebuild the plain AI set
        // and run the full SwitchAttribs sequence on it. Bodied in VehiclePhysics.cpp.
        void SwitchAIDonuttingAttribs(bool lbDonutting);

        // ⭐ VIRTUAL, IMAGE-ATTESTED (orchestrator wave, 2026-08-07). The DWARF declares this
        // virtual (VehiclePhysics.h:1195) and the vtable is now READ OFF THE IMAGE, not
        // reasoned about: the VehiclePhysics/TrafficPhysics vtable @0x820D0C68 carries
        // `li r3,0 ; blr` (an ICF-folded return-false, aliased in the IDB as
        // PVSDebugComponent::IsSimple) at slot +0x14, and the RaceCarPhysics vtable
        // @0x820D1034 carries @0x827E42B0 (`lbz r3,0x140C(r3)` == mbPlayerCarInShowtime) in
        // the same slot. UpdateFreezing dispatches through +0x14, so the distinction is live:
        // traffic never reads showtime state, race cars do. RaceCarPhysics.h's existing
        // declaration is the override.
        virtual bool IsPlayerVehicleActuallyInShowtime() const { return false; }

        // ----- ADDITIVE GROW (aero/downforce group) -----
        // @0x825D0840: the aerodynamic down/drag force magnitude, the textbook quadratic
        //   F = 0.5 * rho * CdA * |mLinearVelocity|^2 * coeff
        // where rho (air density) and CdA (drag-area) are process-wide constants lazily cached
        // into g_vAero_Rho / g_vAero_CdA from un-homed .rdata seeds (kAero_Rho_Scalar /
        // kAero_CdA_Scalar), and coeff is the .w lane of mpAttribs->mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo (@+0xB0). The
        // X360 broadcasts the scalar across a VMX register; here a flat Vector3 with the magnitude
        // in every lane. There is NO speed-curve table -- downforce grows purely with v^2.
        // FLAG (rodata): the rho/CdA seed scalars are un-homed .rdata not present in the function
        // exports; carried as honest flagged-0 placeholders (faithful-but-inert) per project rule
        // -- the formula + offsets are exact, the numeric output stays 0 until the seeds are
        // recovered from the XEX .rdata. NEVER fabricated.
        Vector3 GetDownForce() const;

        // ----- ADDITIVE GROW (surface-response group): the per-surface grip/drag/roughness
        //       lookups. Each reads a 6-bit surface id from a RoadContact CollisionTag
        //       (luSurfaceId = (mCollisionTag.muValue >> 4) & 0x3F -- the X360 reads the HALFWORD at
        //       tag+2, i.e. the low 16 bits of the big-endian muValue, then >>4 &0x3F; matches
        //       UpdateInWaterBehaviour's identical extraction for the same CollisionTag type),
        //       indexes a global per-surface property table, and blends with a lane
        //       of mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.
        //       FLAG (runtime data): the per-surface tables (grip unk_82FB8890, drag unk_82FB8BD0,
        //       roughness unk_82FB8DE0, the global roughness scale unk_82FB9220 and the optional
        //       wet/condition multiplier unk_82FB9EC0) are RUNTIME-LOADED scratch globals, not
        //       .rdata in the function exports -- carried as honest flagged-0 placeholders
        //       (faithful-but-inert): the surface-id extraction, the lerp/scale math and the
        //       attrib lanes are EXACT; the looked-up property stays 0 until the tables are
        //       recovered. NEVER fabricated. The debug "properties loaded" / surface-id-bound
        //       asserts are elided (debug-build guards, no effect on output).

        // @0x825D51B8: per-wheel surface grip multiplier. result = 1 - (1 - gripTable[id]) * blend,
        //   blend = mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor lane .x (FRONT, leWheel<2) or .y (REAR); optional global wet
        //   multiplier when enabled. A lerp toward 1.0 by (1 - blend).
        Vector3 GetSurfaceGrip(EVehicleDrivenWheel leWheel) const;

        // @0x825D5328: per-wheel surface roughness = roughTable[id] * globalRoughScale *
        //   mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor lane .z. Feeds UpdateRoadNoise.
        Vector3 GetSurfaceRoughness(EVehicleDrivenWheel leWheel) const;

        // @0x825D50A8: vehicle linear-drag from a single representative contact (NOT per-wheel) =
        //   dragTable[id] * mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor lane .w. Reads the representative-contact tag below.
        Vector3 GetSurfaceLinearDrag() const;

        // ----- ADDITIVE GROW (surface-grip/drag/friction group, C05): the road-noise rumble and the
        //       two per-wheel tyre-friction solvers. Bodies in VehiclePhysics.cpp. -----

        // @0x825F6980: surface roughness -> stochastic rumble accumulated into each grounded wheel.
        // Per frame it draws floats from the shared Random ring (the inlined Random::AddRandomFloatToBuffer
        // LCG, full 64-bit multiplier 0x5851F42D4C957F2D, +1, 8-entry ring) -- one pre-loop draw shared
        // across wheels plus one draw per grounded wheel -- combines them into a small [0,1) noise term,
        // scales by GetSurfaceRoughness(wheel), the per-vehicle road-noise factor (mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce
        // .z) and the body-frame speed (mfSpeedMPH @+0x6C0), clamps to 1.0 and accumulates into the
        // wheel's road-noise register. Takes the shared Random by reference (DWARF: UpdateRoadNoise(
        // VecFloat, Random&); the leading VecFloat dt is unused by the rumble math and elided here).
        void UpdateRoadNoise(CgsNumeric::Random& lrRandom);

        // @0x825FB458: the per-axle tyre-friction workhorse -- resolves BOTH wheels of an axle at once
        // in 2-wide SIMD (left lane0 / right lane1). Builds longitudinal & lateral unit directions
        // (Gram-Schmidt), projects contact-relative velocity to slip, multiplies by the tyre grip-curve
        // coefficient and the surface-grip limit, resolves the combined long+lat force inside a friction
        // cone (min adhesiveLimit + reciprocal renormalise), then applies each force component as
        // r x F TORQUE via AddWorldSpaceTorque and accumulates the linear residual into
        // mTotalLinearForce (+0xF0) scaled by mvfWheelFrictionLinearMultiplier (+0xFD0). Wheel-spin
        // reaction feeds back via Wheel::ApplyFrictionReaction; wheels without traction are masked out.
        // ⭐⭐⭐ BODIED 2026-08-12 (tyre-force wave). The old "FLAG (blocked): degenerate VMX128, the
        // per-lane semantics are not recoverable" is RETIRED: the pseudocode is degenerate but the
        // 1141-instruction disassembly is not, and it was read end to end and cross-mapped against the
        // BPR twin sub_B9BD60 (algorithm oracle ONLY -- no BPR offset is used). The lane question was
        // settled from the image (`vmrghw <A>,<B>` -> lane0 = A; the four grip-curve lanes pack as
        // {A-long, A-lat, B-long, B-lat}; `vpermwi128 0x27/0x72` un-packs) and then made moot by
        // writing the solver per wheel. See the VehiclePhysics.cpp banner for the full derivation.
        // ⛔ ONE thing is still gated, and only one: mSlipVariables.z (the reported skid factor) needs
        // three .bss constants no other exported function references. It feeds no force.
        // ⚠️ CORRECTIONS from the asm: the linear residual lands in mTotalLinearForce (+0xF0), NOT the
        // "+0x240" this note used to claim; the drift/normal grip-curve selector at +0x1352 is
        // mu8DriftState; and the +0x85 "linear-force cap" (unk_82FBA1E0 <- flt_82013A78) is only
        // INITIALISED here -- this function never reads it, so no such cap is applied.
        // ⭐ SIGNATURE CONFORMED 2026-08-07 (wheel-cluster wave): the old 2-arg form was a slice
        // artifact. DWARF VehiclePhysics.h (decl :5377 block) spells the 9-arg form, and UpdateWheels'
        // register map @0x8261F2CC-0x8261F2F4 names every slot: r4/r5 = the two wheel indices,
        // v1 = the pair's roll direction (mTransform.At() for the rear pair, mSteeringDirection for
        // the front pair), v2 = the downforce-derived grip scale, v3 = dt, v4/v5 = GetSurfaceGrip of
        // wheelA/wheelB, r6 = "3+ wheels have traction", r7 = false (constant at every call site).
        void HandleWheelPairFriction(EVehicleDrivenWheel leWheelA, EVehicleDrivenWheel leWheelB,
                                     Vector3 lvRollDirection, VecFloat lvfDownForce,
                                     VecFloat lvfTimeStep, VecFloat lvfSurfaceGripA,
                                     VecFloat lvfSurfaceGripB, bool lbMostWheelsHaveTraction,
                                     bool lbUnusedFalse);

        // @0x825D41A8: single-wheel scrub path used ONLY while crashing (asserts IsCrashing() @+0x710).
        // A wheel with no traction, or in the burnout inertia state, has its spin decayed by 0.95 and
        // nothing else; otherwise a slip-driven scrub force is applied as r x F torque via
        // AddWorldSpaceTorque and accumulated into mTotalLinearForce (+0xF0).
        // ⭐ 2026-08-12 (tyre-force wave): the INACTIVE decay branch is now BODIED (and its 0.95 is
        // sourced -- `lfs flt_82004FDC`, the same .rdata scalar Engine.cpp homes, not the "inline
        // immediate" the old note claimed). ⛔ The ACTIVE scrub stays GATED, but the reason narrowed:
        // its eight tunables' .rdata seeds are NAMED and six of eight are already homed in this tree
        // (30.0 / 3.0 / 0.95 / 1.0 / 20.0 / 4.0 / 1.0 / 0.2, selected by
        // mPreviousControls.meDriverType @+0x10D4 -- types 1 and 3 vs the rest). What is missing is
        // only which slot each occupies in the 0x825D460C.. cascade. Not guessed.
        // ⭐ SIGNATURE CONFORMED 2026-08-07 (wheel-cluster wave): DWARF VehiclePhysics.h spells
        // (EVehicleDrivenWheel, VecFloat) -- UpdateWheels passes v1 = dt at all four call sites.
        void HandleWheelFrictionCrashing(EVehicleDrivenWheel leWheel, VecFloat lvfTimeStep);

        // @0x825B2EF8: the transform delta from the previous frame to the current frame, expressed
        // in the previous frame's local space:
        //   result = InverseOfMatrixWithOrthonormal3x3(mPreviousTransform) * mTransform
        // The X360 builds the inverse of mPreviousTransform inline (vmrglw/vmrghw transpose of the
        // orthonormal 3x3 + the negated-position FMA cascade) and matrix-multiplies it by the
        // current mTransform, storing the four affine rows into the return buffer.
        Matrix44Affine GetTransformDelta() const;

        // @0x825C0000: recompute the cached normalized linear velocity + speed. Reads the world-space
        // linear velocity (mLinearVelocity, base +0x50), normalizes it (vmsum3fp128 |v|^2 +
        // vrsqrtefp/Newton-refined reciprocal magnitude), and stores the unit direction in the xyz
        // lanes and the speed magnitude in the "plus" (w) lane of mNormLinearVelocityMag. A zero-speed
        // input leaves the direction zeroed (the asm's vsel/vcmpeqfp-against-zero guard).
        void UpdateLinearVelocityMagnitude();

        // ----- Vehicle-physics group: two header-homed methods (bodies inline below) -----

        // @0x827E24E8: returns the showtime deformation scale. On X360 this is a leaf returning a
        // single constant (lfs from flt_82001C98 == 1.0). The richer mass-scaled variant lives in
        // RaceCarPhysics; the base VehiclePhysics simply returns 1.0. Spelled f64 to match the
        // X360 ABI (the value comes back in f1 as a double).
        f64 GetShowtimeDeformationScale() const
        {
            return 1.0;
        }

        // @0x825BFEF0: true when the car is being counter-steered while moving slowly -- used by
        // UpdateWheels / ApplyEngineForces to soften the handling at low speed. Logic recovered
        // store-for-store from the asm:
        //   * requires LOW forward speed: `vcmpgtfp. unk_82FB9FC0, v1` -- the VecFloat arg is the
        //     forward speed and the gate is a lazily-cached splat of flt_8208F9D4 == 20.0
        //     (x360rd-read 2026-08-07 -- the old "un-homed rodata" enable-flag FLAG is RETIRED,
        //     another absence banner down). fwdSpeed >= 20.0 -> false.
        //   * requires throttle: `fcmpu f2 vs flt_82004744(0.2); ble -> false` -- the second f32
        //     arg is the GAS input (the caller's `lfs f2, 4(controls)`), not a speed.
        //   * if the yaw rate (mAngularVelocity.y) is clearly positive (> 0.5) and steering is not
        //     strongly opposite (>= -0.1) -> counter-steering: true.
        //   * else if the yaw rate is NOT below -0.5 -> false.
        //   * else (yaw < -0.5) counter-steering iff steering is mild (<= 0.1).
        //
        // ⭐ SIGNATURE CONFORMED 2026-08-07 (wheel-cluster wave). The old 2-arg (steering, speed)
        // form was a slice artifact: DWARF VehiclePhysics.h:2069 spells (VecFloat, float32_t,
        // float32_t), and UpdateWheels' call site @0x8261F19C-0x8261F1C4 maps v1 = splat(dot3(
        // mLinearVelocity, At)) [the forward speed], f1 = controls->mfSteering, f2 = controls->
        // mfGas. The old body's `lfSpeed <= 0.2f` was really the GAS test; the true low-SPEED gate
        // (vs 20.0) was mis-filed as an un-homed feature enable.
        //
        // FLAG (rodata, resolved): 20.0 (flt_8208F9D4 via unk_82FB9FC0) / 0.2 (flt_82004744) /
        // -0.1 (flt_8200D530) / 0.1 (flt_82004014) / 0.5 (flt_82001DA0) / -0.5 (flt_82004C78 --
        // a DISTINCT symbol from the +0.5, confirmed by its other homes in this codebase).
        bool IsCounterSteeringAtLowSpeed(VecFloat lvfForwardSpeed, f32 lfSteering, f32 lfGas) const
        {
            static const f32 KF_LOW_SPEED_GATE = 20.0f;   // unk_82FB9FC0 <- flt_8208F9D4

            if (lvfForwardSpeed.x >= KF_LOW_SPEED_GATE)   // !(gate > fwdSpeed) -> false
                return false;
            if (lfGas <= 0.2f)                            // flt_82004744
                return false;

            // lane 1 of the +0x60 register == base+0x50 == mAngularVelocity.y == the YAW RATE
            // (asm @0x825BFF6C-0x825BFF84: `li r9,0x60 ; lvx128 v0,r3,r9 ; vspltw v0,v0,1`).
            const f32 lfLateral = mAngularVelocity.y;

            if (lfLateral > 0.5f && lfSteering >= -0.1f)
                return true;

            if (!(-0.5f > lfLateral))                 // lfLateral >= -0.5 -> not counter-steering
                return false;

            return lfSteering <= 0.1f;
        }

        // ----- ADDITIVE GROW (C06 steering/drift/handbrake group): the signature steering+drift+
        //       handbrake pipeline (bodies in VehiclePhysics.cpp). All operate on the 11-Vector4
        //       drift state bank (+0xFE0..+0x1080) + the drift byte flags grown below. The X360
        //       build is dense VMX128; these are the de-SIMD'd named-member equivalents. The heavy
        //       CgsDev::Assert/StrStream "Invalid ... during drift" machinery + the per-phase
        //       ExternalPhysicsBody::CheckState debug calls are ELIDED (debug-build guards, no
        //       effect on output). Driver-control args are the live BrnPlayerDriverControls.
        //
        //   Enum/flag vocabulary is the DWARF's (EDriftState, DriftFlags::KU_DRIFT_FLAG_*). -----

        enum EDriftState
        {
            eDriftState_None       = 0,
            eDriftState_FacingLeft = 1,
            eDriftState_FacingRight = 2,
            eNumDriftStates        = 3
        };

        // @0x825D4028 (virtual): the speed-sensitive world steering angle. When NOT drifting
        //   (mu8DriftState == 0) OR below the speed guard, returns the cached steering angle
        //   (mvSteeringAngle_..._.x). When drifting AND above the guard, recomputes the world
        //   steering direction: angle = acos(dot(normalize(mLinearVelocity), forwardAxis @+0x30)),
        //   SIGNED by dot(unitVel, rightAxis @+0x10), then speed-blended (authority shrinks with
        //   speed via mvLatDriftForceFactor_..._.w = CurrentDriftAngle lane) and clamped.
        f32 GetSteeringAngle() const;

        // @0x825D34D8: caps the wheel angle during a slide. Builds a steering direction from the
        //   stiffened steer input, takes acos against the velocity, scales to a max via
        //   mpAttribs->mvDriftParams (+0xF0) lane .x with deg->rad (0.017453292), returns the angle.
        f32 GetMaxSteeringAngleDuringDrift(f32 lfSteeringInput) const;

        // @0x825CFB70: quartic stick-stiffening: s' = -1 - sign(s)*(s^4 * 1.25). Softens centre,
        //   sharpens the extremes. Writes back lrControls.mfSteering; when on a wheel device it
        //   blends the steering direction toward the body forward (unk_82FB9370 weight).
        void ModifyControlsForSteeringWheelInput(BrnPlayerDriverControls& lrControls) const;

        // @0x825CFC68: re-maps steer to drift control while sliding (gated on mu8DriftState!=0 and
        //   the original-controls drift-override byte). Direction signed by the drift state.
        void ModifyControlsForDrift(BrnPlayerDriverControls& lrControls) const;

        // ⭐⭐ SIGNATURES CORRECTED 2026-08-03 (the "cheap prize" wave). The DecFIGS DWARF declares
        //    the WHOLE drift family with a TRAILING `VecFloat` time-step that this header had
        //    dropped from every one of them (VehiclePhysics.h:1439/1451/1457/1460/1466):
        //        UpdateDrift          (const BrnPlayerDriverControls*, VecFloat)
        //        UpdateDriftState     (const BrnPlayerDriverControls*, f32, f32, f32, VecFloat)
        //        CheckForEnteringDrift(const BrnPlayerDriverControls*, f32, f32, f32, VecFloat)
        //        ApplyDriftForces     (const BrnPlayerDriverControls*, f32, f32, f32, VecFloat)
        //        UpdateDriftScale     (const BrnPlayerDriverControls*, f32, f32, VecFloat)
        //    The X360 asm agrees: UpdateDrift keeps the incoming v1 alive across the whole body
        //    (`vmr128 v122,v1` @0x8262E23C) and hands it to UpdateDriftState/ApplyDriftForces, and
        //    UpdateDriftState is the only consumer that USES it (guard 3 integrates a timer by it).
        //    Five wrong arities, one systemic drop -- exactly the trap the tree already documents
        //    for stubs. The three f32s' ROLES are the PS3 DWARF's own local names, and each one is
        //    independently confirmed by what UpdateDrift @0x8262E200 loads into f1/f2/f3.

        // @0x8262E200: the per-frame drift entry. Computes |Steering| (+0xFE0 .y), |DriftScale|
        //   (+0x1000 .w) and the body speed in m/s from mLinearVelocity, runs the drift state machine
        //   (UpdateDriftState), and when drifting (mu8DriftState!=0) advances TimeDrifting by the
        //   time-step and applies the drift forces (ApplyDriftForces); when not drifting, eases the
        //   cached drift scale back.
        void UpdateDrift(const BrnPlayerDriverControls* lpOriginalControls, VecFloat lvfTimeStep);

        // @0x8261F728: the drift state machine. CheckForEnteringDrift then a long battery of
        //   ExitDrift guards (handbrake-on time, handbrake-off time, static-friction dwell, exit
        //   timers, the attribs speed limit, off-ground time, above-ground validity, speed too low,
        //   steering crossed centre).
        void UpdateDriftState(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                              f32 lfAbsDriftScale, f32 lfSpeedMPS, VecFloat lvfTimeStep);

        // @0x825FA748: grows mDriftScale toward the target slip and applies the natural self-aligning
        //   drift yaw (ApplyNaturalDriftForces). Reads the controls' aftertouch/forced-drift state.
        void UpdateDriftScale(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                              f32 lfAbsDriftScale, VecFloat lvfTimeStep);

        // @0x825FA268: latches mu8DriftState from the sign of the entry steering input
        //   (mfSteering <= 0 -> FacingRight(2), else FacingLeft(1)), seeds the drift timers/scale and
        //   the StartSlip lane, and resets mDriftFlags.mu8DriftFlags = KU_DRIFT_FLAG_DO_ALL.
        void EnterDrift(const BrnPlayerDriverControls* lpControls, f32 lfSlip, f32 lfSpeed);

        // @0x825B8220: tears down the drift -- clears mu8DriftState, zeroes the drift-bank timer
        //   lanes (NeutralControlTime / TimeDrifting / TimeInFrictionState / ...), and resets
        //   mDriftFlags.mu8DriftFlags = KU_DRIFT_FLAG_DO_ALL (and the slam marker mDriftFlags = -1).
        void ExitDrift();

        // @0x8261FAB0: dispatches the four drift sub-forces in order: MaintainDriftSpeed,
        //   UpdateDriftScale, ApplyDriftYaw, then ApplyDriftLatForce (the last gated by
        //   mbAllWheelsHaveTraction && mAboveGroundTestResult.mbValid && !mbHandBrake).
        //   [V] 0x8261FB28-3C: it forwards f1/f2 and the VecFloat v1 UNCHANGED into UpdateDriftScale
        //   (`vmr128 v1,v127 ; fmr f2,f30 ; fmr f1,f31`), which is what pins UpdateDriftScale's own
        //   two f32s to lfAbsSteering / lfAbsDriftScale as well.
        void ApplyDriftForces(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                              f32 lfAbsDriftScale, f32 lfSpeedMPS, VecFloat lvfTimeStep);

        // @0x825D2B20: the sideways world-space force that steps the rear out. Tangent-projects the
        //   lateral force against the ground normal (+0x580) so drift never pushes into/off the road.
        void ApplyDriftLatForce(f32 lfSlipAngle, f32 lfSpeed, f32 lfSteeringDir, f32 lfTimeStep);

        // @0x825D25A0: a world-space yaw torque rotating the car toward the drift direction (gated on
        //   the computed local drift angle AND mDriftFlags & KU_DRIFT_FLAG_APPLY_TORQUE).
        void ApplyDriftYaw(const BrnPlayerDriverControls* lpControls, f32 lfSlipAngle, f32 lfSpeed);

        // @0x825D2F78: a gentle straightening yaw when NOT actively drifting (self-aligning torque
        //   signed by mu8DriftState), gated on the per-car drift push-time attrib.
        void ApplyNaturalDriftForces();

        // @0x825D2270: keeps a sliding car from scrubbing off speed -- when MaintainedSpeed (the
        //   +0x1000 .y lane) exceeds current speed AND throttle >= 0.3 AND grounded, adds a ground-
        //   tangent world impulse along a Z/velocity blend (mvPropSpeedMaintainAlong* @+0x1050).
        void MaintainDriftSpeed(const BrnPlayerDriverControls* lpControls, f32 lfTimeStep);

        // @0x825CFA10: the handbrake latch+timer with hysteresis around input 0.1. Engages above
        //   0.1; releases below 0.1 only once a drift is active OR the on-time threshold passes. The
        //   two timers (TimeHandbrakeHasBeenOn / TimeSinceLastHandBrake) live in the +0x1080 lane.
        void UpdateHandBrake(f32 lfHandBrakeInput, f32 lfTimeStep);

        // ----- ADDITIVE GROW (C07 boost/speed-match group): the boost regime classifier + its two
        //       force appliers + the AI speed-match assist (bodies in VehiclePhysics.cpp). All boost
        //       force = Mass * acceleration along the body forward axis, applied via the base
        //       ExternalPhysicsBody::AddLocalForce (declared BY NAME below, owned by the base TU). The
        //       boost-state lanes live in mvBoost_..._CurrentBoostKickTime (+0x1040) + mbInBoostKick
        //       (+0x10F5), grown below. The heavy CgsDev::Assert mutual-exclusion machinery in the two
        //       appliers is ELIDED (debug-build guards, no effect on output). -----

        // @0x825FACE8: the per-frame boost driver. Gates on the boost button (lrControls.mbBoostBounce
        //   @+0x3B) and the 5.0-mph floor; when boost is live and below the throttle-scaled MaxBoostSpeed
        //   cap, classifies the regime -- a fresh boost (TimeBoosting==0) past the 2.0s kick cooldown and
        //   below BoostKickMaxStartSpeed seeds the kick window (CurrentBoostKickTime = clamp(BoostKickMaxTime
        //   * (1 - speed/BoostKickMaxStartSpeed)^2, BoostKickMinTime, BoostKickMaxTime)); then sets
        //   mbInBoostKick = (CurrentBoostKickTime > TimeBoosting) and dispatches Apply{Boost,Normal}*Force.
        //   The boost-state timers (+0x1040) are advanced every path (TimeBoosting += dt while boosting,
        //   else reset; TimeSinceLastBoostKick accumulates the cooldown).
        void UpdateBoost(const BrnPlayerDriverControls* lpControls, f32 lfTimeStep);

        // @0x825D30C8: sustained boost. F = Mass * NormalBoostAcceleration along the body forward axis,
        //   applied at the NormalBoostHeightOffset local position. Gated on mbAllWheelsHaveTraction
        //   (+0x135B). Advances TimeSinceLastBoostKick by dt and zeroes CurrentBoostKickTime.
        void ApplyNormalBoostForce(f32 lfTimeStep);

        // @0x825D3228: the boost KICK -- a larger forward impulse applied at the off-centre
        //   BoostKickHeightOffset to generate a wheelie (pitch-up torque). It then SELF-LIMITS: once the
        //   pitch exceeds sin(kfMaxWheelieAngle deg->rad), it damps the roll/pitch components of the
        //   velocity/impulse rows (+0x60/+0x100/+0x120) by kfWheelieLimitDamping. Resets the kick
        //   cooldown lane (TimeSinceLastBoostKick).
        void ApplyBoostKickForce(f32 lfTimeStep);

        // @0x825D4AD8: the AI rubber-band speed-match assist. Gated on mbAllWheelsHaveTraction (+0x135B)
        //   + the controls' speed-match mode/target. Computes (targetSpeed - currentForwardSpeed), clamps
        //   the delta to +/-(clampVec * dt), then distributes a bounded corrective force into the four
        //   wheel integration accumulators (front pair scaled by 1/maWheels[0].mSlipVariables.w, rear pair
        //   by 1/maWheels[2].mSlipVariables.w) and folds the same clamped delta back into mLinearVelocity
        //   along the forward axis. A soft, capped nudge -- never a hard velocity set.
        void UpdateSpeedMatch(const BrnPlayerDriverControls* lpControls, f32 lfTimeStep);

        // AddLocalForce is the BASE's 4-argument form (ExternalPhysicsBody @0x825A1670). The
        // 2-argument declaration that used to sit here dropped both rw::physics::InputSpace tags,
        // and its FLAG guessed that the appliers pass "a local-space force at a local-space
        // position". They do not: every call site recovered from its own asm passes the FORCE in
        // WORLD_SPACE and only the POSITION in BODY_SPACE.
        //   ApplyNormalBoostForce @0x825D3138/0x825D3160 : r4 = 0 (WORLD), r5 = 1 (BODY)
        //   ApplyBoostKickForce   @0x825D3280/0x825D3290 : r4 = 0 (WORLD), r5 = 1 (BODY)
        //   ApplySuspensionForces @0x825D20FC/0x825D2100 : r4 = 0 (WORLD), r5 = 1 (BODY)
        //   UpdateAirRam          @0x825FCA7C/0x825FCA80 : r4 = mAirRamEffect[i].meImpulseSpace
        //                                                  (a STORED tag, not a literal), r5 = 1
        //   RaceCarPhysics::UpdateAftertouch @0x8262F490/0x8262F5C4 : r4 = 0, r5 = 0 (both WORLD)
        // (r4 gates the force vector, r5 the position -- AddLocalForce tests them at 0x825A17D0
        // `cmpwi r28,1` and 0x825A17FC `cmpwi r27,0`, i.e. against OPPOSITE values, so no single
        // 2-argument form can be right for both.)

        // ----- Vehicle-physics group (class TU): slam/shunt state read by the out-of-line
        //       Is{...}Slamed-or-Shunted predicates (bodied in VehiclePhysics.cpp) -----

        // @+0x1100: the in-progress slam. This home used to pin the six SlamEffect scalars FLAT
        // (mfSlamSteering @+0x1114, mfSlamOriginalSteering @+0x1118, mfSlamLife @+0x111C,
        // mfTotalSlamTime @+0x1120, mfRecoveryTime @+0x1124, mi8SlamNumber @+0x1128) with a note
        // saying the console pinned them flat "not as an embedded SlamEffect instance". That was
        // wrong: the DWARF (VehiclePhysics.h:927) declares `SlamEffect mSlamEffect` and the asm
        // agrees exactly -- the nested SlamEffect's own field order puts mfSteering/mfOriginalSteering/
        // mfSlamLife/mfTotalSlamTime/mfRecoveryTime/mi8SlamNumber at +0x14/+0x18/+0x1C/+0x20/+0x24/
        // +0x28 off a base of +0x1100, reproducing all six committed offsets, and the struct ends at
        // +0x1130 which is precisely where mShuntEffect (the DWARF's very next member) begins.
        // AddSlam @0x825D4880/0x825D489C/0x825D4904 reads +0x111C/+0x1120/+0x1128 off `this`.
        SlamEffect mSlamEffect;

        // @+0x1130: the embedded shunt effect; IsBeingSlamedOrShunted consults mShuntEffect.IsActive()
        // (asm: `addi r3,r3,0x1130 ; bl ShuntEffect::IsActive`). Pinned BY NAME.
        ShuntEffect mShuntEffect;

        // @+0x13E0: the id of the race car that last attacked (slammed/shunted) this vehicle, read
        // by IsBeingSlamedOrShuntedByRaceCar to filter (asm @0x8261529C: `lbz r11,0x13E0(r3) ; extsb`
        // then compared sign-extended against the queried id). This home used to call it
        // `mi8SlammingRaceCarId`, a proposed name; the DWARF member is mi8LastAttackersRaceCarIndex
        // (VehiclePhysics.h:979) and VehicleOutputInterface::UpdateRaceCarState @0x825EC8C0 settles
        // it -- it sign-extends +0x13E0 into RaceCarState+0x43C, which BrnVehicleEvents.h:136 names
        // mi8LastAttackersRaceCarIndex. (+0x1150, the byte right after mShuntEffect, is the DIFFERENT
        // member mi8LastContactedRaceCar; UpdateRaceCarState @0x825EC8CC copies that one to
        // RaceCarState+0x445. Two console members, one of which this slice had absorbed.)
        s8         mi8LastAttackersRaceCarIndex;

        // ----- ADDITIVE GROW (C08 airborne/water/freeze/spin group): the air-ram + spin slot
        //       allocators and their effect-record arrays, read/written by UpdateAirRam /
        //       UpdateSpinEffects / AddAirRam. Each is pinned BY NAME at its console offset (the
        //       ~0x1138 bytes of base + engine/drift/boost/slam/shunt state that precede mUsedAirRams
        //       are not reproduced as padding). The DWARF (VehiclePhysics.h:932-936) orders them
        //       exactly: mUsedAirRams, mAirRamEffect[4], mUsedSpins, maSpinEffects[8]. The X360 asm
        //       confirms the offsets: AddAirRam writes mAirRamEffect[i].mImpulse at +0x1160+48*i and
        //       walks mUsedAirRams as a u64 field at +0x1158; UpdateSpinEffects reads
        //       maSpinEffects[i].mfTimeRemaining at +0x1244+32*i and mUsedSpins at +0x1220. -----

        // @+0x1158 (4440): which of the 4 air-ram slots are in use (a set bit = an active queued ram).
        // BitArray<4> is one 64-bit field; UpdateAirRam/AddAirRam use GetFirstNonZeroBit/GetNextNonZeroBit
        // (the X360 cntlzd lowest-set-bit walk) + SetBit/UnSetBit. Pinned BY NAME.
        CgsContainers::BitArray<KU_MAX_AIR_RAMS> mUsedAirRams;

        // @+0x1160 (4448), stride 0x30: the 4 air-ram effect records (mImpulse @+0x00, mPosition @+0x10,
        // mfDecay @+0x20, meImpulseSpace @+0x24, mfTimerTillFire @+0x28). Pinned BY NAME.
        AirRamEffect mAirRamEffect[KU_MAX_AIR_RAMS];

        // @+0x1220 (4640): which of the 8 spin slots are in use. BitArray<8> is one 64-bit field.
        // UpdateSpinEffects walks it with GetFirstNonZeroBit/GetNextNonZeroBit + UnSetBit. Pinned BY NAME.
        CgsContainers::BitArray<KU_MAX_SPINS> mUsedSpins;

        // @+0x1230 (4656), stride 0x20: the 8 spin effect records (mForce @+0x00, mfTotalTime @+0x10,
        // mfTimeRemaining @+0x14). Pinned BY NAME.
        SpinEffect maSpinEffects[KU_MAX_SPINS];

        // ----- Vehicle-physics group (class TU): members read/written by the three VMX128 funcs
        //       (GetCarGroundDistanceCheck / GetTransformDelta / UpdateLinearVelocityMagnitude) -----

        // mTransform (@+0x10) and mLinearVelocity (@+0x50) are the ExternallySimulatedBody base's,
        // and are no longer re-declared here. GetTransformDelta reads the transform as the "current"
        // matrix (asm: `addi r10,r4,0x10 ; lvx128` of the four rows -- the +0x10 is the base
        // subobject offset, which is why it appeared as "base @+0x10" in the old note);
        // UpdateLinearVelocityMagnitude reads the velocity (`addi r9,r3,0x50 ; lvx128`) and
        // normalizes it, and GetDownForce squares it (`lvx128 v0,r4,0x50 ; vmsum3fp128`).

        // @+0x720: the live per-car attribute/tuning block (player vs AI set is reconciled each
        // frame by Update via SwitchAttribs). GetDownForce reads mpAttribs->mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo (asm:
        // `lwz r11,0x720(r4)`). Pinned BY NAME (the intervening handling state is not reproduced as
        // padding; mpAttribs points at one of the two embedded VehicleAttribs sets owned by the
        // full VehiclePhysics TU). ⚠️ NON-const, per the DWARF (VehiclePhysics.h:837
        // `VehicleAttribs * mpAttribs`) -- and the console MUTATES through it:
        // VehiclePhysics::SetAttributes @0x8262E070/@0x8262E098 re-streams the pointed-to set
        // and nudges its COM. The `const` that stood here was a reconstruction guess.
        VehicleAttribs* mpAttribs;

        // ===== ADDITIVE GROW (Construct wave, 2026-08-03): the TWO EMBEDDED ATTRIBUTE SETS =====
        // @+0x730 (1840) and @+0xAA0 (2720). Names and types VERBATIM from the DecFIGS DWARF
        // (VehiclePhysics.h:840 / :843), which places them immediately after mpAttribs -- and the
        // asm agrees exactly: VehiclePhysics::Construct @0x8262DC88/@0x8262DC94 calls
        // `VehicleAttribs::Construct` on `this+0x730` then `this+0xAA0`, a gap of 0x370 ==
        // sizeof(VehicleAttribs). mpAttribs (+0x720, a 4-byte console pointer) is re-pointed at one
        // of these two each frame by SwitchAttribs depending on whether the car is player- or
        // AI-driven; that is why the pointer and the storage are separate members. The PS3 build of
        // the same source makes the same two calls at its own +0x720/+0xA90.
        // These are 0x370 bytes EACH, so declaring them grows sizeof(VehiclePhysics) by 1760 bytes
        // -- which is correct: on the console they are part of the object. Parity here is BY NAME,
        // and no absolute offset in this class is static_asserted.
        VehicleAttribs mAIVehicleAttribs;       // :840  (+0x730)
        VehicleAttribs mPlayerVehicleAttribs;   // :843  (+0xAA0)

        // ===== The +0x570 above-ground-test block and the +0x6A0 half-extent are the BASE's =====
        // This home used to carry five separately-named members over the console bytes
        // +0x580/+0x590/+0x594/+0x596/+0x598, plus a scalar at +0x6A4. All six are views into two
        // SimpleVehiclePhysics members, and SetAboveGroundTestResult @0x82602880 proves the frame:
        // it takes `addi r11,this,0x570` (0x826029D4) and then writes
        //     +0x00 mIntersectionPosition   +0x10 mIntersectionNormal
        //     +0x24 mCollisionTag hi-u16    +0x26 mCollisionTag lo-u16
        //     +0x20 mfVerticalDistance      +0x28 mbValid = 1
        // so mAboveGroundTestResult sits at +0x570 and the five map as:
        //     mGroundNormal             +0x580 -> mAboveGroundTestResult.mIntersectionNormal
        //     mfWaterDepth              +0x590 -> mAboveGroundTestResult.mfVerticalDistance
        //     mWaterContactTag          +0x594 -> mAboveGroundTestResult.mCollisionTag, BE-high u16
        //     mRepresentativeContactTag +0x596 -> mAboveGroundTestResult.mCollisionTag, BE-low  u16
        //     mbAboveGroundTestValid    +0x598 -> mAboveGroundTestResult.mbValid
        // ⚠️ mfVerticalDistance is NOT a "water depth": the asm computes it as
        // mTransform.wAxis.y - lineTestResult.y (0x826029D4-0x826029F8), i.e. the height of the car
        // above the road. UpdateInWaterBehaviour tests that height against a threshold; the old
        // "water-depth scalar" name inverted the quantity's meaning.
        // ⚠️ mWaterContactTag / mRepresentativeContactTag were TWO names for the two halves of ONE
        // u32 CollisionTag. On the X360 the +0x594 halfword is the HIGH half of that word and +0x596
        // the LOW half; on x64 the byte positions swap, so a consumer must take the halves from
        // mCollisionTag.muValue by SHIFT, never by re-deriving a byte offset. Helpers below.
        u16 GetAboveGroundTagHi() const { return u16((mAboveGroundTestResult.mCollisionTag.muValue >> 16) & 0xFFFFu); }
        u16 GetAboveGroundTagLo() const { return u16( mAboveGroundTestResult.mCollisionTag.muValue        & 0xFFFFu); }

        // The car's vertical extent GetCarGroundDistanceCheck @0x825C0138 reads when the car is
        // inverted (`lfs f13,0x6A4(r3)`) was a separately-named f32 `mfCarGroundCheckExtent`. +0x6A0
        // is SimpleVehiclePhysics::mHalfExtent (both boost appliers load +0x6A0 and splat lane .z),
        // so +0x6A4 is simply its .y lane -- the box's vertical half-extent, which is exactly the
        // quantity that check needs. The base owns it; retired here.

        // mfSpeedMPH @+0x6C0 is the BASE's (SimpleVehiclePhysics :365). The DWARF types it VecFloat,
        // not f32 -- this home's f32 copy was both a duplicate and a narrowing. Recomputed each frame
        // at the tail of UpdateDriving = dot3(mTransform forward axis, mLinearVelocity) x 2.2369363.
        // UpdateRoadNoise reads it (`li r24,0x6C0 ; lvx128 v124,r29,r24`) so the rumble scales with
        // speed; UpdateBoost reads it (`li r6,0x6C0 ; lvx128 v12,r31,r6`) for the 5.0-mph floor, the
        // MaxBoostSpeed cap and the BoostKickMaxStartSpeed eligibility test. The +0x6A0/+0x6B0/+0x6C0
        // spacing is exactly mHalfExtent / mWheelPlanePosAndHeight / mfSpeedMPH in DWARF sequence,
        // which independently confirms both endpoints.

        // ----- ADDITIVE GROW (C06 steering/drift/handbrake group): the 11-Vector4 DRIFT STATE BANK
        //       (+0xFE0..+0x1080) + the drift byte flags + the ground-normal/aboveground members the
        //       drift forces consult. Member names + lane packing are VERBATIM from the DWARF
        //       (references/DecFIGS/.../VehiclePhysics.h). Pinned BY NAME at their console offsets. NOTE:
        //       mvSideForceMag_TimeBoosting_... (+0x1040), mbAllWheelsHaveTraction (+0x135B) and
        //       mfSpeedMPH (+0x6C0) are SHARED with the C07 boost/speed-match group and declared above --
        //       NOT re-declared here (one owner each). The drift pipeline reads/writes these by lane
        //       (.x/.y/.z/.w). -----

        // @+0xFE0 (4064): lane .x = SteeringAngle (the cached value GetSteeringAngle returns when not
        //   recomputing), .y = Steering, .z = PrevSteering, .w = DriftGasLetOffAmount.
        Vector4 mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount;

        // @+0xFF0 (4080): player-stat lanes (not read by C06; pinned to hold the bank stride between
        //   +0xFE0 and +0x1000 so the named offsets stay correct).
        Vector4 mvPlayerStatSpeed_PlayerStatStrength_PlayerStatControl_PlayerStatBoost;

        // @+0x1000 (4096): lane .y = MaintainedSpeed (MaintainDriftSpeed target), .z = NeutralControlTime,
        //   .w = DriftScale (the live drift scale UpdateDriftScale grows). lane .x = Spare.
        Vector4 mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale;

        // @+0x1010 (4112): lane .x = 1/TimeToReachTargetDriftSlip, .y = StartSlip (seeded by EnterDrift),
        //   .z = TimeDrifting, .w = BrakeScale.
        Vector4 mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale;

        // @+0x1020 (4128): lane .x = DesiredDriftAngleScale, .y = CappedDriftScale, .z = DesiredDriftSlip,
        //   .w = TimeInFrictionState.
        Vector4 mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState;

        // @+0x1030 (4144): lane .x = LatDriftForceFactor, .y = DriftPushTime, .z = MaxSteeringAngle,
        //   .w = CurrentDriftAngle (the speed-blend authority lane GetSteeringAngle uses).
        Vector4 mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle;

        // ⭐ ADDED 2026-08-06 (UpdateVehiclePhysics wave). Two DWARF-attested inlines over the
        // steering bank, both recovered from the stationary-wheel-pose tail of
        // VehicleManager::UpdateVehiclePhysics @0x82645F58..0x82645FA0:
        //   GetMaxSteeringAngle (DWARF :1745; `lvx +0x1030 ; vspltw lane 2` -- the .z lane,
        //   de-SIMD'd to f32 per the project convention for VecFloat returns), and
        //   OverrideWheelAngle (DWARF :1144, VecFloat arg; its console inline is
        //   SetPackedSteeringAngle+SetX -- write lane .x of the +0xFE0 steering register,
        //   `vrlimi128 v13, v0, 8, 0 ; stvx128`).
        f32  GetMaxSteeringAngle() const
        {
            return mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.z;
        }
        void OverrideWheelAngle(f32 lfAngleRadians)
        {
            mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x = lfAngleRadians;
        }

        // @+0x1050 (4176): lane .x = PropSpeedMaintainAlongZ, .y = PropSpeedMaintainAlongVel
        //   (MaintainDriftSpeed blends the impulse along Z vs velocity by these).
        Vector4 mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor;

        // @+0x1060 (4192): lane .x = TimeStandingStill, .z = TimeWithoutTraction, .w = TimeWithTraction
        //   (UpdateDriftState's "off-ground too long" exit guard reads the without-traction lane).
        Vector4 mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction;

        // @+0x1070 (4208): lane .x = TimeSinceHardLanding, .y = SteeringOverride, .z = CarCarResponse,
        //   .w = SecondsSinceLastWallContact.
        Vector4 mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact;

        // @+0x1080 (4224): lane .x = DampRollVel, .y = TimeInDriftWithStaticFriction,
        //   .z = TimeHandbrakeHasBeenOn, .w = TimeSinceLastHandBrake (UpdateHandBrake's two timers).
        Vector4 mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake;

        // @+0x10F4 (4340): the drift sub-force gate bitfield (ExitDrift resets it to DO_ALL=0xFF; the
        //   slam path writes -1 here -- DWARF VehiclePhysics.h:910). Pinned BY NAME.
        DriftFlags mDriftFlags;

        // @+0x1352 (4946): the drift state (0=None, 1=FacingLeft, 2=FacingRight). The master gate for
        //   the whole drift pipeline; EnterDrift latches it from the entry steering sign. Pinned BY NAME.
        u8 mu8DriftState;

        // @+0x1353 (4947): per-frame world-collision count (UpdateDriftState's `>0` exit guard reads it).
        s8 mi8NumWorldCollisions;

        // @+0x1354 (4948): the running collision count (UpdateDriftState's second `>0` exit guard). The
        //   X360 reads it as a byte against 0; modelled as the full int32 it is in the DWARF.
        s32 miNumCollisions;

        // @+0x1358 (4952): the handbrake latch UpdateHandBrake maintains (engaged above input 0.1, with
        //   hysteresis). Read by UpdateDrift / MaintainDriftSpeed / the drift-force gates. Pinned BY NAME.
        bool mbHandBrake;

        // The drift forces' ground normal (+0x580) and above-ground-valid flag (+0x598) are the base's
        // mAboveGroundTestResult.mIntersectionNormal / .mbValid -- see the block above. The drift code
        // tangent-projects against the normal so drift never pushes into or off the road
        // (ApplyDriftLatForce/ApplyDriftYaw/MaintainDriftSpeed: `lvx128 r31,1408 ; vmsum3fp128`).

        // The body-space box half-extent (+0x6A0) is SimpleVehiclePhysics::mHalfExtent. Both boost
        // appliers read its .z lane, NEGATE it, and use it as the z of the local force-application
        // point, so the boost force acts behind the centre of mass (ApplyNormalBoostForce
        // @0x825D3120-0x825D3170, ApplyBoostKickForce @0x825D3270-0x825D32B4).

        // @+0x1040 (4160, BY NAME). The boost-state register
        // (mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime in the full
        // VehiclePhysics). The C07 group reads/writes: .y = TimeBoosting (time the current boost has run;
        // 0 == fresh), .z = TimeSinceLastBoostKick (the kick cooldown timer), .w = CurrentBoostKickTime
        // (the seeded kick-window length). Lane .x = SideForceMag (owned by another phase, untouched
        // here). Pinned BY NAME (the ~0x1000 bytes of base/engine/drift/suspension state that precede it
        // are not reproduced as padding).
        Vector4    mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime;

        // @+0x10F5 (4341, BY NAME). The boost-regime flag UpdateBoost writes and the two appliers assert:
        // true while the current boost is in its KICK window, false for sustained NORMAL boost. The X360
        // sets it from `CurrentBoostKickTime > TimeBoosting` (`stb r11,0x10F5(r31)`). Pinned BY NAME.
        bool       mbInBoostKick;

        // ⚠️ RETIRED, AND THE OLD NOTE WAS WRONG. This home used to declare
        //     Vector3 mLinearImpulseAccumulator;      // +0x100
        //     Vector3 mAngularImpulseAccumulatorRow;  // +0x120
        // with the justification "+0x100 = linear impulse accumulator, +0x110/+0x120 = angular".
        // Those are VehiclePhysics-frame offsets, and the base subobject starts at +0x10, so
        //     +0x100 == base+0xF0  == ExternalPhysicsBody::mTotalTorque
        //     +0x120 == base+0x110 == ExternalPhysicsBody::mTotalAngularImpulse
        // -- the first is a TORQUE, not a linear impulse. AddLocalForce @0x825A183C-0x825A1868
        // settles the base frame independently: it accumulates the rotated force into base+0xE0 and
        // cross(r,F) into base+0xF0, i.e. mTotalLinearForce / mTotalTorque.
        // ApplyBoostKickForce @0x825D338C-0x825D3408 damps exactly three registers along
        // mTransform.xAxis (the body RIGHT axis, i.e. the pitch axis): +0x60, +0x100 and +0x120.
        // Read through the corrected map those are {mAngularVelocity, mTotalTorque,
        // mTotalAngularImpulse} -- three ANGULAR quantities, which is the only coherent set for a
        // wheelie limiter. The old reading mixed one linear quantity into an angular damper.
        // Both members are the base's; nothing is re-declared here.

        // @+0x135B (4955, BY NAME). The "all driven wheels have traction" gate the boost appliers and
        // UpdateSpeedMatch consult before applying force (`lbz r11,0x135B(r3) ; cmplwi ; beq`). Pinned BY
        // NAME (it sits in the drift/crash byte-flag block; mbHandBrake/mu8DriftState are nearby).
        bool       mbAllWheelsHaveTraction;

        // @+0x135D (4957). The one-frame "was just slammed" latch AddSlam raises (`stb r10,0x135D(r11)`
        // @0x825D4940) and VehiclePhysics::Prepare @0x826380A0 clears. This home used to call it
        // `mbSlamActive`; the DWARF member is mbJustBeenSlammed (VehiclePhysics.h:958) and
        // VehicleOutputInterface::UpdateRaceCarState @0x825ECB0C settles it -- it copies +0x135D to
        // RaceCarState+0x44F, which BrnVehicleEvents.h:146 names mbJustBeenSlammed. The DWARF byte
        // order also lands it exactly: mbAllWheelsHaveTraction +0x135B, mbResetCarTransform +0x135C,
        // mbJustBeenSlammed +0x135D.
        bool       mbJustBeenSlammed;

        // @+0x1340: cached normalized linear velocity (xyz lanes) + speed magnitude (w/"plus" lane),
        // written by UpdateLinearVelocityMagnitude (`addi r10,r3,0x1340 ; stvx128`) and read by
        // GetLinearVelocityDirection / GetLinearVelocityMagnitude. Pinned BY NAME.
        Vector3Plus mNormLinearVelocityMag;

        // @+0x1370: the previous-frame physics transform, the "from" matrix of GetTransformDelta
        // (asm: `addi r11,r4,0x1370 ; lvx128` of the four rows, whose orthonormal 3x3 is inverted
        // inline). Pinned BY NAME.
        Matrix44Affine mPreviousTransform;

        // ===== ADDITIVE GROW (Reset wave, 2026-08-03): the ten members VehiclePhysics::Reset
        //       @0x825FDD78 writes that this home had never DECLARED. Every name and type is
        //       VERBATIM from references/DecFIGS/dwarfdump/.../VehiclePhysics.h at the cited line;
        //       none is abbreviated or inferred. Pinned BY NAME (this header is deliberately not in
        //       DWARF declaration order -- e.g. the +0x1040 boost register already sits after the
        //       +0x1352 drift state -- so they are grouped here rather than interleaved). The
        //       console offsets are the ones Reset's stores land on. =====

        // @+0x10E0 (4320, DWARF :933). The cached steering direction; Reset zeroes it.
        Vector3 mSteeringDirection;

        // @+0x10F0 (4336, DWARF :936). Reset seeds it to 5.0f == flt_8200426C ==
        // KF_STUCK_IN_COLLISION_TEST_INTERVAL (the DWARF's own constant for this member).
        f32 mfTimeUntilStuckInCollisionTest;

        // @+0x10F6 / +0x10F8 (DWARF :945 / :951). Two latches Reset clears. They sit either side of
        // mbIsUsingAIDonutAttribs (:948), which Reset does NOT touch and which is not declared here.
        bool mbForceFrozen;
        bool mbGivenAftertouchAirBoost;

        // ⭐ ADDED 2026-08-06 (UpdateVehiclePhysics wave). DWARF :1381 `void SetForceFrozen(bool)`;
        // the X360 inlines it as the single byte store at +0x10F6 (UpdateVehiclePhysics
        // @0x826458EC `stb r11, 0x1836(this+5216*player)`).
        void SetForceFrozen(bool lbFrozen) { mbForceFrozen = lbFrozen; }

        // @+0x1150 (4432, DWARF :966). The last car this one touched; Reset seeds -1 ("none").
        s8 mi8LastContactedRaceCar;

        // @+0x1350 / +0x1351 (DWARF :987 / :990). The airborne latches; Reset clears both.
        bool mbHasAir;
        bool mbHadAirLastFrame;

        // @+0x135C (4956, DWARF :1014). ⚠️ Reset sets this one TRUE, not false -- it is the request
        // that the next update re-seat the car's transform. The byte order mbAllWheelsHaveTraction
        // +0x135B / mbResetCarTransform +0x135C / mbJustBeenSlammed +0x135D (already documented
        // above) places it exactly.
        bool mbResetCarTransform;

        // @+0x1361 (4961, DWARF :1029). The burnout latch; Reset clears it.
        bool mbDoingBurnout;

        // @+0x13D0 (5072, DWARF :1044). The force-feedback wheel spring the input layer reads;
        // Reset zeroes both of its coefficients. CgsInput::Device::WheelFFSpring is the committed
        // type from CgsInputTypes.h, not a slice.
        CgsInput::Device::WheelFFSpring mWheelFFSpring;

        // ===== ADDITIVE GROW (C03 suspension/downforce/weight group) =====
        // @+0xE10 (3600), stride 0x30: the four 1-D damped suspension springs (one per driven wheel).
        // SuspensionSpring is the already-committed namespace-scope slice (sizeof 0x30 = 3*Vector4).
        SuspensionSpring maSprings[eNumDrivenWheels];

        // @+0xED0 (3792, DWARF VehiclePhysics.h:849). The per-spring mass scalers. Construct
        // @0x8262DC64 zeroes it with a single whole-register `stvx128 v127(==0), this, 0xED0`
        // BEFORE it builds the engine; SetupSuspension installs the real values later. The offset
        // closes by sequence with no slack: maSprings @+0xE10 + 4*0x30 == +0xED0, and the very next
        // DWARF member (mWeightTransfer, already declared below) is at +0xEE0. Pinned BY NAME.
        Vector4 mvSpringMassScalers;

        // @+0xEE0 (3808): the dynamic load-transfer force CalculateWeightTransfer builds and
        // distributes to the springs (SetExternalForce). DWARF VehiclePhysics.h:830. The offset is
        // confirmed by sequence: maSprings @+0xE10 + 4*0x30 = +0xED0 = mvSpringMassScalers, then
        // mWeightTransfer at +0xEE0.
        Vector3 mWeightTransfer;        // +0xEE0 (packed; .x/.y/.z load-transfer per body axis)

        // @+0xEF0 (3824): the crash-state register Reset zeroes wholesale. DWARF VehiclePhysics.h:855
        // places it between mWeightTransfer and mEngine, which is exactly the +0xEE0/+0xEF0/+0xF00
        // sequence. Pinned BY NAME.
        Vector4 mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare;

        // @+0xF00 (3840): the engine/gearbox model. ADDED 2026-08-03 with SetWheelVelocities, which
        // ends by calling `mEngine.Reset(averageWheelAngularVelocity)` -- the X360 does
        // `addi r3, r29, 0xF00 ; vspltw v1, v0, 0 ; bl Engine::Reset` at 0x825FDD40-0x825FDD4C, and
        // `Engine::mAttribs` is Engine's leading member, so +0xF00 is the object's own base.
        // Construct's `mEngine.mAttribs.Construct(); mEngine.Reset(0);` pair reads the same base.
        // The member was referenced by three comment blocks in this header but had never been
        // DECLARED. Pinned BY NAME; the offset closes by sequence (maSprings +0xE10 + 4*0x30 =
        // +0xED0 mvSpringMassScalers, +0xEE0 mWeightTransfer, +0xEF0
        // mvSpeedOnLastCrashMPH_TimeCrashing_..., then mEngine at +0xF00).
        Engine mEngine;

        // @+0x1330 (4912): the PREVIOUS frame's world-space linear velocity. This home used to call
        // it `mWeightTransferMirror` and describe it as "the +0x50 weight register snapshot" -- but
        // +0x50 is base+0x40 == mLinearVelocity, not a weight register, and UpdateSuspension
        // @0x8261F6E4-0x8261F6F8 literally does `lvx128 v0,this,0x50 ; stvx128 v0,this,0x1330`
        // immediately before calling CalculateWeightTransfer, so the snapshot exists so the weight
        // transfer can difference this frame's velocity against last frame's (i.e. derive the body
        // acceleration). That is the DWARF member mPreviousWorldSpaceVelocity (VehiclePhysics.h:939),
        // and the offset closes exactly: maSpinEffects[8] runs +0x1230..+0x1330, and the members
        // after it (mNormLinearVelocityMag +0x1340, mbHasAir +0x1350, mu8DriftState +0x1352,
        // mi8NumWorldCollisions +0x1353, miNumCollisions +0x1354, mbHandBrake +0x1358,
        // mbAllWheelsHaveTraction +0x135B, mbJustBeenSlammed +0x135D) reproduce every committed
        // offset in the byte block in DWARF declaration order.
        Vector3 mPreviousWorldSpaceVelocity;

        // ==========================================================================================
        // ⭐⭐ THE OWN-MEMBER BLOCK CLOSES -- 2026-08-03 (VehiclePhysics own-block wave).
        //
        // The DecFIGS DWARF (VehiclePhysics.h:822-982) lists this class's own members in ORDER; the
        // X360 asm gives OFFSETS. Laid against each other they meet with **zero slack at both
        // ends**, and the two derivations know nothing about each other:
        //   * the block OPENS at 0x720 -- `VehiclePhysics::Construct` @0x8262DC8C `stw r30, 0x720(r31)`
        //     is mpAttribs, and 0x720 is exactly sizeof(SimpleVehiclePhysics) as that class's own
        //     block closes it (BrnSimpleVehiclePhysics.h, X360Layout::KU_SVP_SIZEOF);
        //   * the block CLOSES at 0x13F0 -- the last member (mpDebugComponent @0x13E4, 4 bytes)
        //     ends at 0x13E8, which 16-rounds to 0x13F0, and 0x13F0 is exactly where the PREVIOUS
        //     wave independently put RaceCarPhysics::mPropCollisionImpulseSum (from a different
        //     function and a PS3 Delta = -16 cross-check), from which that block closes on the
        //     0x1460 == 5216 per-car stride BrnVehicleManager.h pins from a third function.
        // Three chains, no shared assumption, no slack anywhere. THAT is the proof -- not the map.
        //
        //   X360    member                                   first-hand evidence
        //   ------  ---------------------------------------  ----------------------------------------
        //   0x720   mpAttribs                                Construct `stw r30,0x720(r31)`
        //   0x730   mAIVehicleAttribs      (0x370)           Construct `addi r3,r31,0x730`
        //   0xAA0   mPlayerVehicleAttribs  (0x370)           Construct `addi r3,r31,0xAA0`
        //   0xE10   maSprings[4]           (stride 0x30)     Construct `addi r29,r31,0xE10` + the
        //                                                    SuspensionSpring::Prepare loop's
        //                                                    `addi r29,r29,0x30`; 0xAA0+0x370==0xE10
        //   0xED0   mvSpringMassScalers                      Construct `li r11,0xED0 ; stvx128`
        //   0xEE0   mWeightTransfer                          (bracketed, zero slack)
        //   0xEF0   mvSpeedOnLastCrashMPH_TimeCrashing_...   Construct `li r28,0xEF0 ; stvx128`
        //   0xF00   mEngine                (0xD0)            Construct `addi r30,r31,0xF00` ->
        //                                                    EngineAttribs::Construct, Engine::Reset.
        //                                                    Engine::Prepare `memcpy(this,src,0xA0)`
        //                                                    fixes EngineAttribs==160 and
        //                                                    `stw 1,0xC0(r31)` fixes mu8CurrentGear
        //                                                    @Engine+0xC0 => sizeof(Engine)==0xD0.
        //   0xFD0   mvfWheelFrictionLinearMultiplier         forced by the two neighbours; and this
        //                                                    header's own HandleWheelPairFriction note
        //                                                    already said "+4048" (written "0x4048"
        //                                                    by mistake) == 0xFD0 -- an INDEPENDENT
        //                                                    earlier witness of the same seat.
        //   0xFE0   mvSteeringAngle_Steering_PrevSteering_..  GetSteeringAngle `addi r11,r31,0xFE0`
        //                                                    then `vspltw128 v124,v0,0` == lane .x
        //                                                    == SteeringAngle. Name/lane agree.
        //   0xFF0
        //   ..0x1080 the remaining nine Vector4s              Construct lane-inserts at 0x1040 and
        //                                                    0x1070; UpdateHandBrake works 0x1080
        //                                                    lanes **z and w**, which the member name
        //                                                    calls TimeHandbrakeHasBeenOn /
        //                                                    TimeSinceLastHandBrake. Exact.
        //   0x1090  mPreviousControls      (0x48)            sizeof is this tree's own asm literal
        //                                                    (UpdateDriving `memcpy(...,0x48)`), and
        //                                                    meDriverType @+0x44 lands on 0x10D4 --
        //                                                    see below.
        //   0x10D4  mPreviousControls.meDriverType           VehiclePhysics::Prepare
        //                                                    `stw r30,0x10D4(r31)`; read by Update,
        //                                                    HandleWheelFrictionCrashing,
        //                                                    DeformableObject::UpdateAbsorptionSet,
        //                                                    DoBodyPartWorldContactGeneration and
        //                                                    VehicleManager::HandleRaceCarRaceCarContact
        //   0x10E0  mSteeringDirection                       (Reset, already committed)
        //   0x10F0  mfTimeUntilStuckInCollisionTest          VehicleManager::
        //                                                    ReadPlayerStuckTractionLineTestResults and
        //                                                    UpdatePlayerStuckInCollisionSpheres
        //                                                    `lfs`/`stfs 0x10F0`
        //   0x10F4  mDriftFlags                              ExitDrift `stb r4,0x10F4(r3)`
        //   0x10F5  mbInBoostKick                            (already committed)
        //   0x10F6  mbForceFrozen                            UpdateFreezing `lbz r9,0x10F6(r31)`
        //   0x10F7  mbIsUsingAIDonutAttribs                  DWARF order between two attested bytes
        //   0x10F8  mbGivenAftertouchAirBoost                (already committed)
        //   0x1100  mSlamEffect            (0x30)            ⭐ AddSlam touches ALL SEVEN fields at
        //                                                    the DWARF's own intra-struct offsets off
        //                                                    0x1100: +0x14/+0x18 mfSteering and
        //                                                    mfOriginalSteering (the SAME value to
        //                                                    both), +0x1C mfSlamLife, +0x20
        //                                                    mfTotalSlamTime, +0x24 mfRecoveryTime,
        //                                                    +0x28 mi8SlamNumber clamped to 2 ==
        //                                                    KI8_MAX_NUM_SLAMS-1.
        //   0x1130  mShuntEffect           (0x20)            HackedResetAndFlyAround `addi r6,r3,0x1130`
        //   0x1150  mi8LastContactedRaceCar                  HandleRaceCarRaceCarContact
        //                                                    `stb r14,0x1150(r16)`; read by
        //                                                    UpdateRaceCarState / the two Check* funcs
        //   0x1158  mUsedAirRams           (u64)             UpdateAirRam `addi r21,r14,0x1158` then a
        //                                                    `ld`/`cntlzd` walk bounded at **4**;
        //                                                    Construct zeroes it with `std r30,0x1158`
        //   0x1160  mAirRamEffect[4]       (stride 0x30)     (already committed)
        //   0x1220  mUsedSpins             (u64)             UpdateSpinEffects `addi r22,r29,0x1220`,
        //                                                    walk bounded at **8**; Construct
        //                                                    `std r30,0x1220`
        //   0x1230  maSpinEffects[8]       (stride 0x20)     (already committed)
        //   0x1330  mPreviousWorldSpaceVelocity              Construct `li r6,0x1330 ; stvx128`
        //   0x1340  mNormLinearVelocityMag                   UpdateLinearVelocityMagnitude
        //                                                    `addi r10,r3,0x1340`
        //   0x1350  mbHasAir                                 UpdateInAirBehaviour / UpdateDownForce /
        //                                                    UpdateSuspensionPostSimulation
        //   0x1351  mbHadAirLastFrame                        UpdateInAirBehaviour, read immediately
        //                                                    after 0x1350
        //   0x1352  mu8DriftState                            ⭐ ExitDrift `stb r5,0x1352(r3)`
        //   0x1353  mi8NumWorldCollisions                    ApplyCrashed/ShowtimeContactImpulse
        //                                                    `lbz`+`stb` (byte counter)
        //   0x1354  miNumCollisions                          the same three impulse funcs,
        //                                                    `lwz`+`stw` (4-byte counter)
        //   0x1358  mbHandBrake                              ⭐ UpdateHandBrake `lbz`/`stb 0x1358`
        //   0x1359  mbDeformationModelIsActive               ⭐ VehicleManager::SetRaceCarCrashing
        //                                                    `stb 1,0x1359(record)` immediately
        //                                                    before the ResetDeformableAABB copy
        //   0x135A  mbDeformedThisFrame                      VehicleOutputInterface::UpdateRaceCarState
        //   0x135B  mbAllWheelsHaveTraction                  (already committed)
        //   0x135C  mbResetCarTransform                      (already committed)
        //   0x135D  mbJustBeenSlammed                        ⭐ AddSlam `stb r10,0x135D`
        //   0x135E  mbOverrideSteering                       DWARF order (no direct site; bracketed
        //                                                    by 0x135D and 0x135F, both attested)
        //   0x135F  mbIsWedgedInWorld                        ⭐ VehicleManager::DoPlayerStuckLineTests
        //                                                    `stb ...,0x135F`
        //   0x1360  mbIsFrontRayOccluded                     ⭐ DoPlayerStuckLineTests `stb ...,0x1360`
        //   0x1361  mbDoingBurnout                           ⭐ UpdateBurnout `lbz`/`stb 0x1361`
        //   0x1362  mbContactingWall                         ⭐ CheckForGrindingAndRubbing
        //                                                    `lbz 0x1362`; HackedResetAndFlyAround `stb`
        //   0x1370  mPreviousTransform     (0x40)            GetTransformDelta `addi r11,r4,0x1370`
        //                                                    + the four row loads
        //   0x13B0  mLastLinearVelocity                      (bracketed, zero slack)
        //   0x13C0  mPitchYawRollFromTakeOff                 UpdateInAirBehaviour `addi r24,r30,0x13C0`
        //                                                    (twice) and `li r11,0x13C0`
        //   0x13D0  mWheelFFSpring         (8)               UpdateDriving `stfs 0x13D0` + `stfs
        //                                                    0x13D4`; UpdateInAirBehaviour
        //                                                    `stfs f30,0x13D0`. Two floats -- exactly
        //                                                    the DWARF's mfStrength/mfOffset.
        //   0x13D8  mbRollingInAir                           UpdateInAirBehaviour `stb`/`lbz 0x13D8`
        //   0x13DC  meCarType                                ⭐ VehiclePhysics::Prepare `stw r8,0x13DC`
        //   0x13E0  mi8LastAttackersRaceCarIndex             AddSlam `stb r8,0x13E0`; AddShunt
        //                                                    `stb r4,0x13E0` (already committed)
        //   0x13E4  mpDebugComponent                         ApplySuspensionForces / UpdateDownForce
        //                                                    `lwz r11,0x13E4`
        //                                                    -> ends 0x13E8, 16-rounds to **0x13F0**
        //
        // ⚠️ HOST DIVERGENCE, stated once and NOT pretended away. This block is NOT width-identical
        // on x64 (unlike the RaceCarPhysics one): mpAttribs and mpDebugComponent are pointers, and
        // several embedded sub-types in this tree are reconstructions rather than byte-exact copies
        // of the console's. So NO absolute console offset in this class is static_asserted on the
        // host. What the gate (VehiclePhysics_layout_check.cpp) DOES assert is (a) the arithmetic
        // above -- that the DWARF order plus the asm-literal sub-object sizes reproduces every
        // asm-literal anchor and lands on 0x13F0 -- and (b) the host-side sizes of the pointer-free
        // sub-structs the map depends on.
        // ==========================================================================================

        // ----- ADDITIVE GROW (own-block closure wave, 2026-08-03): the fourteen DWARF members of
        //       this class that this home had never DECLARED. Every name, type and DWARF line is
        //       verbatim; every offset is in the map above. Nothing existing is reordered. -----

        // @+0xFD0 (4048, DWARF :835). The scalar HandleWheelPairFriction multiplies the residual
        // linear friction force by before accumulating it. Referenced by this header's own
        // HandleWheelPairFriction note since before this wave, but never declared.
        VecFloat mvfWheelFrictionLinearMultiplier;

        // @+0x1090 (4240, DWARF :905). LAST FRAME's driver controls -- the whole 0x48-byte payload,
        // not a slice. UpdateDriving `memcpy`s the incoming controls over it at the end of the frame;
        // ModifyControlsForDrift/UpdateBoost difference against it. Its meDriverType lane (+0x44,
        // i.e. class +0x10D4) is what the takedown chain reads to ask "is this car AI-driven?".
        BrnPlayerDriverControls mPreviousControls;

        // @+0x10F7 (4343, DWARF :916). True while the AI donut-attribs set is installed
        // (SwitchAIDonuttingAttribs latches it). Sits between mbForceFrozen and
        // mbGivenAftertouchAirBoost, both of which were already declared.
        bool mbIsUsingAIDonutAttribs;

        // @+0x1359 (4953, DWARF :954). ⭐ The seat the record's `mbCrashCommitted` was really at.
        // VehicleManager::SetRaceCarCrashing @0x82635440 sets it to 1 and, in the same guarded
        // block, copies mOriginalAABB over mDeformableAABB (ResetDeformableAABB) -- i.e. "arm the
        // deformation model and reset its box" is one action.
        bool mbDeformationModelIsActive;

        // @+0x135A (4954, DWARF :955). Published to RaceCarState by
        // VehicleOutputInterface::UpdateRaceCarState @0x825EC9FC.
        bool mbDeformedThisFrame;

        // @+0x135E (4958, DWARF :960). The steering-override latch that pairs with the
        // mvTimeSinceHardLanding_**SteeringOverride**_... lane. No direct asm site of its own; it is
        // bracketed with zero slack by mbJustBeenSlammed (+0x135D) and mbIsWedgedInWorld (+0x135F),
        // both of which ARE attested. FLAG: seat is by order, not by a site.
        bool mbOverrideSteering;

        // @+0x135F (4959, DWARF :961) and @+0x1360 (4960, DWARF :962). Both are written by
        // VehicleManager::DoPlayerStuckLineTests (@0x825C3B70/@0x825C4A28 and
        // @0x825C498C/@0x825C4994) and both are published by UpdateRaceCarState.
        bool mbIsWedgedInWorld;
        bool mbIsFrontRayOccluded;

        // @+0x1362 (4962, DWARF :967). CheckForGrindingAndRubbing @0x825B5484 reads it on BOTH cars
        // before classifying a grind; HackedResetAndFlyAround clears it.
        bool mbContactingWall;

        // @+0x13B0 (5040, DWARF :970) and @+0x13C0 (5056, DWARF :972). The previous linear velocity
        // and the integrated take-off attitude UpdateInAirBehaviour maintains (`addi r24,r30,0x13C0`).
        Vector3 mLastLinearVelocity;
        Vector3 mPitchYawRollFromTakeOff;

        // @+0x13D8 (5080, DWARF :974). UpdateInAirBehaviour @0x825D123C/@0x825D186C is the only
        // writer and reader.
        bool mbRollingInAir;

        // @+0x13DC (5084, DWARF :977). ⭐ The seat the record's `mfPlayerBoostStrengthStat` was
        // really at -- and it is an INT, not a float: VehiclePhysics::Prepare @0x826380F0 stores it
        // with `stw`, and VehicleManager::ApplyPlayerStats @0x8259BFE8 stores the car-stats action's
        // sixth word into it with `stw` too.
        // FLAG: BrnResource::ECarType has no committed home in this tree yet (same as
        // BrnVehicleManager.h's maPlayerCarStats note), so it is carried as the s32 it is on the
        // wire. Retype when that enum lands.
        s32 meCarType;   // logical: BrnResource::ECarType

        // @+0x13E4 (5092, DWARF :982). The per-car debug component the suspension/downforce debug
        // draws hang off (`lwz r11,0x13E4` in ApplySuspensionForces and UpdateDownForce; a null
        // check gates the draw). Declared as an opaque forward-declared pointer -- the component's
        // own type is a separate TU.
        DebugComponent* mpDebugComponent;

        // ----- C03 suspension phases (bodies in VehiclePhysics.cpp) -----
        // ⚠️ ARITY CONFORMED 2026-08-09 (attribs-setup wave): the DWARF (VehiclePhysics.h:1646)
        // spells `void SetupSuspension()` -- NO parameter -- and the @0x8262E10C call site sets
        // no f1. The committed `f64 lfTimeStep` was a slice artifact; the blocked skeleton and
        // its call sites are re-pointed.
        // @0x825CF718 (190) -- ⭐ BODIED 2026-08-11 (ground-contact wave); the 2026-08-03 BLOCKED
        // verdict is RETRACTED in the .cpp with the four rodata values read out of the image.
        // Seeds maSprings[i].{stiffness, damping, mass} from mvSpringMassScalers x the body mass.
        void SetupSuspension();                             // @0x825CF718
        // ⭐ BODIED 2026-08-11 (suspension-springs wave). Its "PARTIAL / un-pinned lane" verdict is
        // RETRACTED in the .cpp: it writes maWheels[i].mPosition.y -- the exact input the grounded
        // arm of UpdateSuspensionSprings reads -- and every offset it touches is a committed member.
        void ApplyWheelWeight();                            // @0x825F7898
        // @0x825F9DD0 -- ⭐⭐ THE MassOnWheel WRITER (the per-frame load pass). dt IS AN ARGUMENT:
        // UpdateSuspension parks the incoming vector (`vmr128 v127,v1`) and re-issues `vmr128 v1,v127`
        // before this call at 0x8261F6EC, and the callee's first instruction is `vrefp v13,v1` -- the
        // 1/dt that turns the velocity delta into an acceleration. The old no-arg spelling was a slice
        // artifact of the [partial] body that never used dt.
        void CalculateWeightTransfer(VecFloat lvfTimeStep);
        void ApplySuspensionForces();                       // @0x825D1EE8 (lever arm + direction + gate corrected 2026-08-11)
        void UpdateSuspension(f64 lfTimeStep);              // @0x8261F698 CLEAN (the virtual spine)
        // ⭐ BODIED 2026-08-11 (suspension-springs wave); the BLOCKED verdict is RETRACTED in the
        // .cpp. ⚠️ ARITY CONFORMED at the same time: UpdateSuspension @0x8261F698 parks the
        // incoming dt (`vmr128 v127,v1`) and re-issues `vmr128 v1,v127` before EACH of its four
        // phase calls, and the callee integrates with it (0x825F9058/0x825F9074). The committed
        // no-parameter form was a slice artifact -- the same shape as SetupSuspension's, inverted.
        void UpdateSuspensionSprings(VecFloat lvfTimeStep); // @0x825F7AF0
        void UpdateSuspensionPostSimulation();              // @0x825F6BB0 BLOCKED (degenerate VMX giant)
        void StabiliseAfterHardLanding();                   // @0x825D1890 PARTIAL (powf settle blocked)

        // ===== ADDITIVE GROW (C09 crash/contact-impulse group) =====
        // The contact-impulse handlers + the slam enqueue/tick. (SetCrashing override + UpdateShunt +
        // UpdateCrashing are already declared above / left declare-only.)
        //
        // ⚠️⚠️ SIGNATURES CORRECTED 2026-08-02 (physics wave 3) -- THE DROPPED-ARGUMENT TRAP, AGAIN.
        // All four of these forward TWO `rw::physics::InputSpace` tags straight into the base
        // ExternalPhysicsBody::GetImpulsesFromLocalImpulse, and the committed 2-/3-argument forms
        // dropped both. This is the same defect wave 2b settled for the AddLocal* family, in four
        // more functions. The base kernel @0x825A1A80 gates on them explicitly:
        //     0x825A1A88  cmpwi cr6, r4, 1     ; r4 == leImpulseSpace  (BODY_SPACE -> rotate)
        //     0x825A1AB4  cmpwi cr6, r5, 0     ; r5 == lePositionSpace (WORLD_SPACE -> subtract COM)
        // and each caller's own asm shows where its r4/r5 come from:
        //     ApplyCarContactImpulse      @0x825D4C10  r4,r5 NEVER WRITTEN before the bl -> both are
        //                                              this function's own arguments, passed through.
        //     ApplyCrashedContactImpulse  @0x825D4D50  r4,r5 untouched (`mr r30,r6` proves the bool
        //                                              is the THIRD integer arg, i.e. r4/r5 precede it).
        //     ApplyShowtimeContactImpulse @0x825D4E00  same shape (`clrlwi r10,r6,24` on the bool).
        //     ApplyWallContactImpulse     @0x825FEA18  `mr r29,r4` at entry then `mr r4,r29` at the
        //                                              call -> the impulse space is forwarded; the
        //                                              POSITION space is the literal `li r5,1`
        //                                              (BODY_SPACE), so this one takes only one tag
        //                                              and its `bool` really is r5.
        // FLAG (not fabricated, not yet settled): the VECTOR-register slots do not line up with a
        // naive (Vector3, InputSpace, Vector3, InputSpace) reading -- ApplyCarContactImpulse does
        // `vmr v2, v3` and ApplyWallContactImpulse `vmr128 v125, v3`, i.e. their second Vector3
        // arrives in v3, so at least one further vector-register parameter (very likely a VecFloat)
        // sits between them and is not yet recovered. The INTEGER argument order below is
        // asm-proven; the vector argument list may still grow. No literal tag is invented anywhere.
        void ApplyCarContactImpulse(const Vector3& lvLocalImpulse,
                                    rw::physics::InputSpace leImpulseSpace,
                                    const Vector3& lvContactPosition,
                                    rw::physics::InputSpace lePositionSpace);                                // @0x825D4C10
        void ApplyCrashedContactImpulse(const Vector3& lvLocalImpulse,
                                        rw::physics::InputSpace leImpulseSpace,
                                        const Vector3& lvContactPosition,
                                        rw::physics::InputSpace lePositionSpace,
                                        bool lbZeroResponse);                                                // @0x825D4D50
        void ApplyWallContactImpulse(const Vector3& lvLocalImpulse,
                                     rw::physics::InputSpace leImpulseSpace,
                                     const Vector3& lvContactNormal,
                                     rw::physics::InputSpace lePositionSpace);                               // @0x825FEA18
        void ApplyShowtimeContactImpulse(const Vector3& lvLocalImpulse,
                                         rw::physics::InputSpace leImpulseSpace,
                                         const Vector3& lvContactPosition,
                                         rw::physics::InputSpace lePositionSpace,
                                         bool lbZeroResponse);                                               // @0x825D4E00
        void AddSlam(bool lbTaper, f32 lfDuration, f32 lfSteer, f32 lfRecoveryTime, s8 li8RaceCarId);        // @0x825D4870
        // __fastcall with three VMX128 float args the Hex-Rays signature drops: speed-increase delta,
        // shunt direction (stored verbatim, not normalized), and a Life-register seed splat.
        void AddShunt(f32 lfSpeedIncrease, const Vector3& lvShuntDirection, f32 lfLifeSeed,
                     s8 li8RaceCarId);                                                                       // @0x825FC630
        void UpdateSlam(f32* lpControlsCopy, f32 lfFrameTime);                                               // @0x825D4950
        // ⚠️ RETIRED 2026-08-02: a 4-argument `GetImpulsesFromLocalImpulse(const Vector3&, const
        // Vector3&, Vector3&, Vector3&) const` used to sit here, marked "declare-only (base)". It was
        // not the base's function -- the base's is SIX arguments with the two InputSpace tags above,
        // so this declaration both HID the inherited name and mangled to a symbol nothing defines.
        // The name is now inherited from ExternalPhysicsBody.

        // ===== ADDITIVE GROW (deformation impulse-passing group) =====
        // ⭐⭐ SETTLED 2026-08-09 (crash/shunt wave): the +0x10 slot's role-inferred name
        // `IsIgnoringPassedOnImpulses` was WRONG. Both concrete vtables are now read off the
        // image: the VehiclePhysics/TrafficPhysics vtable @0x820D0C68/@0x820D0C98 carries
        // `li r3,0 ; blr` at +0x10, and the RaceCarPhysics vtable @0x820D1034 carries
        // @0x825D7B68 == RaceCarPhysics::IsPlayerVehicleInShowtime in the same slot. So slot
        // +0x10 IS the DWARF virtual IsPlayerVehicleInShowtime (VehiclePhysics.h:1192), and
        // VehicleRigidBody::RecievePassedOnImpulse's gate reads "do not re-apply passed-on
        // deformation impulses to the player's showtime vehicle". The default body below IS the
        // recovered console default (the ICF-folded return-false), so the old vtable-closure
        // trap in VehiclePhysics.cpp is retired with it. UpdateCrashing @0x82638FC0 dispatches
        // the same slot when it AND-folds the wheels-on-ground run into mbAllWheelsHaveTraction.
        // RaceCarPhysics.h's existing declaration is the override.
        virtual bool IsPlayerVehicleInShowtime() const { return false; }   // vtable +0x10

        // ⭐⭐ VIRTUAL, IMAGE-ATTESTED (crash/shunt wave, 2026-08-09). Vtable slot +0x18 (DWARF
        // VehiclePhysics.h:1204). The base/traffic default @0x82C296C8 is `li r3,1 ; blr` (an
        // ICF fold aliased in the IDB as CgsSound Content::DoOnPostLoad): a crashing vehicle
        // crashes "normally" unless a subclass says otherwise. The RaceCarPhysics vtable
        // carries @0x827E42B8 in this slot (its showtime-aware override, RaceCarPhysics.h:89).
        // UpdateCrashing dispatches it twice: to select the crash damping pair and to gate the
        // down-force + the synthetic-mass regime.
        virtual bool IsCrashingNormally() const { return true; }   // vtable +0x18

        // ⭐⭐ VIRTUAL, IMAGE-ATTESTED (crash/shunt wave, 2026-08-09). Vtable slot +0x28 (DWARF
        // VehiclePhysics.h:1514: `void UpdateAftertouch(const BrnPlayerDriverControls *,
        // const rw::math::vpu::Matrix44Affine *, VecFloat, bool, bool)`). The base/traffic
        // default @0x8284CB38 is `blr` (the ICF-folded empty function) -- traffic cars have no
        // aftertouch. The RaceCarPhysics vtable carries @0x8262EBE8 (the committed
        // RaceCarPhysics::UpdateAftertouch, widened to this 5-arg DWARF form in the same
        // commit -- its asm SAVES v1 at entry, `vmr128 v121, v1` @0x8262EC08, the dropped
        // dt-argument trap again). UpdateCrashing dispatches it under
        // lbPlayerAftertouchForceAdditive.
        virtual void UpdateAftertouch(const BrnPlayerDriverControls* /*lpControls*/,
                                      const rw::math::vpu::Matrix44Affine* /*lpCameraMatrix*/,
                                      VecFloat /*lvfTimeStep*/,
                                      bool /*lbDoForceAdditiveAftertouch*/,
                                      bool /*lbShowtimeAllowed*/) {}   // vtable +0x28

        // ⚠️ NOT A CONSOLE FUNCTION. The layout gate for the two own-member blocks recovered above.
        // It is a STATIC MEMBER so that offsetof() reaches the protected members this class
        // inherits from SimpleVehiclePhysics; it is never called, and its whole body is
        // static_asserts, which fire at COMPILE time -- so /OPT:REF discarding it afterwards is
        // irrelevant. Defined in VehiclePhysics_layout_check.cpp, which IS mounted; see that file's
        // banner for why the gate is console ARITHMETIC and not a host offsetof.
        static void _AssertOwnBlockLayout();
    };

    // ==============================================================================================
    // ⭐⭐ THE CONSOLE ANCHORS THE OWN-BLOCK MAP CLOSES ON (2026-08-03).
    //
    // Every constant here is an X360 ASM LITERAL taken from the map inside VehiclePhysics -- not a
    // computed value. VehiclePhysics_layout_check.cpp re-derives the same numbers by walking the
    // DWARF member order with the asm-literal sub-object sizes and static_asserts that the two
    // agree, at every anchor and at the 0x13F0 end. That is a machine-checked statement of the
    // CONSOLE layout, which is the artifact this wave recovered; it is deliberately NOT an
    // offsetof() gate, because this block is not width-identical on x64 (two pointers) and several
    // embedded sub-types in this tree are reconstructions rather than byte-exact console copies.
    // ==============================================================================================
    namespace X360Layout
    {
        const unsigned KU_VP_MPATTRIBS_OFF        = 0x720u;   // Construct `stw r30,0x720(r31)`
        const unsigned KU_VP_AIATTRIBS_OFF        = 0x730u;   // Construct `addi r3,r31,0x730`
        const unsigned KU_VP_PLAYERATTRIBS_OFF    = 0xAA0u;   // Construct `addi r3,r31,0xAA0`
        const unsigned KU_VP_VEHICLEATTRIBS_SIZE  = 0x370u;   // == the gap between the two above
        const unsigned KU_VP_SPRINGS_OFF          = 0xE10u;   // Construct `addi r29,r31,0xE10`
        const unsigned KU_VP_SPRING_STRIDE        = 0x30u;    // Construct's Prepare loop `addi r29,r29,0x30`
        const unsigned KU_VP_SPRINGMASSSCALERS_OFF= 0xED0u;   // Construct `li r11,0xED0`
        const unsigned KU_VP_SPEEDONLASTCRASH_OFF = 0xEF0u;   // Construct `li r28,0xEF0`
        const unsigned KU_VP_ENGINE_OFF           = 0xF00u;   // Construct `addi r30,r31,0xF00`
        const unsigned KU_VP_ENGINEATTRIBS_SIZE   = 0xA0u;    // Engine::Prepare `memcpy(this,src,0xA0)`
        const unsigned KU_VP_ENGINE_GEAR_OFF      = 0xC0u;    // Engine::Prepare `stw r10,0xC0(r31)`
        const unsigned KU_VP_ENGINE_SIZE          = 0xD0u;    // 0xC0 + 4 + 2 -> 16-round
        const unsigned KU_VP_STEERINGANGLE_OFF    = 0xFE0u;   // GetSteeringAngle `addi r11,r31,0xFE0`
        const unsigned KU_VP_HANDBRAKETIMERS_OFF  = 0x1080u;  // UpdateHandBrake `addi r11,r3,0x1080`
        const unsigned KU_VP_DRIVERTYPE_OFF       = 0x10D4u;  // VehiclePhysics::Prepare `stw r30,0x10D4`
        const unsigned KU_VP_CONTROLS_SIZE        = 0x48u;    // UpdateDriving `memcpy(...,0x48)`
        const unsigned KU_VP_STUCKTIMER_OFF       = 0x10F0u;  // UpdatePlayerStuckInCollisionSpheres
        const unsigned KU_VP_DRIFTFLAGS_OFF       = 0x10F4u;  // ExitDrift `stb r4,0x10F4(r3)`
        const unsigned KU_VP_SLAMEFFECT_OFF       = 0x1100u;  // AddSlam's seven field offsets
        const unsigned KU_VP_SHUNTEFFECT_OFF      = 0x1130u;  // HackedResetAndFlyAround `addi r6,r3,0x1130`
        const unsigned KU_VP_LASTCONTACTED_OFF    = 0x1150u;  // HandleRaceCarRaceCarContact `stb r14,0x1150`
        const unsigned KU_VP_USEDAIRRAMS_OFF      = 0x1158u;  // UpdateAirRam `addi r21,r14,0x1158`
        const unsigned KU_VP_AIRRAMEFFECT_OFF     = 0x1160u;  // AddAirRam
        const unsigned KU_VP_USEDSPINS_OFF        = 0x1220u;  // UpdateSpinEffects `addi r22,r29,0x1220`
        const unsigned KU_VP_SPINEFFECTS_OFF      = 0x1230u;  // UpdateSpinEffects
        const unsigned KU_VP_PREVWORLDVEL_OFF     = 0x1330u;  // Construct `li r6,0x1330`
        const unsigned KU_VP_NORMLINVELMAG_OFF    = 0x1340u;  // UpdateLinearVelocityMagnitude
        const unsigned KU_VP_HASAIR_OFF           = 0x1350u;  // UpdateInAirBehaviour
        const unsigned KU_VP_DRIFTSTATE_OFF       = 0x1352u;  // ExitDrift `stb r5,0x1352(r3)`
        const unsigned KU_VP_NUMCOLLISIONS_OFF    = 0x1354u;  // ApplyCarContactImpulse `lwz`+`stw`
        const unsigned KU_VP_HANDBRAKE_OFF        = 0x1358u;  // UpdateHandBrake `lbz`/`stb`
        const unsigned KU_VP_DEFORMACTIVE_OFF     = 0x1359u;  // SetRaceCarCrashing `stb 1,0x1359`
        const unsigned KU_VP_JUSTBEENSLAMMED_OFF  = 0x135Du;  // AddSlam `stb r10,0x135D`
        const unsigned KU_VP_WEDGED_OFF           = 0x135Fu;  // DoPlayerStuckLineTests
        const unsigned KU_VP_FRONTRAY_OFF         = 0x1360u;  // DoPlayerStuckLineTests
        const unsigned KU_VP_BURNOUT_OFF          = 0x1361u;  // UpdateBurnout
        const unsigned KU_VP_CONTACTINGWALL_OFF   = 0x1362u;  // CheckForGrindingAndRubbing
        const unsigned KU_VP_PREVTRANSFORM_OFF    = 0x1370u;  // GetTransformDelta `addi r11,r4,0x1370`
        const unsigned KU_VP_PITCHYAWROLL_OFF     = 0x13C0u;  // UpdateInAirBehaviour
        const unsigned KU_VP_WHEELFFSPRING_OFF    = 0x13D0u;  // UpdateDriving `stfs 0x13D0`/`0x13D4`
        const unsigned KU_VP_ROLLINGINAIR_OFF     = 0x13D8u;  // UpdateInAirBehaviour `stb 0x13D8`
        const unsigned KU_VP_CARTYPE_OFF          = 0x13DCu;  // VehiclePhysics::Prepare `stw r8,0x13DC`
        const unsigned KU_VP_LASTATTACKER_OFF     = 0x13E0u;  // AddSlam `stb r8,0x13E0`
        const unsigned KU_VP_DEBUGCOMPONENT_OFF   = 0x13E4u;  // ApplySuspensionForces `lwz r11,0x13E4`
        // The end of the own block == the start of the RaceCarPhysics own block, derived by a
        // DIFFERENT wave from a DIFFERENT function. This is the closure.
        const unsigned KU_VP_SIZEOF               = 0x13F0u;
    }
}
}
