#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "SDKs/EATech/rwcollision/volume_debug_access.h"
#include "vendor/renderware/collision/FeatureEdge.hpp"   // Vec4 / RwBool (this directory's
                                                         //   vocabulary-neutral vector row)

// The common native Volume declaration lives in volume_debug_access.h.
// BoxVolume and SphereVolume add no fields to that 0x60-byte base.

namespace rw
{
    // rw::Resource -- the RenderWare memory-block descriptor every rwcollision
    // `Initialize` takes. Its real home is the generated vendor header
    // b5-decomp/vendor/renderware/include/rw/rwcore_structs.h:73
    // (`struct Resource : public BaseResources<4>`), so it is NAMED here, never forked;
    // a reference parameter does not need the definition. The two bodies that read it
    // (BoxVolume.cpp) include that header.
    //
    // CROSS-BUILD DELTA, MEASURED: the X360 instantiates it with FIVE base resources --
    // all four call sites stage a 20-byte (5 x 4-byte) stack record and zero every word
    // before storing the block pointer into word 0 (ProcessAddTriggerEvents @0x822D8F48
    // at 0x822D9520/0x822D96C4/0x822D98DC, ActiveRaceCar::AddToScene @0x822EB768 at
    // 0x822EB918). PC rwcore uses <4> (32 bytes on x64). Only word 0 is ever read by the
    // callee (`lwz r3, 0(r3)`), so the arity difference is inert here; it is the same
    // <4>-vs-<5> drift AGENTS.md records for rw::ResourceDescriptor.
    struct Resource;
}

namespace rw
{
namespace collision
{
    // rw::collision::AABBox lives in vendor/renderware/collision/AABBox.hpp and is only
    // NAMED here (a reference out-parameter), which does not need the definition.
    // DELIBERATE: AABBox.hpp pulls the EATech rw::math::vpu vocabulary, and this header
    // is included by GAME translation units (BrnTriggerEntityModule.cpp,
    // BrnActiveRaceCar_wQ5_01.cpp) that live in the vendor-POD Vector3 world. Dragging
    // AABBox.hpp in here would break every one of them. Same reasoning, same wording as
    // SDKs/EATech/rwcollision/volume_debug_access.h:61-65.
    class AABBox;

    // The other four descriptor-slot parameter types, NAMED for exactly the same reason
    // (a reference / pointer parameter needs no definition, and each of their headers
    // drags a vocabulary this header must stay free of). Their homes:
    //   Feature                      vendor/renderware/collision/Feature.hpp:39
    //   GPInstance / Interval        vendor/renderware/collision/GPInstance.hpp:69/:55
    //   VolumeLineSegIntersectResult vendor/renderware/collision/LineSegIntersect.hpp:59
    // Class keys match those definitions (all four are `struct`).
    struct Feature;
    struct GPInstance;
    struct Interval;
    struct VolumeLineSegIntersectResult;

    const u32 KU_VOLUMEFLAG_ISENABLED = 0x00000001;

    // ---------------------------------------------------------------------------
    // rw::collision::BoxVolume -- DWARF box.h:50 (`: public Volume`).
    //
    // Only the members the X360 ledger attests are declared. DWARF box.h also lists
    // GetDimensions / SetDimensions / GetResourceDescriptor / CreateGPInstance /
    // GetInterval / GetMaximumFeature / LineSegIntersect / Release; of those only
    // GetMaximumFeature @0x82BA87F0, CreateGPInstance @0x82BA92E8 and LineSegIntersect
    // @0x82BA9478 are X360 ledger rows (all three unreconstructed -- see BoxVolume.cpp's
    // NOT-LANDED block), and the rest are header inlines with no symbol. AGENTS: the
    // DWARF supplies names/types, the X360 ledger decides what exists.
    // ---------------------------------------------------------------------------
    struct BoxVolume : public Volume
    {
        // Canonical default ctor -- members left uninitialised until Initialize, the
        // same spelling CapsuleVolume.hpp:53 / CylinderVolume.hpp:52 use. Declaring the
        // (Vec4) ctor below would otherwise suppress it.
        BoxVolume() {}

        // @ 0x82BAA0F0 -- DWARF box.h:57 `void BoxVolume(Vector3)`. Seeds the record:
        // identity transform, half-extents from arHalfDimensions, zero fatness, zero
        // group/surface, flags = 1, and the BOX descriptor slot. The Vector3 argument
        // arrives by value in v1 and is immediately spilled and re-read as three scalar
        // floats (`stvx128 v1, r1, r12` then `lfs arg_20/arg_24/arg_28`), so a
        // `const Vec4&` is the faithful host spelling -- the same choice CapsuleVolume
        // makes for its by-value Vector3 parameters.
        explicit BoxVolume(Vector3 arHalfDimensions);

