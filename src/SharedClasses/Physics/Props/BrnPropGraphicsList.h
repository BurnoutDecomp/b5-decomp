#ifndef BRN_PHYSICS_PROPS_PROP_GRAPHICS_LIST_H
#define BRN_PHYSICS_PROPS_PROP_GRAPHICS_LIST_H

#include "types.hpp"

#include <cstddef>   // offsetof (layout static_asserts below)

// =============================================================================
// BrnPhysics::Props::PropGraphicsList  (resource 0x10010)
//   DWARF home: SharedClasses/Physics/Props/BrnPropGraphicsList.{h,cpp}.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX + the shipped resource spec
// (docs/PropGraphicsList.md, verified against the decomp). The list carries two
// parallel arrays: PropGraphics (one per whole prop -> its body Model) and
// PropPartGraphics (one per destructible part -> its own Model).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit) -- CRITICAL, per wave rule 4:
//   The three records embed Model*/PropPartGraphics* pointers, so their strides
//   WIDEN on a 64-bit host (PropGraphics/PropPartGraphics: 12 bytes on X360, 24 on
//   host; PropGraphicsList pointers move from +0x10/+0x14 to +0x10/+0x18). Only the
//   pointer-free head offsets are host-stable, so only those are static_assert'd
//   below. FixDown walks the SERIALISED (X360, 12-byte-stride) blob by raw byte
//   arithmetic, independent of the host struct stride.
//
// `Model` (the RenderWare model resource each element imports) is a foreign type
// homed elsewhere; only its address is stored here, so it is forward-declared.
// =============================================================================

namespace rw
{
    struct Resource;   // rw::Resource : BaseResources<4> (rw/rwcore_structs.h)
}

class Model;           // foreign RenderWare model resource (pointer-only here)

namespace BrnPhysics
{
namespace Props
{
    // One per whole prop. X360 stride 12 { muTypeId@0, mpPropModel@4, mpParts@8 };
    // the two pointers widen on host so the stride is not asserted.
    struct PropPartGraphics;

    struct PropGraphics
    {
        u32     muTypeId;       // +0x00  prop type id (index into the prop-types table)
        Model*  mpPropModel;    // X360 +0x04 (host-widened) -- body Model (0 on disk)
        PropPartGraphics* mpParts;  // X360 +0x08 -- this prop's first part (grouped by type)
    };

    // One per destructible part. X360 stride 12 { muTypeId@0, muPartId@4, mpPropModel@8 }.
    struct PropPartGraphics
    {
        u32     muTypeId;       // +0x00  owning prop's type id (parts grouped by this)
        u32     muPartId;       // +0x04  part index within the owning prop
        Model*  mpPropModel;    // X360 +0x08 (host-widened) -- this part's Model (0 on disk)
    };

    // The serialised prop-graphics list. Pointer-free prefix (through +0x10) is
    // host-stable; the two table pointers widen (+0x14 -> +0x18 on host).
    struct PropGraphicsList
    {
        // Serialise-out fix-up: rebase every embedded pointer to a base-relative
        // offset (null-preserving) using the resource's base data address.
        void FixDown(const rw::Resource& lrBaseResource);

        // Bounds-checked element accessors (non-gating tripwire asserts).
        PropGraphics*     GetPropGraphics(u32 luIndex);
        PropPartGraphics* GetPartGraphics(u32 luIndex);

        // ---- console-faithful member layout (pointer-free head + widening tail) ----
        u32               muSizeInBytes;             // +0x00
        u32               muZoneNumber;              // +0x04
        u32               muNumberOfPropModels;      // +0x08
        u32               muNumberOfPropPartModels;  // +0x0C
        PropGraphics*     mpaPropGraphics;           // X360 +0x10 (host-widened)
        PropPartGraphics* mpaPropPartGraphics;       // X360 +0x14 (host +0x18)
    };

    // Pin ONLY the pointer-free prefixes (rule 4: do not assert past the first
    // pointer of a widening record).
    static_assert(offsetof(PropGraphicsList, muSizeInBytes)            == 0x00, "PropGraphicsList::muSizeInBytes @ +0");
    static_assert(offsetof(PropGraphicsList, muZoneNumber)            == 0x04, "PropGraphicsList::muZoneNumber @ +4");
    static_assert(offsetof(PropGraphicsList, muNumberOfPropModels)    == 0x08, "PropGraphicsList::muNumberOfPropModels @ +8");
    static_assert(offsetof(PropGraphicsList, muNumberOfPropPartModels) == 0x0C, "PropGraphicsList::muNumberOfPropPartModels @ +0xC");
    static_assert(offsetof(PropGraphics, muTypeId)     == 0x00, "PropGraphics::muTypeId @ +0");
    static_assert(offsetof(PropPartGraphics, muTypeId) == 0x00, "PropPartGraphics::muTypeId @ +0");
    static_assert(offsetof(PropPartGraphics, muPartId) == 0x04, "PropPartGraphics::muPartId @ +4");

} // namespace Props
} // namespace BrnPhysics

#endif // BRN_PHYSICS_PROPS_PROP_GRAPHICS_LIST_H
