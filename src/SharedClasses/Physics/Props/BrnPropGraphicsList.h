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
// Native ARTIST layout: 24-byte list header, and 12-byte graphics/part
// records. The guards below pin every field and both array strides.
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
typedef CgsGraphics::Model Model;

namespace BrnPhysics
{
namespace Props
{
    // One per whole prop. X360 stride 12 { muTypeId@0, mpPropModel@4, mpParts@8 };
    // both pointer members occupy one native word.
    struct PropPartGraphics;

    struct PropGraphics
    {
        u32     muTypeId;       // +0x00  prop type id (index into the prop-types table)
        Model*  mpPropModel;    // native +0x04 -- body Model (0 on disk)
        PropPartGraphics* mpParts;  // X360 +0x08 -- this prop's first part (grouped by type)
    };

    // One per destructible part. X360 stride 12 { muTypeId@0, muPartId@4, mpPropModel@8 }.
    struct PropPartGraphics
    {
        u32     muTypeId;       // +0x00  owning prop's type id (parts grouped by this)
        u32     muPartId;       // +0x04  part index within the owning prop
        Model*  mpPropModel;    // native +0x08 -- this part's Model (0 on disk)
    };

    // The serialised prop-graphics list has a 16-byte pointer-free prefix
    // followed by the two native table pointers.
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

        // ---- native member layout ----
        u32               muSizeInBytes;             // +0x00
        u32               muZoneNumber;              // +0x04
        u32               muNumberOfPropModels;      // +0x08
        u32               muNumberOfPropPartModels;  // +0x0C
        PropGraphics*     mpaPropGraphics;           // native +0x10
        PropPartGraphics* mpaPropPartGraphics;       // native +0x14
    };

    // ARTIST FixUp/FixDown and both getters attest these offsets and strides.
    CGS_STATIC_ASSERT((offsetof(PropGraphicsList, muSizeInBytes) == 0x0), "PropGraphicsList::muSizeInBytes @0x0");
    CGS_STATIC_ASSERT((offsetof(PropGraphicsList, muZoneNumber) == 0x4), "PropGraphicsList::muZoneNumber @0x4");
    CGS_STATIC_ASSERT((offsetof(PropGraphicsList, muNumberOfPropModels) == 0x8), "PropGraphicsList::muNumberOfPropModels @0x8");
    CGS_STATIC_ASSERT((offsetof(PropGraphicsList, muNumberOfPropPartModels) == 0xC), "PropGraphicsList::muNumberOfPropPartModels @0xC");
    CGS_STATIC_ASSERT((offsetof(PropGraphicsList, mpaPropGraphics) == 0x10), "PropGraphicsList::mpaPropGraphics @0x10");
    CGS_STATIC_ASSERT((offsetof(PropGraphicsList, mpaPropPartGraphics) == 0x14), "PropGraphicsList::mpaPropPartGraphics @0x14");
    CGS_STATIC_ASSERT((sizeof(PropGraphicsList) == 24), "native PropGraphicsList stride");
    CGS_STATIC_ASSERT((__alignof(PropGraphicsList) == 4), "native PropGraphicsList alignment");
    CGS_STATIC_ASSERT((offsetof(PropGraphics, muTypeId) == 0x0), "PropGraphics::muTypeId @0x0");
    CGS_STATIC_ASSERT((offsetof(PropGraphics, mpPropModel) == 0x4), "PropGraphics::mpPropModel @0x4");
    CGS_STATIC_ASSERT((offsetof(PropGraphics, mpParts) == 0x8), "PropGraphics::mpParts @0x8");
    CGS_STATIC_ASSERT((sizeof(PropGraphics) == 12), "native PropGraphics stride");
    CGS_STATIC_ASSERT((__alignof(PropGraphics) == 4), "native PropGraphics alignment");
    CGS_STATIC_ASSERT((offsetof(PropPartGraphics, muTypeId) == 0x0), "PropPartGraphics::muTypeId @0x0");
    CGS_STATIC_ASSERT((offsetof(PropPartGraphics, muPartId) == 0x4), "PropPartGraphics::muPartId @0x4");
    CGS_STATIC_ASSERT((offsetof(PropPartGraphics, mpPropModel) == 0x8), "PropPartGraphics::mpPropModel @0x8");
    CGS_STATIC_ASSERT((sizeof(PropPartGraphics) == 12), "native PropPartGraphics stride");
    CGS_STATIC_ASSERT((__alignof(PropPartGraphics) == 4), "native PropPartGraphics alignment");

} // namespace Props
} // namespace BrnPhysics

#endif // BRN_PHYSICS_PROPS_PROP_GRAPHICS_LIST_H