        // @ 0x82BAA188 -- DWARF box.h:109
        //     `BoxVolume* Initialize(const Resource&, Vector3)`.
        // STATIC (the asm's r3 is the Resource, not a `this`: `lwz r3, 0(r3)` reads the
        // descriptor's first word and the tail `b` hands that pointer to the ctor as r3).
        // Returns NULL when the resource carries no block; every caller uses the result
        // as its own `lpVolume != NULL` guard.
        static BoxVolume* Initialize(const ::rw::Resource& arResource,
                                     Vector3               arHalfDimensions);

        // DWARF box.h:112 -- the three-float overload. It has NO symbol of its own in the
        // X360 image (the callers build the Vector3 in v1 themselves and call 0x82BAA188
        // directly), i.e. it is a header inline that forwards; reproduced as one here.
        static BoxVolume* Initialize(const ::rw::Resource& arResource,
                                     f32 afHalfX, f32 afHalfY, f32 afHalfZ)
        {
            const Vector3 lvHalfDimensions(afHalfX, afHalfY, afHalfZ);
            return Initialize(arResource, lvHalfDimensions);
        }

        // @ 0x82BA9FC8 (74 insns) -- DWARF box.h:157. Write the box's (optionally
        // transformed) AABB to arResult and return 1. abTight is accepted and DEAD in the
        // asm (r5 is never read) -- a box has no tight/loose split.
        RwBool GetBBox(const Matrix44Affine* lpTransform, RwBool abTight, AABBox& arResult) const;

        // @ 0x82BA8890 -- DWARF box.h:160. The LOCAL bbox diagonal (full extents), i.e.
        // 2 * (|row0|*hx + |row1|*hy + |row2|*hz + radius). No transform argument.
        Vector3 GetBBoxDiag() const;

        RwBool GetInterval(Vector3 aDir, Interval& arInterval) const;
        void Release();

        // @ 0x82BA87F0 (9 insns) -- DWARF box.h:42
        //     `RwBool GetMaximumFeature(RwBool, Vector3, Feature &) const`.
        // ⚠️ `arDir` is BY VALUE here (DWARF), unlike the sphere/capsule/cylinder
        // spelling -- that is what puts the Feature out-parameter in r5 rather than r6
        // on the console (see the VTable banner). Both parameters are DEAD in the body:
        // the box's maximum feature is always its centre point.
        RwBool GetMaximumFeature(RwBool abCcw, Vector3 aDir, Feature& arFeature) const;

        // @ 0x82BA92E8 (~85 insns) -- DWARF box.h:36. Instance the box into the GP
        // narrow-phase image: three face normals + three edge directions (the same three
        // optionally-rotated transform rows), mPos = the transformed centre row,
        // mDimensions = (hx, hy, hz, hx) and the BOX row of g_aGPVolumeMethods.
        RwBool CreateGPInstance(GPInstance& arInstance, const Matrix44Affine* lpTransform) const;

        // @ 0x82BA9478 (723 insns) -- DWARF box.h:176 `RwBool LineSegIntersect(const Vector3&, const Vector3&,
        // const Matrix44Affine*, VolumeLineSegIntersectResult&, float32_t) const`. The segment arPt1 -> arPt2 against
        // the box (half extents +0x44..+0x4C) rounded by mfRadius + afFatness, in its frame (composed with lpTransform when given); fills
        // arResult on a hit (1). LANDED 2026-09-25 (crash parity FX-FOLLOWUPS stage (b)); body in LineSegIntersect.cpp,
        // bound to the descriptor's lineSegIntersect slot in VolumeVTables.cpp.
        RwBool LineSegIntersect(const Vector3& arPt1, const Vector3& arPt2, const Matrix44Affine* lpTransform,
                                VolumeLineSegIntersectResult& arResult, f32 afFatness) const;
    };

    // ---------------------------------------------------------------------------
    // rw::collision::SphereVolume -- DWARF sphere.h. Declared here (rather than in its
    // own file) because this header already owned the placeholder the trigger module
    // uses, and the Volume shape change forces both bodies to be rewritten anyway.
    // ---------------------------------------------------------------------------
    struct SphereVolume : public Volume
    {
        SphereVolume() {}

        // @ 0x82BA84E8 -- DWARF sphere.h:12 `SphereVolume* Initialize(const Resource&,
        // float32_t)`. Static, same contract as BoxVolume::Initialize. The X360 folded
        // the constructor into it (there is no separate SphereVolume::SphereVolume
        // symbol), so this one function carries the whole seed.
        static SphereVolume* Initialize(const ::rw::Resource& arResource, f32 afRadius);

        // @ 0x82BA8020 -- the sphere's AABB: the transformed centre +/- the radius.
        // abTight is dead in the asm here too.
        RwBool GetBBox(const Matrix44Affine* lpTransform, RwBool abTight, AABBox& arResult) const;

        // @ 0x82BA8580 -- 2 * radius on every axis.
        Vector3 GetBBoxDiag() const;

        RwBool GetInterval(const Vector3& arDir, Interval& arInterval) const;
        void Release();

