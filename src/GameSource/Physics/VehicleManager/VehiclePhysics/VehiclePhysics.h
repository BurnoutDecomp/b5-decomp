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
#include "rw/physics/rigidbody.h"   // rw::physics::InputSpace (AirRamEffect::meImpulseSpace)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<N> (air-ram/spin slot allocators)

namespace CgsNumeric { struct Random; }   // UpdateRoadNoise draws from the shared Random ring (CgsRandom.h)

namespace BrnPhysics
{
    // ----- ADDITIVE GROW (suspension/downforce/weight group, C03): a MINIMAL OWNING SLICE of the
    //       per-wheel 1-D damped suspension spring (BrnPhysics::SuspensionSpring, namespace-scope,
    //       NOT nested -- references/DecFIGS/.../PhysicsUtilities/Spring1D.h:38). The full
    //       SuspensionSpring TU (Construct/Reset/Integrate/Prepare + the ~25 accessors) is owned
    //       elsewhere (Spring1D.cpp / BrnPhysicsUnity). This group bodies the four VehiclePhysics
    //       suspension PHASES that drive the springs, so only the storage (three packed registers,
    //       verbatim DWARF order Spring1D.h:147-149) + the setter/Prepare entry points those phases
    //       call BY NAME are reconstructed here -- declare-only bodies live in the SuspensionSpring
    //       TU (no ODR clash). sizeof == 0x30 (3 * Vector4), matching the maSprings stride. When the
    //       real Spring1D.h lands, REPLACE this slice with an include of the committed type.
    struct SuspensionSpring
    {
        // Spring1D.h:147-149 -- the three packed registers (DWARF-named lanes, kept verbatim).
        Vector4 mvStiffness_Damping_Mass_Position;                  // .x=k .y=c .z=mass .w=position
        Vector4 mvVelocity_Acceleration_DampingForce_SpringForce;   // .x=v .y=a .z=Fdamp .w=Fspring
        Vector4 mvExternalForce;                                    // additive load-transfer force

        // Owned by the SuspensionSpring TU -- declare only every entry point this group calls BY
        // NAME. Args spelled VecFloat per the DWARF (Spring1D.h prototypes).
        void Prepare(VecFloat lrStiffness, VecFloat lrDamping, VecFloat lrMass);   // Spring1D.h:51
        void SetStiffness(VecFloat lvfStiffness);                                  // Spring1D.h:89
        void SetDamping(VecFloat lvfDamping);                                      // Spring1D.h:97
        void SetMass(VecFloat lvfMass);                                            // Spring1D.h:105
        void SetPosition(VecFloat lvfPosition);                                    // Spring1D.h:113
        void SetVelocity(VecFloat lvfVelocity);                                    // Spring1D.h:121
        void SetExternalForce(VecFloat lvfExternalForce);                          // Spring1D.h:191
    };
namespace Vehicle
{
    // The driver-control input block the driving/aftertouch methods read (throttle/brake/steer +
    // button bytes + showtime/aftertouch fields). Defined by a separate VehiclePhysics-input TU;
    // forward-declared here because several method signatures (ModifyControlsForDrift, UpdateDrift,
    // RaceCarPhysics::Update/UpdateAftertouch, ...) take it by pointer/reference.
    class BrnPlayerDriverControls;

    // Forward decl (C11 group): the streamed deformation spec a Prepare consumes by pointer. Full
    // type owned by a deformation TU; only forwarded through, never dereferenced here.
    struct StreamedDeformationSpec;

    // ----- ADDITIVE GROW (aero/downforce group): a MINIMAL OWNING SLICE of the per-car
    //       VehicleAttribs block, reconstructed only far enough for GetDownForce to read its
    //       aerodynamic coefficient. The full VehicleAttribs (masses, wheel positions, tyre/
    //       steering/drift/engine/suspension/boost lanes) is owned by a separate future TU and
    //       has NO committed header yet; when it lands this slice should be REPLACED by an include
    //       of the real type (same as the EngineAttribs minimal slice in Engine.h). Per project
    //       rule the member is pinned BY NAME at its console offset, not reproduced as padding.
    struct VehicleAttribs
    {
        // @+0x70 (BY NAME). The base parameter register
        // (mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce in the full VehicleAttribs). UpdateRoadNoise
        // reads its .z lane as the per-vehicle road-noise scale factor (asm: `lwz mpAttribs,0x720 ;
        // addi r11,r11,0x70 ; lvx128 ; vspltw128 v126,v0,2`). The other lanes belong to the full TU.
        Vector4 mvBaseParams;

        // @+0xB0 (BY NAME). The aero parameter register; GetDownForce reads its .w lane as the
        // per-vehicle downforce/drag coefficient (asm: `lwz mpAttribs ; addi r11,r11,0xB0 ;
        // lvx128 ; vspltw v11,v11,3`). The other lanes belong to the full VehicleAttribs TU.
        Vector4 mvAeroParams;

