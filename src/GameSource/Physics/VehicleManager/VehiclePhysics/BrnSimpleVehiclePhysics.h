#pragma once

// BrnPhysics::Vehicle::AboveGroundTestResult -- the above-ground (down-ray) test result
// embedded by value in RaceCarState (and in SimpleVehiclePhysics). Namespace-scope struct,
// NOT nested, per references/DecFIGS/dwarfdump/.../BrnSimpleVehiclePhysics.h (line :71).
//
// Only this result struct is reconstructed; the SimpleVehiclePhysics class that also owns
// it (and its methods Reset/SetFrom/SetValidResult) is a separate future TU. Those methods
// are declared-only here so that TU can define them later without an ODR clash.
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, Matrix44Affine, CollisionTag, VecFloat
#include "types.hpp"          // f32, u16, u64, s32

#include <cstddef>            // offsetof (the SimpleVehicleAttribs interior gate)
#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"   // BrnPhysics::ExternalPhysicsBody (base)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"    // BrnPhysics::Vehicle::Wheel (full 0xE0 layout)
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // Forward decl: the per-car tuning block (full type owned by the VehicleAttribs TU).
    // struct, not class -- the committed home (VehicleAttribs.h:83) spells `struct`, and MSVC
    // bakes the class-key into the mangling (?AU vs ?AV): a `class` forward-decl here made
    // SimpleVehicleAttribs::SetupAttribs(const VehicleAttribs*) mangle to a symbol no TU defines.
    struct VehicleAttribs;
}
}

// Forward decl: the generated AttribSys handling wrapper (full type in
// GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h).
namespace Attrib { namespace Gen { class physicsvehiclehandling; } }
namespace CgsGeometric { struct Box; }

namespace BrnPhysics
{
namespace Vehicle
{
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
    // VehicleAttribs.h:1831). GROWN TO THE FULL 240-BYTE TYPE (attribs-setup wave,
    // ) from the 20-byte {mCOMOffset, mbIsValid} slice, additively per the slice's
    // own contract: every member below is the DWARF's, in DWARF sequence. The console offsets
    // in the right column CLOSE EXACTLY at 0xF0 == 240 == the 0x690-0x5A0 gap the layout gate
    // already pinned, and mCOMOffset lands at +0xD0 => SimpleVehiclePhysics+0x670, matching the
    // long-committed "mSimpleAttribs.mCOMOffset @+1648" witness independently.
    //
    // HOST == CONSOLE WIDTH: no pointer, no vptr; TireAttribs is 0x40 on both (static_assert in
    // VehicleAttribs.h); Attribute::Key is 8 bytes, spelled u64 per the VehicleAttribs.h
    // precedent (this tree's AttribSys header typedefs Attribute::Key to u32; the console
    // record is plainly 64-bit -- SimpleVehicleAttribs::SetupAttribs @0x825BE0C8 copies it
    // with one ld/std pair).
    struct SimpleVehicleAttribs
    {
        Vector4 mvUpwardMovement_DownwardMovement_Mass_TractionLineLength;                 // :1282 @0x00
        Vector4 mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset; // :1283 @0x10
        Wheel::TireAttribs mFrontTireAttribs;   // :1287 @0x20
        Wheel::TireAttribs mRearTireAttribs;    // :1288 @0x60
        u64     mAttribsKey;                    // :1289 @0xA0 (Attribute::Key, 8 bytes; see above)
        Vector3 mFrontRightWheelPos;            // :1290 @0xB0
        Vector3 mRearRightWheelPos;             // :1291 @0xC0
        Vector3 mCOMOffset;                     // :1292 @0xD0
        s32     miRaceCarID;                    // :1301 @0xE0
        bool    mbIsValid;                      // :1304 @0xE4  -> 16-rounds to 0xF0 == 240

        bool IsValid() const { return mbIsValid; }   // :1879

        // @0x825E6580 (125 insns) -- the default-attribs initialiser (car geometry defaults,
        // zeroed tires, mbIsValid = false). Bodied in VehicleAttribs.cpp (the DWARF home file
        // for this type).
        void Construct();

        // @0x825BE0C8 (81 insns) -- stream the simple set out of a full VehicleAttribs (the
        // masses/heights/wheel positions/COM/tires/key; sets mbIsValid = true; miRaceCarID is
        // NOT copied). Bodied in VehicleAttribs.cpp.
        void SetupAttribs(const VehicleAttribs* lpSource);

