#pragma once

// BrnPhysics::Vehicle::AboveGroundTestResult -- the above-ground (down-ray) test result
// embedded by value in RaceCarState (and in SimpleVehiclePhysics). Namespace-scope struct,
// NOT nested, per references/DecFIGS/dwarfdump/.../BrnSimpleVehiclePhysics.h (line :71).
//
// Only this result struct is reconstructed; the SimpleVehiclePhysics class that also owns
// it (and its methods Reset/SetFrom/SetValidResult) is a separate future TU. Those methods
// are declared-only here so that TU can define them later without an ODR clash.
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, Matrix44Affine, CollisionTag, VecFloat
#include "types.hpp"          // f32, u16
#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"   // BrnPhysics::ExternalPhysicsBody (base)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"    // BrnPhysics::Vehicle::Wheel (full 0xE0 layout)

namespace BrnPhysics
{
namespace Vehicle
{
    // Forward decl: the per-car tuning block (full type owned by the VehicleAttribs TU).
    class VehicleAttribs;

    // DWARF BrnSimpleVehiclePhysics.h:52 -- the driven-wheel index enum used across the vehicle
    // physics. Reproduced here for the shared vocabulary (the bodies index maWheels by it).
    enum EVehicleDrivenWheel
    {
        eFrontLeftWheel  = 0,
        eFrontRightWheel = 1,
        eRearLeftWheel   = 2,
        eRearRightWheel  = 3,
        eNumDrivenWheels = 4,
    };

    struct AboveGroundTestResult
    {
        Vector3      mIntersectionPosition;
        Vector3      mIntersectionNormal;
        f32          mfVerticalDistance;
        CollisionTag mCollisionTag;
        bool         mbValid;

        // Owned by the BrnSimpleVehiclePhysics TU -- declare only (no body).
        void Reset();
        void SetFrom(Vector3, const AboveGroundTestResult*);
        void SetValidResult(Vector3, Vector3, f32, u16, u16);
    };

    // SimpleVehicleAttribs -- the per-car physics attribute block (DecFIGS DWARF
    // VehicleAttribs.h:1831). MINIMAL OWNING SLICE (flagged): the full type carries masses,
    // wheel positions and tyre attribs and is owned by the VehicleAttribs TU. This group only
    // reads ->IsValid() and ->mCOMOffset, so only those are reconstructed here. The future
    // VehicleAttribs TU MUST GROW this home ADDITIVELY (add the remaining members in their
    // DWARF sequence) rather than redefine it -- do NOT fork. mCOMOffset / mbIsValid /
    // IsValid() match the DWARF (:1861/:1869/:1879). IsValid() body is the trivial getter.
    struct SimpleVehicleAttribs
    {
        Vector3 mCOMOffset;   // :1861
        bool    mbIsValid;    // :1869
        bool    IsValid() const { return mbIsValid; }   // :1879

        // Owned by the SimpleVehicleAttribs TU -- declared only (no body here) so SimpleVehiclePhysics
        // ::Construct can call it BY NAME without an ODR clash. (DWARF: SimpleVehicleAttribs::Construct
        // zeroes the attribs block + sets mbIsValid false.)
        void Construct();
    };