        // @+0xD0 (BY NAME). The per-vehicle surface-response blend register read by the three
        // GetSurface* accessors (asm: `lwz mpAttribs ; addi r11,r11,0xD0 ; lvx128 ; vspltw`).
        // Lane .x = grip blend (FRONT wheels), .y = grip blend (REAR wheels), .z = roughness
        // scale, .w = linear-drag scale. (mvAeroParams + 0x20 = +0xD0.)
        Vector4 mvSurfaceBlend;

        // ----- ADDITIVE GROW (C06 steering/drift/handbrake group): the per-car STEERING + DRIFT
        //       attrib lanes the drift pipeline samples. Each register is pinned BY NAME at its
        //       console offset inside the per-car VehicleAttribs block (the full type owns the rest).
        //       The exact lane->meaning split inside a register is, where the asm leaves it
        //       ambiguous, FLAGGED; the offset + the lane index the code reads are ASM-exact.

        // @+0xF0 (240, BY NAME). The steering attrib register. GetMaxSteeringAngleDuringDrift reads
        //   lane .x as the max wheel angle (in DEGREES; the body multiplies by deg->rad 0.017453292).
        Vector4 mvSteeringParams;     // FLAG: only lane .x is pinned by use; other lanes belong to full TU

        // @+0x110 (272, BY NAME). The primary drift attrib register. UpdateDriftState reads lane .x
        //   as the slip-angle limit; ApplyDriftLatForce/ApplyDriftYaw/UpdateDriftScale read lane .x.
        Vector4 mvDriftParams0;

        // @+0x120 (288, BY NAME). Drift control register. ModifyControlsForDrift reads lane .w and
        //   lane .z (the drift steer-remap weights); EnterDrift consults it.
        Vector4 mvDriftParams1;

        // @+0x130 (304, BY NAME). ApplyDriftYaw reads lane .x as a yaw gate term.
        Vector4 mvDriftParams2;

        // @+0x140 (320, BY NAME). ApplyDriftLatForce reads lane .y (the lateral-force shaping coeff).
        Vector4 mvDriftParams3;

        // @+0x150 (336, BY NAME). ApplyDriftLatForce / ApplyDriftYaw / ApplyNaturalDriftForces read
        //   lanes .x/.y/.z here (drift force/yaw magnitudes + the natural-yaw clamp).
        Vector4 mvDriftParams4;

        // @+0x160 (352, BY NAME). The drift yaw/push register: lane .x = min-yaw threshold (the
        //   ExitDrift/UpdateDriftState guard), lane .y = drift push-time (the gate in
        //   ApplyNaturalDriftForces / ApplyDriftYaw / MaintainDriftSpeed entry).
        Vector4 mvDriftParams5;

        // @+0x170 (368, BY NAME). ApplyDriftYaw / UpdateDrift read lane .y as a drift damping factor.
        Vector4 mvDriftParams6;

        // @+0x180 (384, BY NAME). ApplyDriftLatForce reads lane .z (the lateral-force final scale).
        Vector4 mvDriftParams7;

        // @+0x280 (640, BY NAME). ApplyCrashedContactImpulse scales the post-contact ANGULAR impulse
        // by lane .y of this register (asm: `*(this+1824)+640 ; vspltw v13,v13,1`). A logical by-name
        // pin (this slice is not offset-faithful); the other lanes belong to the full VehicleAttribs TU.
        Vector4 mvCrashImpulseScale;

        // ----- ADDITIVE GROW (C07 boost/speed-match group): the per-car BOOST attrib sub-block. The
        //       three boost appliers (UpdateBoost / ApplyNormalBoostForce / ApplyBoostKickForce) sample
        //       it via mpAttribs (+0x720) at +0x290..+0x2B0. Reproduced as the DWARF-named nested
        //       VehicleAttribs::BoostAttribs (VehicleAttribs.h:794) so the lane->meaning split is the
        //       authoritative one; pinned BY NAME at its console offset inside the per-car block (the
        //       intervening engine/suspension/collision lanes belong to the full VehicleAttribs TU and
        //       are not reproduced as padding). When the real VehicleAttribs.h lands, REPLACE this slice.
        struct BoostAttribs
        {
            // @+0x290 (BY NAME). lane .x = BoostBase, .y = MaxBoostSpeed (the boost speed cap, read by
            //   UpdateBoost as `MaxBoostSpeed * throttle`), .z = BoostLinearDrag, .w =
            //   NormalBoostHeightOffset (the local offset ApplyNormalBoostForce applies the force at).
            Vector4 mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset;

            // @+0x2A0 (BY NAME). lane .x = NormalBoostAcceleration (sustained-boost accel), .y =
            //   BoostKickMaxStartSpeed (kick eligibility speed ceiling), .z = BoostKickMaxTime, .w =
            //   BoostKickMinTime (the clamp bounds on the computed kick window).
            Vector4 mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime;