        // @0x825E6778 (104 insns) -- stream the simple set out of a loaded AttribSys handling
        // record (the physicsvehiclebaseattribs + physicsvehiclesuspensionattribs sub-records).
        // Bodied in VehicleAttribs.cpp. The DWARF/PS3 signature takes the wrapper BY VALUE
        // (the console call site runs the checked copy-ctor @0x825BDB88 then passes the copy,
        // and the callee destroys it); spelled const-ref here with the explicit copy transcribed
        // at each call site -- identical semantics without dragging the generated header into
        // this one.
        void SetupAttribs(const Attrib::Gen::physicsvehiclehandling& lrHandling);
    };

    // The grown type is pointer-free and width-identical host vs console, so its interior IS
    // gated with offsetof/sizeof (unlike the owning classes around it).
    static_assert(sizeof(SimpleVehicleAttribs) == 240, "SimpleVehicleAttribs must be 240 bytes");
    static_assert(offsetof(SimpleVehicleAttribs, mFrontTireAttribs) == 0x20,
                  "mFrontTireAttribs @0x20");
    static_assert(offsetof(SimpleVehicleAttribs, mAttribsKey) == 0xA0, "mAttribsKey @0xA0");
    static_assert(offsetof(SimpleVehicleAttribs, mCOMOffset) == 0xD0,
                  "mCOMOffset @0xD0 (SimpleVehiclePhysics +0x670 == the +1648 witness)");
    static_assert(offsetof(SimpleVehicleAttribs, mbIsValid) == 0xE4, "mbIsValid @0xE4");

    // BrnPhysics::Vehicle::SimpleVehiclePhysics -- the lightweight ("simple") vehicle physics
    // body used for traffic/secondary cars. Derives from ExternalPhysicsBody (which derives
    // from ExternallySimulatedBody, the owner of mTransform / mAngularVelocity). Member
    // SEQUENCE verbatim from the DecFIGS DWARF (BrnSimpleVehiclePhysics.h:357-373).
    //
    // ADDITIVE GROW (flagged by BrnPhysics-bodies group): only this group's 3 ledger funcs are
    // bodied (the two graphics-transform funcs + IsContactBelowWheelPlane). The remaining ~50
    // methods listed in the DWARF (Construct/Prepare/SwitchAttribs/SetAttributes/wheel & traction
    // accessors/...) are a separate future TU and are NOT declared here. The embedded sibling
    // types (Wheel, SweptSphere, SimpleVehicleAttribs, AxisAlignedBox) are owned by their normal
    // headers; the
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
        // SimpleVehicleAttribs::Construct, zero mHandlingBodyOffset/mHalfExtent, reset the
        // above-ground result, then run the zero-argument Reset wrapper. Bodied in the .cpp.
        void Construct();

        // @0x826206D0: destruct -- base Destruct, Wheel::Clear loop, Reset. Bodied below.
        void Destruct();

        // The no-argument source wrapper (DecFIGS @0x6D8824) calls Reset(Vector3::Zero) and
        // clears the base frozen flag.  Breaker inlines that wrapper in Construct/Destruct.
        void Reset();

        // @0x825D9A58: the Vector3 overload (DecFIGS @0x6D8718).  The source parameter is real
        // and is passed in v1 by Breaker callers, although this build's body does not consume it.
        // Wheel::Reset is gated by !mSimpleAttribs.IsValid(), then the local traction points,
        // speed and crash-state flags are cleared.
        void Reset(Vector3 lInitialVelocity);

        // DecFIGS BrnSimpleVehiclePhysics.cpp:834; its emitted PS3 body @0x6B4400 is empty.
        // Breaker's callers inline the more-derived VehiclePhysics implementation, which attests
        // this non-virtual VecFloat declaration without requiring an X360 out-of-line emission.
        void UpdatePostSimulation(VecFloat lvfTimeStep);

        // @0x82602880: stamp the above-ground (down-ray) test result from a position+normal+two
        // collision-tag halfwords. Asserts the position/normal are finite (debug). Bodied below.
        void SetAboveGroundTestResult(Vector3 lvPosition, Vector3 lvNormal,
                                      u16 lu16TagHi, u16 lu16TagLo);