    // BrnPhysics::Vehicle::SimpleVehiclePhysics -- the lightweight ("simple") vehicle physics
    // body used for traffic/secondary cars. Derives from ExternalPhysicsBody (which derives
    // from ExternallySimulatedBody, the owner of mTransform / mAngularVelocity). Member
    // SEQUENCE verbatim from the DecFIGS DWARF (BrnSimpleVehiclePhysics.h:357-373).
    //
    // ADDITIVE GROW (flagged by BrnPhysics-bodies group): only this group's 3 ledger funcs are
    // bodied (the two graphics-transform funcs + IsContactBelowWheelPlane). The remaining ~50
    // methods listed in the DWARF (Construct/Prepare/SwitchAttribs/SetAttributes/wheel & traction
    // accessors/...) are a separate future TU and are NOT declared here. The embedded sibling
    // types (Wheel, SweptSphere, SimpleVehicleAttribs, AxisAlignedBox) are their own TUs; the
    // ones whose storage precedes the funcs' members are reconstructed minimally so the members
    // this group touches resolve BY NAME. POINTER/SIZE DIVERGENCE: the X360 absolute offsets
    // (mTransform @+16, mSimpleAttribs.mCOMOffset @+1648, mbMinWheelDistValid @+1812,
    // mWheelPlanePosAndHeight @+1712) assume the console sub-type sizes; per project rule we pin
    // BY NAME + SEQUENCE and do NOT static_assert absolute leaf offsets.
    // SweptSphere -- one wheel's swept-collision sphere. MINIMAL OWNING SLICE (flagged): the full
    // type (centre/radius/sweep history) is owned by a collision TU; this group's bodied funcs only
    // need it to occupy the maWheelSpheres[4] storage in DWARF sequence, so a 0x20 opaque slice is
    // carried BY NAME. When the collision TU lands this should be REPLACED by the committed type.
    struct SweptSphere
    {
        u8 mData[0x20];   // opaque (flagged): real fields owned by a collision TU
    };

    // AxisAlignedBox -- the deformable/original collision AABB (min,max). MINIMAL OWNING SLICE
    // (flagged): full type owned by a geometry TU; carried as min/max Vector3 BY NAME so the two
    // AABB members lay out in DWARF sequence.
    struct AxisAlignedBox
    {
        Vector3 mMin;
        Vector3 mMax;
    };

    class SimpleVehiclePhysics : public ExternalPhysicsBody
    {
    public:
        // @0x825BF158: graphics transform = physics transform with the centre-of-mass offset
        // removed from the translation (the physics body pivots about the COM; the graphics
        // mesh pivots about the model origin). Asserts the attribs are valid and both the source
        // mTransform and the produced lTransform are finite.
        Matrix44Affine GetGraphicsVehicleTransform() const;

        // @0x825BF618: inverse of the above -- store a graphics transform back as the physics
        // transform, ADDING the COM offset back into the translation. Asserts the incoming
        // graphics transform is finite.
        void SetGraphicsVehicleTransform(Matrix44Affine lTransform);

        // @0x825BF870: true when a contact point lies below the wheel plane, within a vertical
        // threshold (projected on the vehicle up axis = mTransform.yAxis). Returns false early
        // when the wheel plane has not been computed (mbMinWheelDistValid == false).
        bool IsContactBelowWheelPlane(Vector3 lvContactPoint, VecFloat lvfThreshold) const;

        // Attribs accessor (DWARF :252/:256): returns &mSimpleAttribs. Inlined here so this
        // group's transform funcs resolve it BY NAME without depending on the future TU.
        const SimpleVehicleAttribs* GetSimpleAttribs() const { return &mSimpleAttribs; }
        SimpleVehicleAttribs*       GetSimpleAttribs()       { return &mSimpleAttribs; }

        // ----- ADDITIVE GROW (C11_simple_traffic_attribs group): the SimpleVehiclePhysics body
        //       set. The leading sub-type members (maWheels/maWheelSpheres/maLocalTractionPoints/
        //       mAboveGroundTestResult) that the DWARF places before mSimpleAttribs are now laid out
        //       BY NAME below so these bodies resolve their members without raw-offset casts. -----

        // @0x826203E8: construct -- base Construct, Wheel::Clear each of the 4 wheels,
        // SimpleVehicleAttribs::Construct, zero mHandlingBodyOffset/mHalfExtent/the two AABBs, seed
        // the crash/deform bools, then Reset. Bodied in BrnSimpleVehiclePhysics.cpp.
        void Construct();

        // @0x826206D0: destruct -- base Destruct, Wheel::Clear loop, Reset. Bodied below.
        void Destruct();

