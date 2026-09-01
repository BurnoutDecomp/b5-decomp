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
// LAYOUT NOTE (X360 32-bit vs host 64-bit):
//   The three records embed Model*/PropPartGraphics* pointers, so their strides
//   WIDEN on a 64-bit host (PropGraphics 12 -> 24, PropPartGraphics 12 -> 16;
//   PropGraphicsList tail pointers move from +0x10/+0x14 to +0x10/+0x18). The
//   HOST layout *is* the platform-4 serialised form: the porter
//   (tools/assets/bundles/world_type_transcode.py transcode_propgraphicslist)
//   rebuilds the X360 blob to these strides, recomputes muSizeInBytes, and
//   rewrites the bundle import offsets to the widened Model slots (+8), so the
//   FixUp/FixDown walks and the import arithmetic index by named members. The
//   static_asserts at the bottom pin that porter contract.
//
// `Model` (the RenderWare model resource each element imports) is a foreign type
// homed elsewhere; only its address is stored here, so it is forward-declared.
// =============================================================================

namespace rw
{
    struct Resource;   // Paradise rw::Resource : BaseResources<5>
}

namespace CgsGraphics { class Model; }
// The RenderWare model resource these records import IS CgsGraphics::Model (the type
// CgsModel.h defines and the whole renderer takes). The old global `class Model;` was a
// DISTINCT, never-defined type, so every consumer that handed mpPropModel to the renderer
// had to reinterpret_cast across two unrelated types -- the prop draw path cannot. Aliased
// rather than renamed so BrnPropGraphicsList.cpp / ...ResourceType.cpp keep their spelling.
using Model = CgsGraphics::Model;

namespace BrnPhysics
{
namespace Props
{
    // One per whole prop. X360 stride 12 { muTypeId@0, mpPropModel@4, mpParts@8 };
    // the two pointers widen on host (stride 24 -- the porter contract below).
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
        // Load fix-up (X360 @0x8267DB38): rebase the two table pointers and each
        // PropGraphics element's mpParts by the resource's base data address, then
        // tripwire-assert muSizeInBytes sanity. The Model slots are NOT touched --
        // they are bundle import slots the pool resolution writes.
        void FixUp(const rw::Resource& lrBaseResource);

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

    // Pin the pointer-free prefixes (X360-faithful offsets) AND the widened
    // tail -- the host layout is the platform-4 serialised contract the porter
    // (world_type_transcode.py transcode_propgraphicslist) emits.
    static_assert(offsetof(PropGraphicsList, muSizeInBytes)            == 0x00, "PropGraphicsList::muSizeInBytes @ +0");
    static_assert(offsetof(PropGraphicsList, muZoneNumber)            == 0x04, "PropGraphicsList::muZoneNumber @ +4");
    static_assert(offsetof(PropGraphicsList, muNumberOfPropModels)    == 0x08, "PropGraphicsList::muNumberOfPropModels @ +8");
    static_assert(offsetof(PropGraphicsList, muNumberOfPropPartModels) == 0x0C, "PropGraphicsList::muNumberOfPropPartModels @ +0xC");
    static_assert(offsetof(PropGraphicsList, mpaPropGraphics)         == 0x10, "PropGraphicsList::mpaPropGraphics @ +0x10 (porter contract)");
    static_assert(offsetof(PropGraphicsList, mpaPropPartGraphics)     == 0x18, "PropGraphicsList::mpaPropPartGraphics @ +0x18 (porter contract)");
    static_assert(sizeof(PropGraphicsList) == 0x20, "PropGraphicsList header 0x20 (porter contract)");
    static_assert(offsetof(PropGraphics, muTypeId)     == 0x00, "PropGraphics::muTypeId @ +0");
    static_assert(offsetof(PropGraphics, mpPropModel)  == 0x08, "PropGraphics::mpPropModel @ +8 (porter import slot)");
    static_assert(offsetof(PropGraphics, mpParts)      == 0x10, "PropGraphics::mpParts @ +0x10 (porter contract)");
    static_assert(sizeof(PropGraphics) == 24, "PropGraphics stride 24 (porter contract)");
    static_assert(offsetof(PropPartGraphics, muTypeId) == 0x00, "PropPartGraphics::muTypeId @ +0");
    static_assert(offsetof(PropPartGraphics, muPartId) == 0x04, "PropPartGraphics::muPartId @ +4");
    static_assert(offsetof(PropPartGraphics, mpPropModel) == 0x08, "PropPartGraphics::mpPropModel @ +8 (porter import slot)");
    static_assert(sizeof(PropPartGraphics) == 16, "PropPartGraphics stride 16 (porter contract)");

} // namespace Props
} // namespace BrnPhysics

#endif // BRN_PHYSICS_PROPS_PROP_GRAPHICS_LIST_H