            // @+0x2B0 (BY NAME). lane .x = BoostKickAcceleration (the larger kick accel), .y =
            //   BoostKickHeightOffset (the off-centre local offset that turns the kick into a
            //   wheelie/pitch-up torque). Lanes .z/.w unused by this group.
            Vector4 mvBoostKickAcceleration_BoostKickHeightOffset;
        };

        // @+0x290 (BY NAME). The embedded boost attrib sub-block. mpAttribs->mBoostAttribs is read by
        //   the three boost appliers (asm: `lwz mpAttribs,0x720 ; addi r,r,0x290/0x2A0/0x2B0 ; lvx128`).
        BoostAttribs mBoostAttribs;
    };

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
        void Update(s32 a2, const BrnPlayerDriverControls* lpControls, bool lbApplyAftertouch,
                    s32 a5, s32 a6, s32 a7);

        // ----- ADDITIVE GROW (C11 group): the crash master-gate accessors the TrafficPhysics layer
        //       consults. Inherited by SimpleVehiclePhysics in the console layout (mbIsCrashing @
        //       +0x710); on this standalone VehiclePhysics minimal slice they are DECLARE-ONLY,
        //       bodied by the SimpleVehiclePhysics/VehiclePhysics TU. (DWARF BrnSimpleVehiclePhysics.h
        //       :285 IsCrashing / .cpp:805 SetCrashing.) -----
        bool IsCrashing() const;
        virtual void SetCrashing();

        // @0x825D3720 (inlined into UpdateDriving): re-run steering on the engine-only path.
        void UpdateSteering(s8 li8CarType, f32 lfSteer);

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

        // @0x825BE710 / siblings (ExternalPhysicsBody integrator accumulators; accum += arg, guarded).
        void AddWorldSpaceForce(const Vector3& lvForce);
        void AddWorldSpaceImpulse(const Vector3& lvImpulse);
        void AddWorldSpaceAngularImpulse(const Vector3& lvAngularImpulse);
        void AddLocalImpulse(const Vector3& lvLocalImpulse, const Vector3& lvLocalPos);

        // @0x825BE9D0 (ExternalPhysicsBody): the world-space torque accumulator (accum += arg @+0xF0,
        // guarded). DECLARE-ONLY here (owned by the ExternalPhysicsBody base TU) -- the C06 drift yaw
        // (ApplyDriftYaw / ApplyNaturalDriftForces) banks its yaw torque through it. Added BY NAME by
        // the C06 group; the pre-existing AddWorldSpace* siblings above are untouched.
        void AddWorldSpaceTorque(const Vector3& lvTorque);

        // (owned elsewhere): the drift-entry test UpdateDriftState chains into to LATCH a new drift
        // (it sets mu8DriftState + seeds the bank when entry conditions are met). DECLARE-ONLY here --
        // bodied by its own TU (no ODR clash). Added BY NAME by the C06 group.
        void CheckForEnteringDrift(const BrnPlayerDriverControls* lpControls, f32 lfSlipAngle,
                                   f32 lfSpeed, f32 lfTimeStep, f32 lfSteeringDir);

        // @ SimpleVehiclePhysics::AddTractionPoint: record a wheel/surface traction point. This
        // minimal slice has no real SimpleVehiclePhysics base type, so the base entry the
        // RaceCarPhysics override chains into is declared here (DECLARE-ONLY, bodied by the base TU).
        void AddTractionPoint(s32 leWheel, u32 luSurfaceTag);

        // @ ExternallySimulatedBody: the world-space linear velocity (mLinearVelocity, base +0x50).
        const Vector3& GetLinearVelocity() const { return mLinearVelocity; }
        void SetLinearVelocity(const Vector3& lvVelocity) { mLinearVelocity = lvVelocity; }

        // The body world-transform up row (mUpAxis, base +0x20/+0x30 region). The C10 launch builds
        // its push direction from worldUp; provided as a const accessor over the pinned member.
        const Vector3& GetWorldUpRow() const { return mUpAxis; }

        // ----- ADDITIVE GROW (takedown-chain group): the post-slam/shunt wheel-velocity refresh the
        //       car-car contact handler re-runs on both cars after applying a slam/shunt impulse
        //       (X360 VehiclePhysics::SetWheelVelocities). DECLARE-ONLY -- bodied by its own TU. The
        //       X360 passes the slam/shunt velocity in a VMX register; the scalar shape used here is
        //       (the velocity vector). FLAG: arg shape inferred from the call site. -----
        void SetWheelVelocities(Vector3 lvVelocity);

        // ----- ADDITIVE GROW (C11_simple_traffic_attribs group): the three VehiclePhysics entries the
        //       TrafficPhysics layer forwards into. DECLARE-ONLY -- each is bodied by its own (full
        //       VehiclePhysics / driving-spine) TU; declared here so TrafficPhysics::PreparePhysical /
        //       Update resolve them BY NAME under the per-TU gate. Signatures recovered from the
        //       TrafficPhysics call sites (X360 passes the control block + transform by pointer). FLAG:
        //       arg shapes inferred from the forwarding call (the X360 Hex-Rays dropped the full lists).
        bool Prepare(const Matrix44Affine* lpTransform, const StreamedDeformationSpec* lpDeformSpec,
                     VehicleAttribs* lpAttribs, const Vector3* lpWheelPositions,
                     const f32* lpafWheelRadii);
        void UpdateShunt(const BrnPlayerDriverControls* lpControls);
        void UpdateCrashing(f32 lfTimeStep, const BrnPlayerDriverControls* lpControls);

