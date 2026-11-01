#pragma once

#include <cstddef>
#include "types.hpp"
#include "vendor/renderware/collision/AABBox.hpp"

namespace rw { namespace collision {

class Volume;

// DecFIGS volume.h:1092–1098; ARTIST 82BB33E8 copies only these fields.
struct VolRef
{
    typedef rw::collision::Volume Volume;
    VolRef& operator=(const VolRef& rOther);

    const Volume* volume;
    rw::math::vpu::Matrix44Affine* tm;
    rw::math::vpu::Matrix44Affine tmContents;
    AABBox bBox;
    u32 tag;
    u8 numTagBits;
};

CGS_STATIC_ASSERT((sizeof(VolRef) == 0x80), "Native VolRef size");
CGS_STATIC_ASSERT((__alignof(VolRef) == 16), "Native VolRef alignment");
CGS_STATIC_ASSERT((offsetof(VolRef, volume) == 0x00), "VolRef volume");
CGS_STATIC_ASSERT((offsetof(VolRef, tm) == 0x04), "VolRef transform pointer");
CGS_STATIC_ASSERT((offsetof(VolRef, tmContents) == 0x10), "VolRef transform");
CGS_STATIC_ASSERT((offsetof(VolRef, bBox) == 0x50), "VolRef box");
CGS_STATIC_ASSERT((offsetof(VolRef, tag) == 0x70), "VolRef tag");
CGS_STATIC_ASSERT((offsetof(VolRef, numTagBits) == 0x74), "VolRef tag bits");

} }