        // @0x825D9A58: reset -- (gated by mbStartedDeforming==0) Wheel::Reset each wheel, zero
        // maLocalTractionPoints[0..3] (+0x530 stride 16) + mfSpeedMPH, clear the crash bools.
        // Bodied below. ⚠️ The "velocity/transform-delta SIMD registers" reading of the four
        // vector stores was WRONG and is corrected in the .cpp banner (2026-08-03).
        void Reset();

        // @0x82602880: stamp the above-ground (down-ray) test result from a position+normal+two
        // collision-tag halfwords. Asserts the position/normal are finite (debug). Bodied below.
        void SetAboveGroundTestResult(Vector3 lvPosition, Vector3 lvNormal,
                                      u16 lu16TagHi, u16 lu16TagLo);

        // @0x825B8EA8: clear the crash master flag (mbCrashing) + mbStartedFatallyCrashing. Bodied.
        // Virtual in the DWARF (BrnSimpleVehiclePhysics.cpp:786).
        virtual void ClearCrashing();

        // DWARF BrnSimpleVehiclePhysics.h:285 -- the crash master-gate getter (reads mbCrashing).
        // Trivial inline; TrafficPhysics::Update consults it before arming SetCrashing.
        bool IsCrashing() const { return mbCrashing; }

        // ----- The following SimpleVehiclePhysics methods are part of this group's ledger but are
        //       BLOCKED (fidelity:blocked): they are deep VMX128 routines whose math touches per-wheel
        //       swept-sphere / reciprocal-mass / box-inertia internals that cannot be faithfully
        //       reproduced BY NAME (their pseudocode is the degenerate "local variable allocation has
        //       failed" form and/or depends on un-homed rodata / sub-types owned by other TUs). They
        //       are DECLARED here (so the class is complete) and their structural skeletons are in the
        //       group's flags -- no fabricated math is committed. -----
        bool Prepare(Matrix44Affine lTransform, Vector3 lLinearVelocity, Vector3 lAngularVelocity,
                     Vector3 lHandlingBodyOffset, Vector3 lHalfExtent, const AxisAlignedBox& lrAABB,
                     VehicleAttribs* lpAttribs, const Vector3* lpWheelPositions,
                     const f32* lpafWheelRadii);
        void SwitchAttribs(VehicleAttribs* lpAttribs);
        bool SetAttributes(VehicleAttribs* lpAttribs, const Vector3* lpWheelPositions,
                           const f32* lpafWheelRadii);
        void AddTractionPoint(EVehicleDrivenWheel leWheel, Vector3 lvPosition, Vector3 lvNormal,
                              u32 lu32CollisionTag);
        Matrix44Affine GetWheelsWorldTransfrom(EVehicleDrivenWheel leWheel, bool lbApplySteer) const;
        void GetSimpleVehicleBox(/* Box& */ void* lpOutBox) const;
        void CalculateNewWheelPlane();
        virtual void SetCrashing();