        // ----- ADDITIVE GROW (C04 wheels/tire group): two per-frame wheel-geometry funcs bodied in
        //       VehiclePhysics.cpp (CalculateBodyVelocityAtWheelContact, StoreLocalWheelPositions).
        //       The wheels orchestrator UpdateWheels @0x8261E4F0 and the post-impulse refresh
        //       SetWheelVelocities @0x825FD218 are BLOCKED (un-recoverable degenerate VMX128 +
        //       a dozen un-committed helpers / un-homed rodata) -- see the group notes. -----

        // @0x825FB200: the body velocity at one wheel's contact point:
        //   v_contact = mLinearVelocity + mAngularVelocity x (r_contact - bodyPos)
        // r_contact = the wheel's road-contact position when on the ground, else its streamed
        // position (mStreamedPositionPlusTwistAmount). Stored into maWheels[leWheel].mBodyPointVelocity
        // (+0xA0 within the wheel). (mAngularVelocity is the +0x60 register, here mLocalVelocity.)
        void CalculateBodyVelocityAtWheelContact(EVehicleDrivenWheel leWheel);

        // @0x825B7FC0: project the four wheels' world positions into the body's local frame
        //   local_i = transpose(orthonormal3x3(mTransform)) * (worldWheelPos_i - mTransform.Pos())
        // and store them into maLocalWheelPositions[i] (+0x530/+0x540/+0x550/+0x560). The X360 builds
        // the inverse rotation inline (vmrglw/vmrghw transpose) and FMA-cascades the negated position.
        void StoreLocalWheelPositions();

        // ----- ADDITIVE GROW (C08 airborne/water/freeze/spin group): three more bodies in
        //       VehiclePhysics.cpp. (UpdateInAirBehaviour @0x825D0BE8 and UpdateFreezing @0x825CFD20
        //       are BLOCKED -- heavy un-recoverable VMX damping / un-committed control+vtable surface --
        //       so they are intentionally NOT declared here; see the group's notes.) -----

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

        // ----- ADDITIVE GROW (aero/downforce group) -----
        // @0x825D0840: the aerodynamic down/drag force magnitude, the textbook quadratic
        //   F = 0.5 * rho * CdA * |mLinearVelocity|^2 * coeff
        // where rho (air density) and CdA (drag-area) are process-wide constants lazily cached
        // into g_vAero_Rho / g_vAero_CdA from un-homed .rdata seeds (kAero_Rho_Scalar /
        // kAero_CdA_Scalar), and coeff is the .w lane of mpAttribs->mvAeroParams (@+0xB0). The
        // X360 broadcasts the scalar across a VMX register; here a flat Vector3 with the magnitude
        // in every lane. There is NO speed-curve table -- downforce grows purely with v^2.
        // FLAG (rodata): the rho/CdA seed scalars are un-homed .rdata not present in the function
        // exports; carried as honest flagged-0 placeholders (faithful-but-inert) per project rule
        // -- the formula + offsets are exact, the numeric output stays 0 until the seeds are
        // recovered from the XEX .rdata. NEVER fabricated.
        Vector3 GetDownForce() const;

        // ----- ADDITIVE GROW (surface-response group): the per-surface grip/drag/roughness
        //       lookups. Each reads a 6-bit surface id from a RoadContact CollisionTag
        //       (luSurfaceId = (mCollisionTag.muValue >> 12) & 0x3F -- the X360 reads byte+2 of the
        //       tag then >>4 &0x3F; expressed here against the numeric muValue so it is endian-
        //       independent), indexes a global per-surface property table, and blends with a lane
        //       of mpAttribs->mvSurfaceBlend.
        //       FLAG (runtime data): the per-surface tables (grip unk_82FB8890, drag unk_82FB8BD0,
        //       roughness unk_82FB8DE0, the global roughness scale unk_82FB9220 and the optional
        //       wet/condition multiplier unk_82FB9EC0) are RUNTIME-LOADED scratch globals, not
        //       .rdata in the function exports -- carried as honest flagged-0 placeholders
        //       (faithful-but-inert): the surface-id extraction, the lerp/scale math and the
        //       attrib lanes are EXACT; the looked-up property stays 0 until the tables are
        //       recovered. NEVER fabricated. The debug "properties loaded" / surface-id-bound
        //       asserts are elided (debug-build guards, no effect on output).