        // @0x825D85C0 (174 insns) -- BODIED 2026-08-11 (lifetime wave). THE SUSPENSION PROBE:
        // one wheel's downward traction line, in world space. This is the last piece of the
        // ground chain's GENERATION half -- VehicleManager::AddRaceCarTractionLineTests calls it
        // once per wheel and drops the two points straight into the stream command's
        // maLineStart[w] / maLineEnd[w].
        //
        // THE X360 EXPORT SET HAS NO JSON FOR IT (a true directory hole, re-verified this wave
        // by a name index over all 30,084). Recovered TWO independent ways that agree:
        //   1. the image bytes at 0x825D85C0..0x825D8874, decoded with a VMX128 decoder fitted
        //      and self-tested against an EXPORTED twin (VehicleManager::UpdateTriangleCache
        //      @0x82615C38, whose IDA export carries full VMX128 mnemonics);
        //   2. the PS3 export @0x6E894C, which is what gives the CONST-ness, the three parameter
        //      names and the argument types:
        //      _ZNK..SimpleVehiclePhysics15GetTractionLineENS0_19EVehicleDrivenWheelE
        //          RN2rw4math3vpu7Vector3ES7_
        //      Its member offsets match the X360 body exactly (maWheels +0x130 stride 0xE0,
        //      mTransform +0x10, mSimpleAttribs +0x5A0, its IsValid byte at +0xE4 within it).
        void GetTractionLine(EVehicleDrivenWheel leWheel,
                             Vector3& lOutSusLineStart, Vector3& lOutSusLineEnd) const;

        // DWARF BrnSimpleVehiclePhysics.h:193.
        // Per-frame reset of the wheel/road latch set + the car-level above-ground result. The
        // X360 inlines it whole into VehicleManager::UpdateVehiclePhysics' live-car loop
        // (asm 0x826453B0..0x82645444), which is the byte source for the body: per wheel
        // {shift mbIsOnGround into mbWasOnGroundLastUpdate; clear mbIsOnGround /
        // mbIsCloseToGround / mbLineTestIsValid / mi8NumContacts / mbHasTraction}, then
        // mAboveGroundTestResult.Reset(). Bodied in BrnSimpleVehiclePhysics.cpp.
        void ResetAboveGroundTestResult();

        // The first three virtuals are kept in the DecFIGS order.  The base steering implementation
        // is the ICF-folded zero-return leaf; VehiclePhysics overrides it at 0x825D4028.
        virtual VecFloat GetSteeringAngle() const;

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
                     Vector3 lHandlingBodyOffset, Vector3 lHalfExtent,
                     const CgsGeometric::AxisAlignedBox& lrAABB,
                     VehicleAttribs* lpAttribs, const Vector3* lpWheelPositions,
                     const f32* lpafWheelRadii);

        // OUT of the BLOCKED list 2026-08-09 (attribs-setup wave): BODIED in
        // BrnSimpleVehiclePhysics.cpp from @0x82601978 (458 insns) -- the blocker (the 20-byte
        // SimpleVehicleAttribs slice) fell with the full 240-byte type above.
        void SwitchAttribs(VehicleAttribs* lpAttribs);

        // The three DWARF overloads (BrnSimpleVehiclePhysics.h:163/:166/:170; the PS3 export set
        // carries all three mangled names -- 7355CC/734B10/734274). ALL return bool.
        //   0-arg @0x82620498 (142): refresh mSimpleAttribs from the AttribSys handling record
        // keyed by mSimpleAttribs.mAttribsKey, then chain to the 2-arg. BODIED 2026-08-09
        //     (was the unnamed sub_82620498 -- recovered by caller set + the
        //     "mSimpleAttribs.IsValid()" assert's __FILE__/__LINE__).
        //   2-arg @0x826020A0 (503): the shared tail -- mass/inertia from mSimpleAttribs +
        // per-wheel Wheel::Prepare. BODIED 2026-08-09.
        //   3-arg: declared for the class surface (the PS3 attests it); no X360 body recovered
        //     to it yet.
        bool SetAttributes();
        bool SetAttributes(const Vector3* lpaWheelPositions, const f32* lpafWheelRadii);
        bool SetAttributes(VehicleAttribs* lpAttribs, const Vector3* lpWheelPositions,
                           const f32* lpafWheelRadii);

        // OUT of the BLOCKED list 2026-08-07 (orchestrator wave): BODIED in
        // BrnSimpleVehiclePhysics.cpp from @0x825D9608 -- the "cannot be reproduced BY NAME"
        // claim was unverified and false (every lane it touches was already a named member).
        void AddTractionPoint(EVehicleDrivenWheel leWheel, Vector3 lvPosition, Vector3 lvNormal,
                              u32 lu32CollisionTag);

