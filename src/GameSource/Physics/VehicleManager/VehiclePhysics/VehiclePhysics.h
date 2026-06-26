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
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, Vector4
#include "types.hpp"          // f32, s8
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"   // Wheel + Wheel::RoadContact

namespace BrnPhysics
{
namespace Vehicle
{
    // Minimal VehiclePhysics: only the nested SlamEffect / ShuntEffect are reconstructed.
    struct VehiclePhysics
    {
        // The four driven wheels, in the order GetHeightAboveRoad iterates them (DWARF
        // VehiclePhysics.h:106 EVehicleDrivenWheel). Front-left, front-right, rear-left, rear-right.
        enum EVehicleDrivenWheel
        {
            eFrontLeftWheel = 0,
            eFrontRightWheel,
            eRearLeftWheel,
            eRearRightWheel,
            eNumDrivenWheels
        };

        // Accessor for one driven wheel (DWARF VehiclePhysics.h:244). GetHeightAboveRoad reads
        // each wheel's road-contact result through this.
        const Wheel& GetWheel(EVehicleDrivenWheel leWheel) const { return maWheels[leWheel]; }

        // The vehicle "up" axis used to decide whether a wheel's road contact is on the ground
        // (the asm's lvx at _R4+0x20 + vmsum3fp128 dot vs 0.5). Pinned BY NAME.
        const Vector3& GetUpAxis() const { return mUpAxis; }

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

        // ----- ADDITIVE GROW (takedown-chain group): the post-slam/shunt wheel-velocity refresh the
        //       car-car contact handler re-runs on both cars after applying a slam/shunt impulse
        //       (X360 VehiclePhysics::SetWheelVelocities). DECLARE-ONLY -- bodied by its own TU. The
        //       X360 passes the slam/shunt velocity in a VMX register; the scalar shape used here is
        //       (the velocity vector). FLAG: arg shape inferred from the call site. -----
        void SetWheelVelocities(Vector3 lvVelocity);

        // ----- Vehicle-physics group (class TU): three VMX128 funcs lowered to faithful scalar
        //       (bodies in VehiclePhysics.cpp) -----

        // @0x825C0100: a "ground distance" check used by UpdateInAirStats. Normally 0.5; but when
        // the car is inverted (the vehicle up axis points down, mUpAxis.y < 0 -- the asm splats the
        // up-axis .y lane and tests `0 > up.y` via vcmpgtfp.) it returns
        // `mfCarGroundCheckExtent * KF_CAR_GROUND_DISTANCE_INVERTED_SCALE + 0.5`, accounting for the
        // car's own vertical extent when it is upside down. Spelled f64 to match the X360 ABI (the
        // value comes back in f1 as a double; the source return type is float32_t).
        f64 GetCarGroundDistanceCheck() const;

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
        //   * a one-time static feature gate (the X360 computes a cached splat constant compared
        //     against zero -> a build/tunable enable). Carried as KB_COUNTER_STEER_LOW_SPEED_ENABLED.
        //   * requires speed above a small threshold (asm: a3 <= 0.2 -> false).
        //   * if the lateral velocity component (mLocalVelocity.y) is clearly positive (> 0.5) and
        //     steering is not strongly opposite (>= -0.1) -> counter-steering: true.
        //   * else if that component is below the same band and steering is mild (<= 0.1) -> true.
        //   * otherwise false.
        // lfSteering = the steering input lane (f1 / a2); lfSpeed = the speed lane (f2 / a3).
        //
        // FLAG (rodata): the three comparison literals (0.2 / -0.1 / 0.1 / 0.5) are the values the
        // X360 decompiler resolved for the inlined float constants; the feature-gate constant
        // (flt_8208F9D4 splat, lazily cached at unk_82FB9FC0) is un-homed rodata and is carried as
        // an honest enable flag rather than a fabricated numeric value.
        bool IsCounterSteeringAtLowSpeed(f32 lfSteering, f32 lfSpeed) const
        {
            // One-time static feature gate (lazy-cached on X360). Honest placeholder: enabled.
            static const bool KB_COUNTER_STEER_LOW_SPEED_ENABLED = true;   // FLAG: un-homed rodata
            if (!KB_COUNTER_STEER_LOW_SPEED_ENABLED || lfSpeed <= 0.2f)
                return false;

            const f32 lfLateral = mLocalVelocity.y;   // lane 1 of the velocity register at +0x60

            if (lfLateral > 0.5f && lfSteering >= -0.1f)
                return true;

            if (!(0.5f > lfLateral))                  // lfLateral >= 0.5 -> not counter-steering
                return false;

            return lfSteering <= 0.1f;
        }