        // @0x825D51B8: per-wheel surface grip multiplier. result = 1 - (1 - gripTable[id]) * blend,
        //   blend = mvSurfaceBlend lane .x (FRONT, leWheel<2) or .y (REAR); optional global wet
        //   multiplier when enabled. A lerp toward 1.0 by (1 - blend).
        Vector3 GetSurfaceGrip(EVehicleDrivenWheel leWheel) const;

        // @0x825D5328: per-wheel surface roughness = roughTable[id] * globalRoughScale *
        //   mvSurfaceBlend lane .z. Feeds UpdateRoadNoise.
        Vector3 GetSurfaceRoughness(EVehicleDrivenWheel leWheel) const;

        // @0x825D50A8: vehicle linear-drag from a single representative contact (NOT per-wheel) =
        //   dragTable[id] * mvSurfaceBlend lane .w. Reads the representative-contact tag below.
        Vector3 GetSurfaceLinearDrag() const;

        // ----- ADDITIVE GROW (surface-grip/drag/friction group, C05): the road-noise rumble and the
        //       two per-wheel tyre-friction solvers. Bodies in VehiclePhysics.cpp. -----

        // @0x825F6980: surface roughness -> stochastic rumble accumulated into each grounded wheel.
        // Per frame it draws floats from the shared Random ring (the inlined Random::AddRandomFloatToBuffer
        // LCG, full 64-bit multiplier 0x5851F42D4C957F2D, +1, 8-entry ring) -- one pre-loop draw shared
        // across wheels plus one draw per grounded wheel -- combines them into a small [0,1) noise term,
        // scales by GetSurfaceRoughness(wheel), the per-vehicle road-noise factor (mpAttribs->mvBaseParams
        // .z) and the body-frame speed (mfSpeedMPH @+0x6C0), clamps to 1.0 and accumulates into the
        // wheel's road-noise register. Takes the shared Random by reference (DWARF: UpdateRoadNoise(
        // VecFloat, Random&); the leading VecFloat dt is unused by the rumble math and elided here).
        void UpdateRoadNoise(CgsNumeric::Random& lrRandom);

        // @0x825FB458: the per-axle tyre-friction workhorse -- resolves BOTH wheels of an axle at once
        // in 2-wide SIMD (left lane0 / right lane1). Builds longitudinal & lateral unit directions
        // (Gram-Schmidt), projects contact-relative velocity to slip, multiplies by the tyre grip-curve
        // coefficient and the surface-grip limit, resolves the combined long+lat force inside a friction
        // cone (max 0 / min adhesiveLimit + reciprocal renormalise) with a ~0.85 linear-force cap, then
        // applies each force component as r x F TORQUE via AddWorldSpaceTorque and accumulates the linear
        // residual at +0x240 (scaled by mvfWheelFrictionLinearMultiplier @+0x4048). Wheel-spin reaction
        // feeds back via Wheel::ApplyFrictionReaction; locked/skidding wheels get their long lane masked.
        // FLAG (blocked): the X360 body is a degenerate VMX128 routine ("local variable allocation has
        // failed") with ~200 unnamed stack temps and a dozen un-homed rodata permute/limit vectors whose
        // per-lane semantics are not recoverable store-faithfully; bodied as a structural skeleton, not
        // fabricated math. Signature is the de-SIMD'd shape (the two wheel indices + the per-axle inputs).
        void HandleWheelPairFriction(EVehicleDrivenWheel leWheelA, EVehicleDrivenWheel leWheelB);

        // @0x825D41A8: single-wheel scrub path used ONLY while crashing (asserts IsCrashing() @+0x710).
        // Inactive contacts decay the stored friction by 0.95; active contacts compute slip-driven scrub
        // forces shaped by car-type-selected tunables (mu*CarType @+4308: types 1 & 3 use the harsher
        // {4.0,1.0,0.2} set, others {20.0,...}), applied as r x F torque via AddWorldSpaceTorque +
        // accumulated at +0x240.
        // FLAG (blocked): same degenerate-VMX128 condition as HandleWheelPairFriction -- the active-contact
        // scrub math depends on un-homed rodata limit vectors (unk_82FBA180..82FBA110) and unrecoverable
        // SIMD lane routing; bodied as a structural skeleton. The 0.95 inactive-decay branch IS faithfully
        // recovered (constants are inline immediates).
        void HandleWheelFrictionCrashing(EVehicleDrivenWheel leWheel);

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

        // @0x8262E200: the per-frame drift entry. Refreshes the cached steering direction, runs the
        //   drift state machine (UpdateDriftState), and when drifting (mu8DriftState!=0) applies the
        //   drift forces (ApplyDriftForces); when not drifting, eases the cached drift scale back.
        void UpdateDrift(const BrnPlayerDriverControls* lpOriginalControls, f32 lfTimeStep);

        // @0x8261F728: the drift state machine. CheckForEnteringDrift then a long battery of
        //   ExitDrift guards (slip too small, off-ground too long, no wheels on ground, slip-ratio
        //   below threshold, exit timers, attribs limit, speed too low, steering crossed centre).
        void UpdateDriftState(const BrnPlayerDriverControls* lpControls, f32 lfSlipAngle,
                              f32 lfSpeed, f32 lfSteeringDir);