    protected:
        // ==========================================================================================
        // ⭐⭐ THE OWN-MEMBER BLOCK, SEATED 2026-08-03 (VehiclePhysics own-block wave).
        //
        // Member SEQUENCE is the DecFIGS DWARF's (BrnSimpleVehiclePhysics.h:357-373). Every OFFSET
        // in the right-hand column is an X360 ASM LITERAL or is forced with zero slack between two
        // asm literals. The two derivations know nothing about each other, and the block CLOSES:
        // its last data byte is 0x715, which 16-rounds to **0x720 == 1824**, and 0x720 is exactly
        // where `VehiclePhysics::Construct` @0x8262DC8C puts `mpAttribs` (`stw r30, 0x720(r31)`).
        //
        // ⭐ THE FRAME. `SimpleVehiclePhysics::Construct` @0x82620400 calls
        // `ExternalPhysicsBody::Construct(this + 0x10)`, so the leaf's vptr occupies +0x00 and the
        // 16-aligned base sub-object starts at +0x10. Laid out from the committed base headers the
        // chain ends at +0x130 -- the asm-literal `addi r3, r31, 0x130` this Construct then uses as
        // &maWheels[0]. That closure independently re-derives Matrix33 == 48 and
        // sizeof(ExternalPhysicsBody) == 0x120:
        //     +0x10 mTransform(64)  +0x50 mLinearVelocity  +0x60 mAngularVelocity  +0x70 mbFrozen
        //     +0x80 mLocalInverseInertia(48)  +0xB0 mWorldInverseInertia(48)  +0xE0 mfMass
        //     +0xF0 mTotalLinearForce  +0x100 mTotalTorque  +0x110 mTotalLinearImpulse
        //     +0x120 mTotalAngularImpulse                                    -> ends 0x130
        //
        // ⚠️ HOST DIVERGENCE, stated once. The leaf vptr widens 4 -> 8 on x64 but the base is
        // 16-aligned either way, so the base chain does NOT drift. The sub-types embedded below,
        // however, are MINIMAL OWNING SLICES in this tree (SweptSphere is an opaque 0x20 --
        // correct, see the closure note on it -- but SimpleVehicleAttribs is 20 bytes here against
        // the console's 240, and Wheel/AxisAlignedBox are their own reconstructions). So the
        // absolute console offsets below are NOT reproducible as host offsetofs and are NOT
        // static_asserted. What IS gated (BrnSimpleVehiclePhysics_layout_check.cpp) is the DWARF
        // ORDER and every pointer-free RELATIVE run.
        //
        //   X360   member                       first-hand evidence
        //   -----  ---------------------------  ------------------------------------------------
        //   0x130  maWheels[4]  stride 0xE0     Construct `addi r3,r31,0x130` + Wheel::Clear loop
        //                                       `addi r3,r3,0xE0`, 4 iterations
        //   0x4B0  maWheelSpheres[4]            closure: 0x530-0x4B0 == 128 == 4 x 32, so
        //                                       sizeof(SweptSphere) == 0x20 -- the committed
        //                                       opaque slice's size is CORRECT, not a guess
        //   0x530  maLocalTractionPoints[4]     Reset's zero loop (+0x530, stride 16)
        //   0x570  mAboveGroundTestResult       Construct `addi r11,r31,0x570`, then +0x00 pos,
        //                                       +0x10 normal, +0x20 mfVerticalDistance,
        //                                       +0x24/+0x26 the two CollisionTag halfwords,
        //                                       +0x28 mbValid -- the sub-struct's own DWARF order
        //   0x5A0  mSimpleAttribs (240)         Construct `addi r3,r31,0x5A0 ; bl
        //                                       SimpleVehicleAttribs::Construct`
        //   0x690  mHandlingBodyOffset          Construct `li r9,0x690 ; stvx128 v1,r31,r9`
        //   0x6A0  mHalfExtent                  Construct `li r8,0x6A0 ; stvx128 v1,r31,r8`
        //   0x6B0  mWheelPlanePosAndHeight      (bracketed, zero slack)
        //   0x6C0  mfSpeedMPH                   UpdateHandBrake `li r7,0x6C0 ; lvx128 v13,r3,r7`;
        //                                       AddSlam `li r10,0x6C0`
        //   0x6D0  mDeformableAABB (32)         ⭐ VehicleManager::SetRaceCarCrashing
        //   0x6F0  mOriginalAABB   (32)         @0x82635438-0x82635468 copies 32 bytes FROM +0x6F0
        //                                       TO +0x6D0 -- ResetDeformableAABB() inlined. Pins
        //                                       both AABBs and sizeof(AxisAlignedBox) == 32.
        //   0x710  mbCrashing                   the console's own assert string:
        //                                       FireAssert("mbCrashing", ".../RaceCarPhysics.h", 328)
        //   0x711  mbStartedFatallyCrashing     DWARF order
        //   0x712  mbStartedDeforming           DWARF order
        //   0x713  mbCrashedThisFrame           DWARF order
        //   0x714  mbMinWheelDistValid          previously committed (gates IsContactBelowWheelPlane)
        //   0x715  mbAnyWheelsDetatched         DWARF order (last)
        //                                       -> 0x716 rounds to **0x720 == 1824**
        // ==========================================================================================
        Wheel                maWheels[eNumDrivenWheels];              // :357  @0x130 stride 0xE0
        SweptSphere          maWheelSpheres[eNumDrivenWheels];        // :358  @0x4B0 stride 0x20
        Vector3              maLocalTractionPoints[eNumDrivenWheels]; // :359  @0x530 stride 0x10
        AboveGroundTestResult mAboveGroundTestResult;                 // :360  @0x570 (48 bytes)