        // The local-space velocity register read by IsCounterSteeringAtLowSpeed (its .y lane). The
        // full VehiclePhysics owns many members preceding this; per project rule the absolute
        // console offset (this+0x60) is pinned BY NAME, not reproduced as padding before it.
        Vector3 mLocalVelocity;

        // The vehicle up axis + the driven-wheel array read by RaceCarPhysics::GetHeightAboveRoad.
        // The full VehiclePhysics interleaves these among many other members; per project rule the
        // absolute console offsets (maWheels @ +0x130 stride 0xE0, up axis @ +0x20) are pinned BY
        // NAME, not reproduced as padding.
        Vector3 mUpAxis;
        Wheel   maWheels[eNumDrivenWheels];

        // ----- Vehicle-physics group (class TU): slam/shunt state read by the out-of-line
        //       Is{...}Slamed-or-Shunted predicates (bodied in VehiclePhysics.cpp) -----

        // @+0x111C: remaining time of the in-progress slam impulse (the asm reads `lfs f13,0x111C(r3)`
        // and tests `> 0.0`). Positive while a slam is active. Pinned BY NAME (the ~0x1100 bytes of
        // base + engine/drift/boost state that precede it are not reproduced as padding).
        f32        mfSlamLife;

        // @+0x1130: the embedded shunt effect; IsBeingSlamedOrShunted consults mShuntEffect.IsActive()
        // (asm: `addi r3,r3,0x1130 ; bl ShuntEffect::IsActive`). Pinned BY NAME.
        ShuntEffect mShuntEffect;

        // @+0x13E0: the id of the race car currently slamming/shunting this vehicle, used by
        // IsBeingSlamedOrShuntedByRaceCar to filter (asm: `lbz r11,0x13E0(r3) ; extsb` then compared
        // sign-extended against the queried id). Pinned BY NAME.
        s8         mi8SlammingRaceCarId;

        // ----- Vehicle-physics group (class TU): members read/written by the three VMX128 funcs
        //       (GetCarGroundDistanceCheck / GetTransformDelta / UpdateLinearVelocityMagnitude) -----

        // The physics-body world transform, owned by the ExternallySimulatedBody base @+0x10
        // (Matrix44Affine, 64 bytes). GetTransformDelta reads this as the "current" matrix
        // (asm: `addi r10,r4,0x10 ; lvx128` of the four rows). Pinned BY NAME (the vptr +
        // ExternallySimulatedBody/ExternalPhysicsBody preamble that precedes it is not reproduced
        // as padding; the full base is a separate TU).
        Matrix44Affine mTransform;

        // The world-space linear velocity, owned by the ExternallySimulatedBody base @+0x50.
        // UpdateLinearVelocityMagnitude reads this (`addi r9,r3,0x50 ; lvx128`) and normalizes it.
        // Pinned BY NAME.
        Vector3        mLinearVelocity;

        // @+0x6A4: the car's vertical extent used by GetCarGroundDistanceCheck when the car is
        // inverted (read as a scalar float: `lfs f13,0x6A4(r3)`). Pinned BY NAME (the intervening
        // attribs/spring/state block is not reproduced as padding). FLAG: the multiplier the asm
        // applies to it (flt_82001D9C) is un-homed .rdata -- see the body's KF_* constant.
        f32        mfCarGroundCheckExtent;

        // @+0x1340: cached normalized linear velocity (xyz lanes) + speed magnitude (w/"plus" lane),
        // written by UpdateLinearVelocityMagnitude (`addi r10,r3,0x1340 ; stvx128`) and read by
        // GetLinearVelocityDirection / GetLinearVelocityMagnitude. Pinned BY NAME.
        Vector3Plus mNormLinearVelocityMag;

        // @+0x1370: the previous-frame physics transform, the "from" matrix of GetTransformDelta
        // (asm: `addi r11,r4,0x1370 ; lvx128` of the four rows, whose orthonormal 3x3 is inverted
        // inline). Pinned BY NAME.
        Matrix44Affine mPreviousTransform;
    };
}
}