        // @0x825FA748: grows mDriftScale toward the target slip and applies the natural self-aligning
        //   drift yaw (ApplyNaturalDriftForces). Reads the controls' aftertouch/forced-drift state.
        void UpdateDriftScale(const BrnPlayerDriverControls* lpControls, f32 lfTimeStep, f32 lfSlip);

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
        void ApplyDriftForces(const BrnPlayerDriverControls* lpControls, f32 lfSlipAngle,
                              f32 lfSpeed, f32 lfSteeringDir);

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

        // ----- ADDITIVE GROW (C07): the base-class force entry point the boost appliers call. Owned by
        //       BrnPhysics::ExternalPhysicsBody (a separate TU); declared BY NAME here (no body) so the
        //       boost .cpp can reference it without an ODR clash. The X360 calls it on the body subobject
        //       (`addi r3,this,0x10 ; bl AddLocalForce`). Scalar de-SIMD'd shape: a local-space force at a
        //       local-space position. FLAG: the full console signature carries force/position SPACE enums
        //       (rw::physics::InputSpace) the boost callers pass as small immediates; modelled here as the
        //       (force, localPosition) pair the appliers actually populate. -----
        void AddLocalForce(const Vector3& lvLocalForce, const Vector3& lvLocalPosition);

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

        // @+0x530 (1328, BY NAME): the four wheels' positions in the body's local frame, written by
        // StoreLocalWheelPositions (stvx128 at +1328/+1344/+1360/+1376, stride 16) at the tail of
        // UpdateDriving. Pinned BY NAME at the console offset (the intervening engine/drift/boost
        // state between the wheel array and here is owned by the full TU, not reproduced as padding).
        Vector3 maLocalWheelPositions[eNumDrivenWheels];

        // ----- Vehicle-physics group (class TU): slam/shunt state read by the out-of-line
        //       Is{...}Slamed-or-Shunted predicates (bodied in VehiclePhysics.cpp) -----

        // @+0x111C: remaining time of the in-progress slam impulse (the asm reads `lfs f13,0x111C(r3)`
        // and tests `> 0.0`). Positive while a slam is active. Pinned BY NAME (the ~0x1100 bytes of
        // base + engine/drift/boost state that precede it are not reproduced as padding).
        f32        mfSlamLife;

        // ----- ADDITIVE GROW (C09 group): the standalone slam scalars the slam pipeline touches,
        //       pinned BY NAME flat (consistent with the standalone mfSlamLife above -- the committed
        //       home pins the slam scalars flat, not as an embedded SlamEffect instance). -----
        f32        mfSlamSteering;          // @+0x1114  current slam steering offset (env-shaped)
        f32        mfSlamOriginalSteering;  // @+0x1118  the entry slam steering magnitude
        f32        mfTotalSlamTime;         // @+0x1120  the slam's total duration (envelope denominator)
        f32        mfRecoveryTime;          // @+0x1124  post-slam recovery window
        s8         mi8SlamNumber;           // @+0x1128  chained-slam counter (saturated at 2)
        bool       mbSlamActive;            // @+0x135D  set by AddSlam, cleared by UpdateSlam path

        // @+0x1130: the embedded shunt effect; IsBeingSlamedOrShunted consults mShuntEffect.IsActive()
        // (asm: `addi r3,r3,0x1130 ; bl ShuntEffect::IsActive`). Pinned BY NAME.
        ShuntEffect mShuntEffect;

        // @+0x13E0: the id of the race car currently slamming/shunting this vehicle, used by
        // IsBeingSlamedOrShuntedByRaceCar to filter (asm: `lbz r11,0x13E0(r3) ; extsb` then compared
        // sign-extended against the queried id). Pinned BY NAME.
        s8         mi8SlammingRaceCarId;

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

        // The physics-body world transform, owned by the ExternallySimulatedBody base @+0x10
        // (Matrix44Affine, 64 bytes). GetTransformDelta reads this as the "current" matrix
        // (asm: `addi r10,r4,0x10 ; lvx128` of the four rows). Pinned BY NAME (the vptr +
        // ExternallySimulatedBody/ExternalPhysicsBody preamble that precedes it is not reproduced
        // as padding; the full base is a separate TU).
        Matrix44Affine mTransform;

        // The world-space linear velocity, owned by the ExternallySimulatedBody base @+0x50.
        // UpdateLinearVelocityMagnitude reads this (`addi r9,r3,0x50 ; lvx128`) and normalizes it.
        // Pinned BY NAME. Also the v^2 source for GetDownForce (`lvx128 v0,r4,0x50 ; vmsum3fp128`).
        Vector3        mLinearVelocity;

