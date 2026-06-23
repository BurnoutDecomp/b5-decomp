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
// No DWARF hints exist for this TU, so the LAYOUT below is
// reconstructed from the copy widths/offsets in the X360 asm (the copy is the
// only evidence of the field shape):
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

    u32  muVolumePtr;     // +0x00  word   (volume pointer / id)
    u32  muTag;           // +0x04  word
    u32  mPad08[2];       // +0x08  unwritten alignment slot (+0x08..+0x0F)
    Vec4 mRow0;           // +0x10  transform row 0
    Vec4 mRow1;           // +0x20  transform row 1
    Vec4 mRow2;           // +0x30  transform row 2
    Vec4 mRow3;           // +0x40  transform row 3 / bbox
    u64  mu50;            // +0x50  8-byte field
    u64  mu58;            // +0x58  8-byte field
    u64  mu60;            // +0x60  8-byte field
    u64  mu68;            // +0x68  8-byte field
    u32  mu70;            // +0x70  word
    u8   mu74;            // +0x74  byte
};

} // namespace collision
} // namespace rw