        // @0x825D8878 (868 insns) -- BODIED 2026-08-13 (wheel-transform wave). THE WHEELS'
        // WORLD TRANSFORM: one wheel's render matrix -- spin (mIntegrationVariables.z) about the
        // axle, steer (GetSteeringAngle, front wheels only) about up, the crash arm's
        // buckle/spin/twist triple instead while mbCrashing, composed onto mTransform, left
        // wheels mirrored pi-about-Y unless the bool says not to; translation = body position +
        // body-rotated mPosition (suspension travel lives in mPosition.y). Decode bank:
        // scratchpad wheeltransform_bank.md; body in BrnSimpleVehiclePhysics.cpp.
        //
        // PARAMETER NAME CORRECTED from this tree's earlier `lbApplySteer` guess: the PS3
        // DWARF prototype (export 0x6E78CC) names it lbHackDontReverseRightWheels, and the body
        // agrees -- the bool NEVER gates steering (steer runs unconditionally for wheels 0/1);
        // it only gates the left-wheel mirror. (The console name says "Right", the DWARF enum
        // says the mirrored wheels 0/2 are the LEFT ones; the body and enum are the authority,
        // the ABI name is kept.) Every committed caller passes false == mirror ACTIVE.
        //
        Matrix44Affine GetWheelsWorldTransfrom(EVehicleDrivenWheel leWheel,
                                               bool lbHackDontReverseRightWheels) const;
        void GetSimpleVehicleBox(CgsGeometric::Box& lrOutBox) const;

        // OUT of the BLOCKED list 2026-08-07 (wheel-cluster wave): BODIED in
        // BrnSimpleVehiclePhysics.cpp from @0x82602CB8 -- the min-height plane fit over the four
        // wheels' line-test contacts, publishing mWheelPlanePosAndHeight / mbMinWheelDistValid /
        // mbAnyWheelsDetatched. Every lane it touches was already a named member; the exported
        // PSEUDOCODE (not the asm) was the only degenerate thing about it.
        void CalculateNewWheelPlane();
        virtual void SetCrashing();

        // DWARF BrnSimpleVehiclePhysics.h:244 --
        // the body-space box half extents (mHalfExtent @+0x6A0), by value. Consumed through
        // DeformableObject::GetHalfExtents() by the WithBoxes contact fix-up (its inside-the-
        // traffic-box test, `lvx [vehPhys+0x6A0]` @0x825DC0xx).
        Vector3 GetHalfExtent() const { return mHalfExtent; }

    protected:
        // ==========================================================================================
        // THE OWN-MEMBER BLOCK, SEATED 2026-08-03 (VehiclePhysics own-block wave).
        //
        // Member SEQUENCE is the DecFIGS DWARF's (BrnSimpleVehiclePhysics.h:357-373). Every OFFSET
        // in the right-hand column is an X360 ASM LITERAL or is forced with zero slack between two
        // asm literals. The two derivations know nothing about each other, and the block CLOSES:
        // its last data byte is 0x715, which 16-rounds to **0x720 == 1824**, and 0x720 is exactly
        // where `VehiclePhysics::Construct` @0x8262DC8C puts `mpAttribs` (`stw r30, 0x720(r31)`).
        //
        // THE FRAME. `SimpleVehiclePhysics::Construct` @0x82620400 calls
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
        // HOST DIVERGENCE, stated once. The leaf vptr widens 4 -> 8 on x64 but the base is
        // 16-aligned either way, so the base chain does NOT drift. Some sub-types embedded below
        // are MINIMAL OWNING SLICES in this tree (SweptSphere is an opaque 0x20 -- correct, see
        // the closure note on it -- and Wheel/AxisAlignedBox are their own reconstructions;
        // SimpleVehicleAttribs is the FULL width-identical 240 as of 2026-08-09). So the
        // absolute console offsets below are NOT reproducible as host offsetofs and are NOT
        // static_asserted. What IS gated (VehiclePhysics_layout_check.cpp) is the DWARF
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
        // 0x6D0  mDeformableAABB (32)         VehicleManager::SetRaceCarCrashing
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
        CgsGeometric::AxisAlignedBox mDeformableAABB; // :366  @0x6D0
        CgsGeometric::AxisAlignedBox mOriginalAABB;   // :367  @0x6F0
        bool         mbCrashing;                      // :368  @0x710
        bool         mbStartedFatallyCrashing;        // :369  @0x711
        bool         mbStartedDeforming;              // :370  @0x712
        bool         mbCrashedThisFrame;              // :371  @0x713
        bool         mbMinWheelDistValid;             // :372  @0x714 (gates IsContactBelowWheelPlane)
        bool         mbAnyWheelsDetatched;            // :373  @0x715