        // @+0x720: the live per-car attribute/tuning block (player vs AI set is reconciled each
        // frame by Update via SwitchAttribs). GetDownForce reads mpAttribs->mvAeroParams (asm:
        // `lwz r11,0x720(r4)`). Pinned BY NAME (the intervening handling state is not reproduced as
        // padding; mpAttribs points at one of the two embedded VehicleAttribs sets owned by the
        // full VehiclePhysics TU). Typed against the minimal VehicleAttribs slice above.
        const VehicleAttribs* mpAttribs;

        // @+0x596 (BY NAME). The single representative-contact CollisionTag GetSurfaceLinearDrag
        // reads (asm: `*(a2 + 1430) >> 4 & 0x3F`). Unlike grip/roughness this is NOT a per-wheel
        // tag (1430 lies past the 4-wheel array) -- it is a separate vehicle-level contact tag;
        // FLAG: the source member is not named in the DWARF slice, pinned BY NAME with a proposed
        // name. Used only for the linear-drag surface id.
        CollisionTag mRepresentativeContactTag;

        // ----- ADDITIVE GROW (C08 airborne/water/freeze/spin group): the two fields UpdateInWaterBehaviour
        //       reads. Pinned BY NAME at their console offsets. -----

        // @+0x590 (1424): the water-depth scalar UpdateInWaterBehaviour tests (`*(this+1424) <
        // flt_82F2A4E4` -> deep enough to drown). FLAG: the source member is not named in the DWARF
        // slice; pinned BY NAME with a proposed name at the asm offset. (Read as a scalar float.)
        f32          mfWaterDepth;

        // @+0x594 (1428): the contact CollisionTag whose surface id (`(>>4)&0x3F`) UpdateInWaterBehaviour
        // looks up in the water-surface table (byte_82FB7DF4[id]). FLAG: proposed name at the asm
        // offset; distinct from mRepresentativeContactTag (+0x596).
        CollisionTag mWaterContactTag;

        // @+0x6A4: the car's vertical extent used by GetCarGroundDistanceCheck when the car is
        // inverted (read as a scalar float: `lfs f13,0x6A4(r3)`). Pinned BY NAME (the intervening
        // attribs/spring/state block is not reproduced as padding). FLAG: the multiplier the asm
        // applies to it (flt_82001D9C) is un-homed .rdata -- see the body's KF_* constant.
        f32        mfCarGroundCheckExtent;

        // @+0x6C0 (1728): the cached body-frame speed in MPH (mfSpeedMPH; recomputed each frame at the
        // tail of UpdateDriving = dot3(mTransform forward axis, mLinearVelocity) x 2.2369363).
        // UpdateRoadNoise reads it (`li r24,0x6C0 ; lvx128 v124,r29,r24`) so the rumble scales with
        // speed. UpdateBoost reads it as the boost speed (`li r6,0x6C0 ; lvx128 v12,r31,r6`) for the
        // 5.0-mph floor + the MaxBoostSpeed cap + the BoostKickMaxStartSpeed eligibility test. Pinned
        // BY NAME (the intervening state block is not reproduced as padding).
        f32        mfSpeedMPH;

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

        // @+0x580 (1408): the ground contact normal the drift forces tangent-project against (so drift
        //   never pushes into/off the road). ApplyDriftLatForce/ApplyDriftYaw/MaintainDriftSpeed read it
        //   (`lvx128 r31,1408 ; vmsum3fp128`). Pinned BY NAME. FLAG: source member name inferred (the
        //   DWARF slice for the above-ground-test block is not in this excerpt).
        Vector3 mGroundNormal;

        // @+0x598 (1432): mAboveGroundTestResult.mbValid -- the per-frame "are we above the road" flag
        //   gating each drift force (asm `*(this+1432)` + the elided "mAboveGroundTestResult.mbValid"
        //   assert). Pinned BY NAME. FLAG: only the mbValid byte of the result struct is pinned here.
        bool mbAboveGroundTestValid;

        // ----- ADDITIVE GROW (C07 boost/speed-match group): the boost-state register + boost-kick flag +
        //       the box half-extent the boost appliers use as the local force-application point. -----

        // @+0x6A0 (1696, BY NAME). The body-space box half-extent (SimpleVehiclePhysics::mHalfExtent per
        // the findings doc). Both boost appliers read its .z lane, NEGATE it, and use it as the z of the
        // local force-application point (so the boost force is applied behind the centre of mass).
        // FLAG: the source member is a base-class SimpleVehiclePhysics field not in the committed slice;
        // pinned BY NAME at its console offset with the findings-doc name.
        Vector3    mHalfExtent;

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

        // @+0x100 (256, BY NAME) and @+0x120 (288, BY NAME). The linear/angular impulse-accumulator rows
        // ApplyBoostKickForce damps (alongside mLocalVelocity @+0x60) to self-limit the kick wheelie:
        // once the pitch exceeds the max-wheelie angle, each row's component along the body right axis
        // (mTransform.xAxis) is bled out by kfWheelieLimitDamping. Owned by the ExternalPhysicsBody base
        // (findings: +0x100 = linear impulse accumulator, +0x110/+0x120 = angular); pinned BY NAME at
        // their console offsets. FLAG: the precise base-member spellings live in the base TU.
        Vector3    mLinearImpulseAccumulator;     // +0x100
        Vector3    mAngularImpulseAccumulatorRow; // +0x120

