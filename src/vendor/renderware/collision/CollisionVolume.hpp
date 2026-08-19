#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "vendor/renderware/collision/FeatureEdge.hpp"   // Vec4 / RwBool (this directory's
                                                         //   vocabulary-neutral vector row)

// ===========================================================================
// rw::collision::Volume and its two primitive subclasses SphereVolume / BoxVolume.
//
// THIS FILE USED TO BE A 128-BYTE PLACEHOLDER. Until 2026-08-18 (wave Q5) it carried
// `struct Volume { u32 muVolumeType; f32 mafParams[3]; u8 maPad[112]; }` with two
// invented `bool Initialize(...)` members, self-flagged as "a faithful placeholder, not
// the real RW volume layout". It is now the real record, recovered from:
//
//   * DecFIGS DWARF -- .../rw/collision/volume.h:1304-1554 (`struct rw::collision::Volume`:
//     transform / vTable / <union> / radius / groupID / surfaceID / m_flags),
//     .../rw/collision/volumedata.h:80-175 (the six *SpecificData union arms), and
//     .../rw/collision/box.h:50-179 (`struct rw::collision::BoxVolume : public Volume`).
//   * X360 ARTIST assembly for every body (addresses on each declaration below).
//
// LAYOUT -- console byte offsets, all X360-asm-attested, and PRESERVED EXACTLY on the
// host (see the width note further down):
//
//   transform  @ +0x00 (0)   Matrix44Affine, 64 bytes = four 16-byte rows
//   vTable     @ +0x40 (64)  Volume::VTable*   (the per-TYPE descriptor, NOT a C++ vptr)
//   <union>    @ +0x44 (68)  {sphere|capsule|triangle|box|cylinder|aggregate}SpecificData,
//                            12 bytes -- the BOX arm is the half-extents hx/hy/hz
//   radius     @ +0x50 (80)  float32_t  (the "fatness" every AABB axis is padded by)
//   groupID    @ +0x54 (84)  uint32_t
//   surfaceID  @ +0x58 (88)  uint32_t
//   m_flags    @ +0x5C (92)  uint32_t   -> sizeof == 96
//
// The offsets are pinned three independent ways: BoxVolume::BoxVolume @0x82BAA0F0 and
// SphereVolume::Initialize @0x82BA84E8 store to +0x40/+0x44/+0x48/+0x4C/+0x50/+0x54/
// +0x58/+0x5C and to the four 16-byte transform rows; BoxVolume::GetBBox @0x82BA9FC8 and
// GetBBoxDiag @0x82BA8890 read +0x44/+0x48/+0x4C/+0x50 with `lvlx`; and the 96-byte
// stride is the indexing stride of PropTypeData's serialised volume run
// (PropManager::GetPropInertia @0x82612640 computes i*96).
//
// ⚠️ THE +0x40 SLOT IS A CONSOLE-WIDTH WORD ON THE HOST TOO -- DELIBERATELY.
// On the console the slot holds a 4-byte descriptor POINTER. It is modelled here as a
// u32 because everything after it is offset-pinned (+0x44 half-extents, +0x50 radius) and
// because sizeof(Volume) == 96 is the serialised stride; letting the pointer widen to 8
// bytes would push every following field by 4 and silently break every consumer that
// reads this record by byte offset (VolumeBBoxQuery.cpp:78/:86/:95/:102,
// VolumeQuery.cpp:173/:184, PrimitiveIntersect.cpp:825/:1143/:1151,
// SDKs/EATech/rwcollision/volume_debug_access.h). See the ctor in BoxVolume.cpp for what
// this costs and what is stamped there instead; the host-width promotion of this slot is
// a Volume/InitializeVTable-TU job (reported, not done here).
//
// ⚠️ FORK, REPORTED NOT RESOLVED (unchanged by this wave). A second
// `rw::collision::Volume` exists at SDKs/EATech/rwcollision/volume_debug_access.h:118 --
// the same 96-byte record modelled as an opaque `u8 maPayload[96]` with byte-offset
// getters. That one is what PropTypeData/PropPartTypeData hold
// (BrnPhysicsPropTypeData.h:8/:101/:177, `static_assert(sizeof == 96)` at :201); THIS one
// is what the vendor query/intersect TUs and the two runtime volume producers use. They
// describe the SAME console record and now agree field for field, but they are still two
// C++ types with the same qualified name. They are never co-included today (measured
// again this wave). Collapsing them is a separate job -- it is blocked on the duplicate
// `rw::math::vpu::Vector3` vocabulary, whose full work list is in
// scratchpad/waveQ2/rwvol.owner.md section 4.5.
// ===========================================================================

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

    // The rwcollision volume primitive kinds -- DWARF volume.h:1238
    // (VOLUMETYPENULL..VOLUMETYPENUMINTERNALTYPES), MEASURED a second time out of the
    // shipped image: each per-type descriptor's leading word is its own id
    // (vTableArray[1]=0x82F91740 word 1 "SphereVolume" ... [4]=0x82F9176C word 4
    // "BoxVolume" ... [6]=0x82F919D0 word 6 "AggregateVolume"; dump in
    // scratchpad/waveQ2/evidence/volume_vtables_ida_dump.txt).
    //
    // ⚠️ CORRECTED 2026-08-18 (wave Q5). The placeholder this file used to carry declared
    // `E_VOLUMETYPE_BOX = 2`. TWO is CAPSULE. A box stamped with 2 is a capsule as far as
    // the rwcollision runtime is concerned -- FixableVolume::FixUp would hand it
    // gVolumeVTable[2] and every dispatch would run the capsule handler on box data. The
    // spelling and the values now match SDKs/EATech/rwcollision/volume_debug_access.h:90-99
    // and the descriptor table reconstructed in GPRegistration.cpp, which is the whole
    // point of the correction.
    enum EVolumeType
    {
        E_VOLUMETYPE_NULL             = 0,   // slot 0 of gVolumeVTable, always null
        E_VOLUMETYPE_SPHERE           = 1,   // descriptor 0x82F91740, leading word 1
        E_VOLUMETYPE_CAPSULE          = 2,   // descriptor 0x82F918C0, leading word 2
        E_VOLUMETYPE_TRIANGLE         = 3,   // descriptor 0x82F919A4, leading word 3
        E_VOLUMETYPE_BBOX             = 4,   // descriptor 0x82F9176C, leading word 4
        E_VOLUMETYPE_CYLINDER         = 5,   // descriptor 0x82F91894, leading word 5
        E_VOLUMETYPE_AGGREGATE        = 6,   // descriptor 0x82F919D0, leading word 6
        E_VOLUMETYPE_NUMINTERNALTYPES = 7    // == the 7-entry vTableArray in volume.cpp
    };

    // Volume::m_flags bit 0 (canonical rwccore.h VOLUMEFLAG_ISENABLED). Both runtime
    // constructors stamp exactly this (`li r10, 1 ; stw r10, 0x5C(r3)`).
    const u32 KU_VOLUMEFLAG_ISENABLED = 0x00000001;

    // --- the type-specific union arms (DWARF volumedata.h:80-175) -------------------
    // Console widths. SphereSpecificData::nothing (`void*`) and
    // AggregateSpecificData::agg (`Aggregate*`) are CONSOLE pointers; they are spelled as
    // console-width words here for the same reason Volume::muVTableSlot is (see the
    // banner) -- widening either one would push radius/groupID/surfaceID/m_flags off
    // their measured offsets. Neither is read by anything homed in this file.
    struct SphereSpecificData   { u32 muNothing;  };                      // volumedata.h:80
    struct CapsuleSpecificData  { f32 mfHalfHeight; };                    // volumedata.h:96
    struct TriangleSpecificData { f32 mfEdgeCos0, mfEdgeCos1, mfEdgeCos2; }; // volumedata.h:115
    struct BoxSpecificData      { f32 mfHx, mfHy, mfHz; };                // volumedata.h:133
    struct CylinderSpecificData { f32 mfHalfHeight; f32 mfInnerRadius; }; // volumedata.h:156
    struct AggregateSpecificData{ u32 muAggregate; };                     // volumedata.h:173

    // ---------------------------------------------------------------------------
    // rw::collision::Volume -- DWARF volume.h:1304.
    //
    // Kept a `struct` (not a `class`) because VolumeBBoxQuery.hpp:38 and
    // VolumeQuery.hpp:65 already forward-declare it with the struct class-key.
    //
    // ⚠️ MUST NOT GAIN A VIRTUAL FUNCTION: a vptr would shift every field and break the
    // 96-byte stride. The rwcollision dispatch is NOT C++ virtual dispatch -- it goes
    // through the per-TYPE descriptor pointer stored in the record at +0x40. Neither
    // subclass below declares a virtual either, which is also why `BoxVolume*` and
    // `Volume*` are the same address (relied on by GetBBox's `lvx128 v11, r0, r3`
    // reading the transform straight off `this`).
    // ---------------------------------------------------------------------------
    struct Volume
    {
        // The rwcollision per-TYPE method descriptor (DWARF volume.h:1507 / rwccore.h
        // :1584-1595: typeID, getBBox, getBBoxDiag, getInterval, getMaximumFeature,
        // createGPInstance, lineSegIntersect, release, name, flags). Its full host shape
        // is defined by the OTHER half of this fork, SDKs/EATech/rwcollision/
        // volume_debug_access.h (the two headers cannot be co-included, C2011 Vector3);
        // here it is opaque and only named so gVolumeVTable below has the DWARF's
        // element type and the ONE mangled symbol both halves of the tree bind to
        // (?gVolumeVTable@collision@rw@@3PAPEAUVTable@Volume@12@A -- measured, see
        // volume_debug_access.h). The TU-local descriptor views in VolumeQuery.cpp /
        // VolumeBBoxQuery.cpp / PrimitiveIntersect.cpp reinterpret the pointer.
        struct VTable;

        // DWARF `Matrix44Affine transform` @ +0x00. Carried as four scalar Vec4 rows,
        // the vocabulary-neutral spelling this directory already uses for volume frames
        // (CapsuleVolume.hpp:104 `Vec4 maFrame[4]`, CylinderVolume.hpp:85). Row order is
        // the SDK's xAxis / yAxis / zAxis / wAxis -- maTransform[3] is the TRANSLATION
        // row (the one ActiveRaceCar::AddToScene decrements in Y at 0x822EBBD4).
        Vec4 maTransform[4];        // +0x00 / +0x10 / +0x20 / +0x30

        // DWARF `Volume::VTable* vTable` @ +0x40 -- the shared per-TYPE descriptor.
        // Console-width word on the host; see the file banner and BoxVolume.cpp's ctor.
        // On the console this slot holds the EVolumeType enum on disk and, after
        // BrnPhysics::Props::FixableVolume::FixUp @0x828A87A0, gVolumeVTable[enum]
        // (FixDown @0x828A8830 puts the enum back). HOST REPRESENTATION (2026-08-18,
        // wave Q5 integration): the slot ALWAYS holds the 4-byte enum -- an x64 pointer
        // would overlap the +0x44 lane -- and every reader recovers the console's
        // pointer as gVolumeVTable[muVTableSlot] (declared below).
        u32  muVTableSlot;          // +0x40

        // DWARF's anonymous union of the six *SpecificData arms @ +0x44, 12 bytes.
        union
        {
            SphereSpecificData    mSphereData;
            CapsuleSpecificData   mCapsuleData;
            TriangleSpecificData  mTriangleData;
            BoxSpecificData       mBoxData;
            CylinderSpecificData  mCylinderData;
            AggregateSpecificData mAggregateData;
            u32                   mauSpecific[3];   // the raw 12-byte console arm
        };

        f32  mfRadius;              // +0x50  DWARF `float32_t radius`
        u32  muGroupID;             // +0x54  DWARF `uint32_t groupID`
        u32  muSurfaceID;           // +0x58  DWARF `uint32_t surfaceID`
        u32  muFlags;               // +0x5C  DWARF `uint32_t m_flags`
    };

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
        explicit BoxVolume(const Vec4& arHalfDimensions);

        // @ 0x82BAA188 -- DWARF box.h:109
        //     `BoxVolume* Initialize(const Resource&, Vector3)`.
        // STATIC (the asm's r3 is the Resource, not a `this`: `lwz r3, 0(r3)` reads the
        // descriptor's first word and the tail `b` hands that pointer to the ctor as r3).
        // Returns NULL when the resource carries no block; every caller uses the result
        // as its own `lpVolume != NULL` guard.
        static BoxVolume* Initialize(const ::rw::Resource& arResource,
                                     const Vec4&           arHalfDimensions);

        // DWARF box.h:112 -- the three-float overload. It has NO symbol of its own in the
        // X360 image (the callers build the Vector3 in v1 themselves and call 0x82BAA188
        // directly), i.e. it is a header inline that forwards; reproduced as one here.
        static BoxVolume* Initialize(const ::rw::Resource& arResource,
                                     f32 afHalfX, f32 afHalfY, f32 afHalfZ)
        {
            const Vec4 lvHalfDimensions = { afHalfX, afHalfY, afHalfZ, 0.0f };
            return Initialize(arResource, lvHalfDimensions);
        }

        // @ 0x82BA9FC8 (74 insns) -- DWARF box.h:157. Write the box's (optionally
        // transformed) AABB to arResult and return 1. abTight is accepted and DEAD in the
        // asm (r5 is never read) -- a box has no tight/loose split.
        RwBool GetBBox(const Vec4* lpTransform, RwBool abTight, AABBox& arResult) const;

        // @ 0x82BA8890 -- DWARF box.h:160. The LOCAL bbox diagonal (full extents), i.e.
        // 2 * (|row0|*hx + |row1|*hy + |row2|*hz + radius). No transform argument.
        Vec4 GetBBoxDiag() const;
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
        RwBool GetBBox(const Vec4* lpTransform, RwBool abTight, AABBox& arResult) const;

        // @ 0x82BA8580 -- 2 * radius on every axis.
        Vec4 GetBBoxDiag() const;
    };

    // The shared per-type descriptor table -- X360 dword_8327EEE0..dword_8327EEF8, seven
    // consecutive words (`static VTable* vTableArray[]`, rwccore.h:1597 / DWARF volume.h
    // :1521), filled by Volume::InitializeVTable @0x82BB03A8 -- DEFINED ONCE in
    // SDKs/EATech/rwcollision/volume.cpp. Slot 0 is null; slots 1..6 are the six real
    // descriptors. Index it with Volume::muVTableSlot (see that member).
    extern Volume::VTable* gVolumeVTable[E_VOLUMETYPE_NUMINTERNALTYPES];

    // The console's `lwz r11, 0x40(volume)` on the host: the descriptor the type enum in
    // the +0x40 slot names. Out-of-range enums map to the null slot 0 (the console would
    // read past the table; FixUp asserts on them first).
    inline const Volume::VTable* GetVolumeDescriptor(const Volume* lpVolume)
    {
        const u32 luType = lpVolume->muVTableSlot;
        return gVolumeVTable[luType < static_cast<u32>(E_VOLUMETYPE_NUMINTERNALTYPES) ? luType : 0u];
    }

    // --- layout pins (console offsets; all X360-asm-attested) -----------------------
    static_assert(sizeof(Vec4) == 16, "rw::collision::Vec4 must be one 16-byte row");
    static_assert(offsetof(Volume, maTransform)   == 0x00, "Volume::transform  @ +0x00");
    static_assert(offsetof(Volume, muVTableSlot)  == 0x40, "Volume::vTable     @ +0x40");
    static_assert(offsetof(Volume, mBoxData)      == 0x44, "Volume::boxData    @ +0x44");
    static_assert(offsetof(Volume, mfRadius)      == 0x50, "Volume::radius     @ +0x50");
    static_assert(offsetof(Volume, muGroupID)     == 0x54, "Volume::groupID    @ +0x54");
    static_assert(offsetof(Volume, muSurfaceID)   == 0x58, "Volume::surfaceID  @ +0x58");
    static_assert(offsetof(Volume, muFlags)       == 0x5C, "Volume::m_flags    @ +0x5C");
    static_assert(sizeof(Volume)       == 96, "rw::collision::Volume console stride is 96");
    static_assert(sizeof(BoxVolume)    == 96, "BoxVolume adds no storage over Volume");
    static_assert(sizeof(SphereVolume) == 96, "SphereVolume adds no storage over Volume");
    static_assert(offsetof(BoxVolume, mBoxData) == 0x44, "BoxVolume half-extents @ +0x44");

} // namespace collision
} // namespace rw
