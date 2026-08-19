#pragma once

#include <cstddef>   // offsetof (layout pins)
#include <cstdint>   // uintptr_t (host-width pointer words)

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
//     +0x00  word              lwz/stw   (console 32-bit Volume*)
//     +0x04  word              lwz/stw   (console 32-bit transform*)
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
//
// HOST-WIDTH PROMOTION (waveQ5 C1, 2026-08-18) -- AGENTS gotcha 1.
// The two leading fields are POINTERS, not console values. Carrying them as
// `u32` meant every writer had to truncate a 64-bit host pointer
// (`static_cast<u32>(reinterpret_cast<uintptr_t>(p))`) and every reader
// re-widened the truncated word back into a pointer it then DEREFERENCED --
// PrimitiveBatchIntersect -> CreateGPInstance, VolumeBBoxQuery::GetOverlaps,
// VolumeVolumeQuery's culler. Nothing had executed that path yet only because
// VolumeVolumeQuery::Initialize was still gated to NULL; retiring that gate
// turns the truncation into a live wild-pointer dereference.
// They are therefore `uintptr_t` (host width) here. THE STRIDE IS UNCHANGED:
// the console leaves +0x08..+0x0F unwritten (VolRef::operator= @0x82BB33E8
// copies +0x00, +0x04, then jumps to +0x10), so the two extra 4-byte halves
// land exactly in that hole -- mRow0 stays at +0x10, every later member keeps
// its offset, the trailing mPad75[11] is untouched and sizeof stays 0x80.
// (The pre-promotion plan assumed the pad tail would have to shrink by 8; the
// +0x08 alignment slot absorbs it instead, which is why nothing moves. The
// offsets are pinned by static_asserts below so a future edit cannot drift.)
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

    // HOST WIDTH (see the banner above): the console stores one 32-bit word
    // each at +0x00/+0x04 and leaves +0x08..+0x0F unwritten; on x64 the two
    // pointers occupy +0x00..+0x0F and that unwritten slot disappears.
    uintptr_t muVolumePtr;     // +0x00  the referenced Volume
    uintptr_t muTransformPtr;  // +0x08 (console +0x04)  cached transform
                               //       pointer (may aim at mRow0), 0 = none
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

// The stride the query objects index with (`idx << 7`).
static_assert(sizeof(VolRef) == 0x80, "VolRef on-object stride must be 0x80");

// Everything from the transform rows on keeps its console offset after the
// host-width promotion of the two leading pointer words (the +0x08..+0x0F
// unwritten slot absorbs the growth exactly).
static_assert(offsetof(VolRef, muVolumePtr)    == 0x00, "VolRef::muVolumePtr");
static_assert(offsetof(VolRef, mRow0)          == 0x10, "VolRef::mRow0");
static_assert(offsetof(VolRef, mRow1)          == 0x20, "VolRef::mRow1");
static_assert(offsetof(VolRef, mRow2)          == 0x30, "VolRef::mRow2");
static_assert(offsetof(VolRef, mRow3)          == 0x40, "VolRef::mRow3");
static_assert(offsetof(VolRef, mu50)           == 0x50, "VolRef::mu50");
static_assert(offsetof(VolRef, mu68)           == 0x68, "VolRef::mu68");
static_assert(offsetof(VolRef, muTag)          == 0x70, "VolRef::muTag");
static_assert(offsetof(VolRef, muNumTagBits)   == 0x74, "VolRef::muNumTagBits");

} // namespace collision
} // namespace rw