        // @+0x135B (4955, BY NAME). The "all driven wheels have traction" gate the boost appliers and
        // UpdateSpeedMatch consult before applying force (`lbz r11,0x135B(r3) ; cmplwi ; beq`). Pinned BY
        // NAME (it sits in the drift/crash byte-flag block; mbHandBrake/mu8DriftState are nearby).
        bool       mbAllWheelsHaveTraction;

        // @+0x1340: cached normalized linear velocity (xyz lanes) + speed magnitude (w/"plus" lane),
        // written by UpdateLinearVelocityMagnitude (`addi r10,r3,0x1340 ; stvx128`) and read by
        // GetLinearVelocityDirection / GetLinearVelocityMagnitude. Pinned BY NAME.
        Vector3Plus mNormLinearVelocityMag;

        // @+0x1370: the previous-frame physics transform, the "from" matrix of GetTransformDelta
        // (asm: `addi r11,r4,0x1370 ; lvx128` of the four rows, whose orthonormal 3x3 is inverted
        // inline). Pinned BY NAME.
        Matrix44Affine mPreviousTransform;

        // ===== ADDITIVE GROW (C03 suspension/downforce/weight group) =====
        // @+0xE10 (3600), stride 0x30: the four 1-D damped suspension springs (one per driven wheel).
        // SuspensionSpring is the already-committed namespace-scope slice (sizeof 0x30 = 3*Vector4).
        SuspensionSpring maSprings[eNumDrivenWheels];

        // @+0xEE0 (3808)/@+0xEE8 (3816): the dynamic load-transfer force CalculateWeightTransfer builds
        // and distributes to the springs (SetExternalForce). UpdateSuspension mirrors the +0x50 weight
        // register to a +4912 snapshot first. FLAG: the member names + the +4912 mirror are inferred.
        Vector3 mWeightTransfer;        // +0xEE0 (packed; .x/.y/.z load-transfer per body axis)
        Vector4 mWeightTransferMirror;  // +0x1330 (4912): the +0x50 weight register snapshot

        // ----- C03 suspension phases (bodies in VehiclePhysics.cpp) -----
        void SetupSuspension(f64 lfTimeStep);               // @0x825CF718 BLOCKED (VMX permute scatter)
        void ApplyWheelWeight();                            // @0x825F7898 PARTIAL
        void CalculateWeightTransfer();                     // @0x825F9DD0 PARTIAL (units 0.10193679)
        void ApplySuspensionForces();                       // @0x825D1EE8 PARTIAL
        void UpdateSuspension(f64 lfTimeStep);              // @0x8261F698 CLEAN (the virtual spine)
        void UpdateSuspensionSprings();                     // @0x825F7AF0 BLOCKED (degenerate VMX)
        void UpdateSuspensionPostSimulation();              // @0x825F6BB0 BLOCKED (degenerate VMX giant)
        void StabiliseAfterHardLanding();                   // @0x825D1890 PARTIAL (powf settle blocked)

        // ===== ADDITIVE GROW (C09 crash/contact-impulse group) =====
        // The contact-impulse handlers + the slam enqueue/tick. (SetCrashing override + UpdateShunt +
        // UpdateCrashing are already declared above / left declare-only.) The base off-centre-impulse
        // kernel GetImpulsesFromLocalImpulse is declare-only (owned by ExternalPhysicsBody).
        void ApplyCarContactImpulse(const Vector3& lvLocalImpulse, const Vector3& lvContactPosition);        // @0x825D4C10
        void ApplyCrashedContactImpulse(const Vector3& lvLocalImpulse, const Vector3& lvContactPosition,
                                        bool lbZeroResponse);                                                // @0x825D4D50
        void ApplyWallContactImpulse(const Vector3& lvLocalImpulse, const Vector3& lvContactNormal,
                                     bool lbContactPositionNotWorldSpace);                                   // @0x825FEA18
        void ApplyShowtimeContactImpulse(const Vector3& lvLocalImpulse, const Vector3& lvContactPosition,
                                         bool lbZeroResponse);                                               // @0x825D4E00
        void AddSlam(bool lbTaper, f32 lfDuration, f32 lfSteer, f32 lfRecoveryTime, s8 li8RaceCarId);        // @0x825D4870
        void AddShunt(s8 li8RaceCarId);                                                                      // @0x825FC630
        void UpdateSlam(f32* lpControlsCopy, f32 lfFrameTime);                                               // @0x825D4950
        void GetImpulsesFromLocalImpulse(const Vector3& lvLocalImpulse, const Vector3& lvContactPosition,
                                         Vector3& lrJWorld, Vector3& lrAngularJWorld) const;  // declare-only (base)
    };
}
}