        // @ 0x82BA81B0 (9 insns) -- DWARF sphere.h:27
        //     `RwBool GetMaximumFeature(RwBool, const Vector3 &, Feature &) const`.
        // BY REFERENCE here (DWARF), which is why the console writes the Feature through
        // r6 while the byte-identical box body writes through r5. Both parameters are
        // dead: a sphere's maximum feature is its centre point.
        RwBool GetMaximumFeature(RwBool abCcw, const Vector3& arDir, Feature& arFeature) const;

        // @ 0x82BA8100 (~40 insns) -- DWARF sphere.h:21. Instance the sphere into the GP
        // narrow-phase image: mPos = the transformed centre, no face normals, no edge
        // directions, mFatness = radius, and the SPHERE row of g_aGPVolumeMethods.
        RwBool CreateGPInstance(GPInstance& arInstance, const Matrix44Affine* lpTransform) const;

        // @ 0x82BA82C8 (136 insns) -- DWARF sphere.h:90 `RwBool LineSegIntersect(const Vector3&, const Vector3&,
        // const Matrix44Affine*, VolumeLineSegIntersectResult&, float32_t) const`. The segment arPt1 -> arPt2 against
        // the sphere (its centre is the frame's translation) rounded by mfRadius + afFatness, in its frame (composed with lpTransform when given); fills
        // arResult on a hit (1). LANDED 2026-09-25 (crash parity FX-FOLLOWUPS stage (b)); body in LineSegIntersect.cpp,
        // bound to the descriptor's lineSegIntersect slot in VolumeVTables.cpp.
        RwBool LineSegIntersect(const Vector3& arPt1, const Vector3& arPt2, const Matrix44Affine* lpTransform,
                                VolumeLineSegIntersectResult& arResult, f32 afFatness) const;
    };

    // Six native per-type records are defined in VolumeVTables.cpp.
    extern const Volume::VTable gVolumeHandler_82F91740;   // type 1 SPHERE
    extern const Volume::VTable gVolumeHandler_82F918C0;   // type 2 CAPSULE
    extern const Volume::VTable gVolumeHandler_82F919A4;   // type 3 TRIANGLE
    extern const Volume::VTable gVolumeHandler_82F9176C;   // type 4 BOX
    extern const Volume::VTable gVolumeHandler_82F91894;   // type 5 CYLINDER
    extern const Volume::VTable gVolumeHandler_82F919D0;   // type 6 AGGREGATE

    // --- layout pins (console offsets; all X360-asm-attested) -----------------------
    CGS_STATIC_ASSERT((sizeof(Vec4) == 16), "rw::collision::Vec4 must be one 16-byte row");
    CGS_STATIC_ASSERT((offsetof(Volume, mTransform)   == 0x00), "Volume::transform  @ +0x00");
    CGS_STATIC_ASSERT((offsetof(Volume, mpVTable)  == 0x40), "Volume::vTable     @ +0x40");
    CGS_STATIC_ASSERT((offsetof(Volume, mBoxData)      == 0x44), "Volume::boxData    @ +0x44");
    CGS_STATIC_ASSERT((offsetof(Volume, mfRadius)      == 0x50), "Volume::radius     @ +0x50");
    CGS_STATIC_ASSERT((offsetof(Volume, muGroupID)     == 0x54), "Volume::groupID    @ +0x54");
    CGS_STATIC_ASSERT((offsetof(Volume, muSurfaceID)   == 0x58), "Volume::surfaceID  @ +0x58");
    CGS_STATIC_ASSERT((offsetof(Volume, muFlags)       == 0x5C), "Volume::m_flags    @ +0x5C");
    CGS_STATIC_ASSERT((sizeof(Volume)       == 96), "rw::collision::Volume console stride is 96");
    CGS_STATIC_ASSERT((sizeof(BoxVolume)    == 96), "BoxVolume adds no storage over Volume");
    CGS_STATIC_ASSERT((sizeof(SphereVolume) == 96), "SphereVolume adds no storage over Volume");
    CGS_STATIC_ASSERT((offsetof(BoxVolume, mBoxData) == 0x44), "BoxVolume half-extents @ +0x44");

    // ARTIST descriptor records are ten 32-bit words (type id, seven method
    // slots, name and flags); GetBBox is slot 1 and CreateGPInstance slot 5.
    CGS_STATIC_ASSERT((sizeof(Volume::VTable) == 0x28), "X360 volume descriptor size");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, muTypeID)              ==  0), "VTable typeID");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, mpfnGetBBox)           ==  4), "VTable getBBox");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, mpfnGetBBoxDiag)       == 8), "VTable getBBoxDiag");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, mpfnGetInterval)       == 12), "VTable getInterval");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, mpfnGetMaximumFeature) == 16), "VTable getMaximumFeature");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, mpfnCreateGPInstance)  == 20), "VTable createGPInstance");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, mpfnLineSegIntersect)  == 24), "VTable lineSegIntersect");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, mpfnRelease)           == 28), "VTable release");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, mpcName)               == 32), "VTable name");
    CGS_STATIC_ASSERT((offsetof(Volume::VTable, muFlags)               == 36), "VTable flags");

} // namespace collision
} // namespace rw
