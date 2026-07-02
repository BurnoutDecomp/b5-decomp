#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::VolRef -- a "volume reference": a volume pointer plus a cached
// copy of the volume's transform and bounding box, used by the collision query
// objects (VolumeLineQuery / VolumeBBoxQuery store an array of these as their
// result set).
//
// OWNING HOME for the single VolRef function the X360 binary defines:
//     rw::collision::VolRef::operator=  @ 0x82BB33E8
//
// No DWARF layout exists for this TU, so the LAYOUT below is reconstructed
// from the copy widths/offsets in the X360 asm (the copy is the only evidence
// of the field shape):
//     +0x00  word              lwz/stw
//     +0x04  word              lwz/stw
//     +0x10  16-byte vector     lvx128/stvx128   (transform row 0)
//     +0x20  16-byte vector     lvx128/stvx128   (transform row 1)
//     +0x30  16-byte vector     lvx128/stvx128   (transform row 2)
//     +0x40  16-byte vector     lvx128/stvx128   (transform row 3 / bbox)
//     +0x50  8-byte             ld/std
//     +0x58  8-byte             ld/std
//     +0x60  8-byte             ld/std
//     +0x68  8-byte             ld/std
//     +0x70  word               lwz/stw
//     +0x74  byte               lbz/stb
// Members are named per that layout so the assignment lands each field at the
// observed offset; the gap at +0x08..+0x0F is an unwritten alignment slot.
//
// FIELD MEANINGS (attested by this wave's consumers -- see
// rw::collision::PrimitiveBatchIntersect @ 0x82BABC78, which reads +0x00 as
// the Volume* it vtable-dispatches on, +0x04 as the cached transform pointer
// it passes to CreateGPInstance, and +0x70 as the tag word it copies into the
// GPInstance; and rw::collision::AddQueryResult @ 0x82BB1588, which fills the
// VolRef embedded in a VolumeLineSegIntersectResult: volume @+0x00, transform
// pointer @+0x04 aimed at the inline rows @+0x10, tag @+0x70, tag bit count
// @+0x74. The committed SDKs/EATech/rwcollision/volumelinequery.cpp records
// the same shape for its 128-byte entries.)
// ===========================================================================

namespace rw
{
namespace collision
{

class VolRef
{
public:
    // 16-byte (xyzw) vector row matching a VMX register.
    struct Vec4
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };

    // @ 0x82BB33E8 -- copy-assign all cached fields from rOther.
    VolRef& operator=(const VolRef& rOther);

    u32  muVolumePtr;     // +0x00  the referenced Volume (console pointer word)
    u32  muTransformPtr;  // +0x04  cached transform pointer (may aim at mRow0)
    u32  mPad08[2];       // +0x08  unwritten alignment slot (+0x08..+0x0F)
    Vec4 mRow0;           // +0x10  transform row 0
    Vec4 mRow1;           // +0x20  transform row 1
    Vec4 mRow2;           // +0x30  transform row 2
    Vec4 mRow3;           // +0x40  transform row 3 / bbox
    u64  mu50;            // +0x50  8-byte field
    u64  mu58;            // +0x58  8-byte field
    u64  mu60;            // +0x60  8-byte field
    u64  mu68;            // +0x68  8-byte field
    u32  muTag;           // +0x70  aggregate/unit tag word
    u8   muNumTagBits;    // +0x74  tag bit-count byte

    // ADDITIVE GROW (rw-physics-collision group): pad VolRef to its true 0x80
    // stride. The X360 collision query objects (VolumeBBoxQuery / VolumeVolumeQuery)
    // index their result pool and embed a "current" VolRef using `idx << 7` (0x80)
    // -- so the on-object stride is 0x80, not the 0x78 the written fields imply.
    // This trailing pad makes sizeof(VolRef)==0x80 so embedded/strided VolRefs land
    // at the observed offsets. Purely additive: no existing member moves.
    u8   mPad75[11];      // +0x75..+0x7F  (unwritten tail; stride padding)
};

static_assert(sizeof(VolRef) == 0x80, "VolRef on-object stride must be 0x80");

} // namespace collision
} // namespace rw
