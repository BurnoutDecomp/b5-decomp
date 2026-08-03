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
        // Member sequence per BrnSimpleVehiclePhysics.h:357-373. The C11 group (which owns the
        // SimpleVehiclePhysics body set) now lays out the four leading sub-type arrays + the
        // above-ground test result that PRECEDE mSimpleAttribs, in DWARF SEQUENCE, so its bodies
        // resolve them BY NAME. (Originally these were deferred to a "future TU"; that TU is this
        // group.) The members this group's funcs read (maWheels, mAboveGroundTestResult,
        // mSimpleAttribs, mWheelPlanePosAndHeight, mbMinWheelDistValid) are all present BY NAME;
        // per project rule the absolute console offsets are NOT reproduced as padding.
        //
        // ----- ADDITIVE GROW (C11 group): the four leading sub-type arrays + the above-ground test
        //       result, in their DWARF SEQUENCE (:357-360). The full Wheel (0xE0) is the committed
        //       sibling type; SweptSphere / AxisAlignedBox are minimal owning slices (flagged above).
        //       Pinned BY NAME -- the absolute console offsets (maWheels @ +304 stride 224) are NOT
        //       reproduced as padding per project rule; the bodies index these arrays by name.
        Wheel                maWheels[eNumDrivenWheels];              // :357  (+304, stride 0xE0)
        SweptSphere          maWheelSpheres[eNumDrivenWheels];        // :358
        Vector3              maLocalTractionPoints[eNumDrivenWheels]; // :359
        AboveGroundTestResult mAboveGroundTestResult;                 // :360 (pos @ +348, normal @ +364)

        SimpleVehicleAttribs mSimpleAttribs;          // :361
        Vector3      mHandlingBodyOffset;             // :362
        Vector3      mHalfExtent;                     // :363
        Vector3Plus  mWheelPlanePosAndHeight;         // :364  (pos in xyz, plane height in w lane)
        VecFloat     mfSpeedMPH;                      // :365
        AxisAlignedBox mDeformableAABB;               // :366
        AxisAlignedBox mOriginalAABB;                 // :367
        bool         mbCrashing;                      // :368
        bool         mbStartedFatallyCrashing;        // :369
        bool         mbStartedDeforming;              // :370
        bool         mbCrashedThisFrame;              // :371
        bool         mbMinWheelDistValid;             // :372  (gates IsContactBelowWheelPlane)
        bool         mbAnyWheelsDetatched;            // :373
    };
}
}
