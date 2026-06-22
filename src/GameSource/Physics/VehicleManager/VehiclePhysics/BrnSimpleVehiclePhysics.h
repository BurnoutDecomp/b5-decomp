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

namespace BrnPhysics
{
namespace Vehicle
{
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

    protected:
        // Member sequence per BrnSimpleVehiclePhysics.h:357-373. The leaf-specific sub-types
        // that PRECEDE these (Wheel[4] maWheels, SweptSphere[4] maWheelSpheres, Vector3[4]
        // maLocalTractionPoints, AboveGroundTestResult mAboveGroundTestResult) are owned by the
        // future SimpleVehiclePhysics TU and are NOT laid out here -- so the absolute console
        // offsets are intentionally not reproduced (project rule: pin BY NAME). The members
        // this group's funcs read (mSimpleAttribs, mWheelPlanePosAndHeight, mbMinWheelDistValid)
        // are present and reachable BY NAME.
        SimpleVehicleAttribs mSimpleAttribs;          // :361
        Vector3      mHandlingBodyOffset;             // :362
        Vector3      mHalfExtent;                     // :363
        Vector3Plus  mWheelPlanePosAndHeight;         // :364  (pos in xyz, plane height in w lane)
        VecFloat     mfSpeedMPH;                      // :365
        bool         mbCrashing;                      // :368
        bool         mbStartedFatallyCrashing;        // :369
        bool         mbStartedDeforming;              // :370
        bool         mbCrashedThisFrame;              // :371
        bool         mbMinWheelDistValid;             // :372  (gates IsContactBelowWheelPlane)
        bool         mbAnyWheelsDetatched;            // :373
    };
}
}