    public:
        // DWARF :283. ADDED 2026-08-09 (conductor wave): X360-attested by
        // PhysicsModule::Update @0x825B0640's slow-motion block, which inlines the read
        // (`lbz 0x713(car)`) against the car GetRaceCarPhysics resolved. Placed after the
        // member run so the access-specifier does not split the layout block.
        bool HasCrashedThisFrame() const { return mbCrashedThisFrame; }

        // FIVE DWARF-declared read accessors
        // over the protected member run above, every one of them X360-ATTESTED by the same
        // function: BrnPhysics::Vehicle::VehicleOutputInterface::UpdateRaceCarState @0x825EC808,
        // the console's ONLY writer of RaceCarState, which inlines all five against the
        // RaceCarPhysics it is handed (asm offsets in the comments). They are declared HERE,
        // after the member run, so the access specifier cannot split the layout block.
        //   DWARF BrnSimpleVehiclePhysics.h -- GetSpeedMPH / IsFatallyCrashing /
        //   HasStartedDeforming / GetAboveGroundTestResult / GetLocalTractionPoint.
        VecFloat GetSpeedMPH() const { return mfSpeedMPH; }                          // @0x6C0 (`lvx128 v0,r30,0x6C0`)

        // DecFIGS BrnSimpleVehiclePhysics.h:297 declares `VecFloat GetSpeed() const`.
        // Breaker inlines it in StuntOffencesManager::CheckForDrift @0x826138EC:
        // load the splatted MPH register at +0x6C0 and multiply it by the splatted
        // 0.44704 MPH-to-m/s constant at unk_83017FE0. This is a genuine VecFloat
        // accessor; the VMX splat represents one scalar speed, not an xyz velocity.
        VecFloat GetSpeed() const
        {
            static const f32 KF_MPH_TO_MPS = 0.447039992f; // flt_82F31928 / unk_83017FE0
            return VecFloat{
                mfSpeedMPH.x * KF_MPH_TO_MPS,
                mfSpeedMPH.y * KF_MPH_TO_MPS,
                mfSpeedMPH.z * KF_MPH_TO_MPS,
                mfSpeedMPH.w * KF_MPH_TO_MPS
            };
        }
        // ADDITIVE GROW, read-only, no layout change.
        // VehicleManager::HandleRaceCarTrafficCarPotentialContact @0x82640518/@0x826405C0 and
        // ::PredictCarCarIntersection @0x825C5A54 both reach mDeformableAABB with BARE loads off
        // a body they were handed (`addi r9, <body>, 0x6D0` then `lvx128` at +0 / +0x10), from a
        // class that is not a friend. This is the smallest honest seam that lets those bodies read
        // it BY NAME instead of by offset; it invents no API surface beyond the read.
        const CgsGeometric::AxisAlignedBox& GetDeformableAABB() const { return mDeformableAABB; }

        bool     IsFatallyCrashing() const { return mbStartedFatallyCrashing; }      // @0x711 (`lbz r11,0x711(r30)`)
        bool     HasStartedDeforming() const { return mbStartedDeforming; }          // @0x712 (`lbz r11,0x712(r30)`)
        const AboveGroundTestResult* GetAboveGroundTestResult() const                // @0x570 (`addi r11,r31,0x570`)
        {
            return &mAboveGroundTestResult;
        }
        Vector3 GetLocalTractionPoint(u8 lu8Wheel) const                             // @0x530 stride 16
        {                                                                            // (`r29 = (wheel+0x53)<<4 ; lvx128 v0,r29,r30`)
            return maLocalTractionPoints[lu8Wheel];
        }

        // ADDED wave T3 (physical traffic). DWARF BrnSimpleVehiclePhysics.h:214
        // (`const Wheel* GetWheel(EVehicleDrivenWheel) const`) -- the BASE's own wheel accessor,
        // which the tree previously carried only on the derived VehiclePhysics (:244, by
        // reference). X360-attested by VehicleOutputInterface::AddTrafficState @0x825EC390, whose
        // per-wheel loop walks `<physics>+0x130 + 224*i` -- maWheels below -- off a
        // SimpleVehiclePhysics*, the static type PhysicalTrafficVehicle::mpVehicleBody has.
        // Declaration + inline body only: no member, layout or existing signature is touched, and
        // VehiclePhysics::GetWheel continues to hide this one for every existing caller.
        const Wheel* GetWheel(EVehicleDrivenWheel leWheel) const { return &maWheels[leWheel]; }
    };

    // The console sizes this block closes on, exported so the layout gate and the VehiclePhysics
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
