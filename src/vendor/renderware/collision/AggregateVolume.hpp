#pragma once

// ===========================================================================
// rw::collision::AggregateVolume -- a collision Volume whose shape is delegated
// to a collision Aggregate (a ClusteredMesh / KDTree procedural). The volume
// carries its own affine transform in the Volume base and a pointer to the
// aggregate in the Volume-base union slot; its bounding-box queries read the
// aggregate's cached AABBox (Aggregate::m_AABB @ +0x00) and transform it.
//
// OWNING HOME for the two AggregateVolume functions the X360 binary defines:
//     rw::collision::AggregateVolume::GetBBox      @ 0x82BBBA10
//     rw::collision::AggregateVolume::GetBBoxDiag  @ 0x82BBBB58
//
// LAYOUT (attested by the asm; DWARF aggregatevolume.h names). It is the SAME 96-byte
// serialised rw::collision::Volume record CollisionVolume.hpp models, with the union arm
// spelled as the aggregate pointer:
//   Matrix44Affine mTransform    @ +0x00  Volume-base transform (64B; Volume::transform
//                                          sits at +0x00, so &mTransform == this)
//   u32            muVTableSlot  @ +0x40  the per-TYPE descriptor slot (host: the enum)
//   Aggregate*     <maAggregate> @ +0x44  Volume-base union slot (Set/GetAggregate)
//   f32            mfRadius      @ +0x50
//   u32            muGroupID     @ +0x54
//   u32            muSurfaceID   @ +0x58
//   u32            muFlags       @ +0x5C  -> sizeof == 96
//
// The cached bounding box read by both bodies is Aggregate::m_AABB @ +0x00 (the
// aggregate's first member, aggregate.h:341), reached by reinterpreting the
// Aggregate* as a const AABBox*.
//
// 🔴 DEFECT FIXED 2026-08-19 (wave Q5 vtbind) -- THE AGGREGATE POINTER WAS AT THE WRONG
// OFFSET, and it was live. This class used to declare
//     unsigned char maVTablePad[4];   // +0x40
//     Aggregate*    mpAggregate;      // "+0x44"
// but `Aggregate*` is 8-byte aligned on x64, so MSVC pushed it to **+0x48** -- MEASURED
// with offsetof (scratchpad/waveQ5/probe_vtbind/measure_agg.cpp). Both bodies below
// dereference it, and both of their comments said "+0x44". The rest of the tree writes
// and reads the aggregate pointer at +0x44 (VolumeBBoxQuery.cpp:104 does exactly
// `*(Aggregate* const*)((const u8*)volume + 0x44)`), which is also the console offset, so
// the moment a real aggregate volume reached either body it would have loaded four bytes
// of the pointer plus four bytes of Volume::radius and dereferenced the result. Same
// class as the console-value corruptions AGENTS.md lists, arrived at from the other
// direction: an x64 alignment rule silently moving a field off a pinned offset.
// The offset is now pinned by static_asserts at the foot of this file, so it cannot
// drift again, and the pointer lives in RAW STORAGE with accessors because +0x44 is not
// an 8-byte-aligned address.
//
// FLAG (unchanged): this class is still a standalone home rather than
// `: public rw::collision::Volume`. It cannot derive from the CollisionVolume.hpp
// Volume today because its two bodies trade in the EATech `math::vpu` vocabulary
// (Matrix44Affine / Mult / AABBox::Transform) while CollisionVolume.hpp is deliberately
// vocabulary-neutral; the two headers CAN now be co-included (MEASURED,
// scratchpad/waveQ5/probe_vtbind/measure_coinclude.cpp), so the merge is a follow-up
// with a forcing function, not a blocker. The descriptor slot for type 6 binds this
// class through a documented reinterpret in VolumeVTables.cpp, which the layout pins
// below are what make sound.
// ===========================================================================

#include <cstddef>   // offsetof (layout pins)
#include <cstring>   // memcpy (the unaligned +0x44 pointer accessors)

#include "types.hpp"
#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/Aggregate.hpp"
#include "SDKs/EATech/include/rw/math/vpu/matrix44.h"   // math::vpu::Matrix44 / Matrix44Affine + Mult

namespace rw
{
namespace collision
{

// RenderWare boolean (canonical RwBool; the vendor typedef is a 32-bit int). Identical
// to the typedef in FeatureEdge.hpp / provided the same way there -- an identical typedef
// in the same namespace is legal, so co-inclusion with those headers does not clash.
typedef s32 RwBool;

class AggregateVolume
{
public:
    // @ 0x82BBBA10 -- return the aggregate's cached box, transformed by (this->mTransform
    // composed with lpTransform) when lpTransform is non-null, else by this->mTransform.
    // The RwBool second arg is part of the ABI shape but is not consumed by the body.
    RwBool GetBBox(const math::vpu::Matrix44Affine* lpTransform,
                   RwBool abInstanced,
                   AABBox& arBBox) const;

    // @ 0x82BBBB58 -- return the (max - min) diagonal of the aggregate's cached box
    // transformed by this->mTransform.
    math::vpu::Vector3 GetBBoxDiag() const;

    // The Volume-base union's aggregate arm, console +0x44 (`lwz r11, 0x44(r3)` at the
    // head of both bodies). On the console it is a 4-byte pointer; the host convention
    // for this arm -- set by VolumeBBoxQuery.cpp:104, which this class must agree with --
    // is a full 8-byte host pointer written at the SAME offset, spanning +0x44..+0x4B.
    // The union arm is 12 bytes and an aggregate volume uses no other word of it, so it
    // fits. +0x44 is not 8-aligned, so the storage is raw bytes and these two accessors
    // are the only way in or out.
    Aggregate* GetAggregate() const
    {
        Aggregate* lpAggregate;
        std::memcpy(&lpAggregate, maAggregate, sizeof(lpAggregate));
        return lpAggregate;
    }

    void SetAggregate(Aggregate* lpAggregate)
    {
        std::memcpy(maAggregate, &lpAggregate, sizeof(lpAggregate));
    }

    math::vpu::Matrix44Affine mTransform;      // +0x00 Volume-base transform
    u32                       muVTableSlot;    // +0x40 the per-TYPE descriptor slot
                                               //       (host: the EVolumeType enum, 6)
    unsigned char             maAggregate[8];  // +0x44 union arm: the host Aggregate*
    unsigned char             maUnused4C[4];   // +0x4C the union arm's 12th..15th bytes
    f32                       mfRadius;        // +0x50 Volume::radius
    u32                       muGroupID;       // +0x54 Volume::groupID
    u32                       muSurfaceID;     // +0x58 Volume::surfaceID
    u32                       muFlags;         // +0x5C Volume::m_flags
};

// --- layout pins (console offsets; the reinterpret in VolumeVTables.cpp rests on these)
static_assert(offsetof(AggregateVolume, mTransform)   == 0x00, "AggregateVolume::transform");
static_assert(offsetof(AggregateVolume, muVTableSlot) == 0x40, "AggregateVolume::vTable slot");
static_assert(offsetof(AggregateVolume, maAggregate)  == 0x44,
              "the aggregate pointer MUST sit at +0x44 -- a bare Aggregate* member lands "
              "at +0x48 on x64 and that bug was live until 2026-08-19");
static_assert(offsetof(AggregateVolume, mfRadius)     == 0x50, "AggregateVolume::radius");
static_assert(offsetof(AggregateVolume, muFlags)      == 0x5C, "AggregateVolume::m_flags");
static_assert(sizeof(AggregateVolume) == 96,
              "AggregateVolume models the same 96-byte serialised Volume record");

} // namespace collision
} // namespace rw