        SimpleVehicleAttribs mSimpleAttribs;          // :361  @0x5A0 (console 240)
        Vector3      mHandlingBodyOffset;             // :362  @0x690
        Vector3      mHalfExtent;                     // :363  @0x6A0
        Vector3Plus  mWheelPlanePosAndHeight;         // :364  @0x6B0 (pos in xyz, plane height in w)
        VecFloat     mfSpeedMPH;                      // :365  @0x6C0
        AxisAlignedBox mDeformableAABB;               // :366  @0x6D0
        AxisAlignedBox mOriginalAABB;                 // :367  @0x6F0
        bool         mbCrashing;                      // :368  @0x710
        bool         mbStartedFatallyCrashing;        // :369  @0x711
        bool         mbStartedDeforming;              // :370  @0x712
        bool         mbCrashedThisFrame;              // :371  @0x713
        bool         mbMinWheelDistValid;             // :372  @0x714 (gates IsContactBelowWheelPlane)
        bool         mbAnyWheelsDetatched;            // :373  @0x715
    };

    // ⭐ The console sizes this block closes on, exported so the layout gate and the VehiclePhysics
    // own-block map can assert against ONE copy of each number instead of re-typing literals.
    // Every one is an X360 asm literal or forced with zero slack between two of them (see the map
    // above). They describe the CONSOLE object, not the host reconstruction.
    namespace X360Layout
    {
        const unsigned KU_SVP_BASE_END              = 0x130u;  // asm: Construct `addi r3,r31,0x130`
        const unsigned KU_SVP_WHEEL_STRIDE          = 0xE0u;   // asm: Wheel::Clear loop
        const unsigned KU_SVP_SWEPTSPHERE_SIZE      = 0x20u;   // closure 0x530-0x4B0 == 4*0x20
        const unsigned KU_SVP_ABOVEGROUND_OFF       = 0x570u;  // asm: `addi r11,r31,0x570`
        const unsigned KU_SVP_ABOVEGROUND_SIZE      = 0x30u;   // +0x28 mbValid -> 16-round
        const unsigned KU_SVP_SIMPLEATTRIBS_OFF     = 0x5A0u;  // asm: `addi r3,r31,0x5A0`
        const unsigned KU_SVP_HANDLINGBODYOFFSET    = 0x690u;  // asm: `li r9,0x690`
        const unsigned KU_SVP_DEFORMABLE_AABB_OFF   = 0x6D0u;  // asm: SetRaceCarCrashing copy dst
        const unsigned KU_SVP_ORIGINAL_AABB_OFF     = 0x6F0u;  // asm: SetRaceCarCrashing copy src
        const unsigned KU_SVP_AABB_SIZE             = 0x20u;   // asm: the copy is 4 x `ld`/`std`
        const unsigned KU_SVP_MBCRASHING_OFF        = 0x710u;  // console assert RaceCarPhysics.h:328
        const unsigned KU_SVP_SIZEOF                = 0x720u;  // == VehiclePhysics::mpAttribs offset
    }
}
}
