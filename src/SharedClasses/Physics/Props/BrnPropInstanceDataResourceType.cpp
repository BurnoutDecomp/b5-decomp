#include "SharedClasses/Physics/Props/BrnPropInstanceDataResourceType.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "types.hpp"

#include <cstddef>   // offsetof (native layout asserts)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnPhysics::Props::PropInstanceDataResourceType::GetSerialisedResourceDescriptor @ 0x8267B0C8
//   BrnPhysics::Props::PropInstanceDataResourceType::FixUp     @ 0x8267F7C8
//   BrnPhysics::Props::PropInstanceDataResourceType::GetTypeID @ 0x82675638
//
// The serialised payload is a native BrnPhysics::Props::PropZoneData (see the
// layout asserts in FixUp below). FixUp relocates its two table pointers
// (maInstances then maCells, null-preserving) and the descriptor sizes the resource from muSizeInBytes plus the
// header -- both by NAMED member access (this handler is a friend of PropZoneData), replacing
// the earlier raw-offset pokes.

namespace BrnPhysics
{
namespace Props
{
    static const uint32_t KU_PROP_INSTANCE_DATA_RESOURCE_TYPE_ID = 65553;

    uint32_t PropInstanceDataResourceType::GetTypeID() const
    {
        return KU_PROP_INSTANCE_DATA_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x8267B0C8 (store-for-store). The X360 reads
    // PropZoneData::muSizeInBytes (the byte size of everything after the header) and adds the
    // header size:  r9 = *(a3 + 0xC); r11 = r9 + 0x1C  -> entry0 size = muSizeInBytes + 0x1C.
    // entry0 align = 16; entry1..4 = {0,1}.
    CgsResource::ResourceDescriptor
    PropInstanceDataResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const PropZoneData* lpZone = static_cast<const PropZoneData*>(lpResource);
        const u32 luSize = lpZone->muSizeInBytes + static_cast<u32>(sizeof(PropZoneData));

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;   // entry0 size
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;      // entry0 align
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // FixUp @ 0x8267F7C8. Relocate the serialised table offsets by the resource's load base,
    // null-preserving, in the X360 store order (maInstances -- the X360 +8 slot -- first, then
    // maCells at +0). Pointer-sized arithmetic retains the native 32-bit delta.
    void PropInstanceDataResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        // ARTIST FixUp @0x8267F7C8 and descriptor @0x8267B0C8 use the
        // native pointer slots and 0x1C-byte header.
        CGS_STATIC_ASSERT((offsetof(PropZoneData, maCells) == 0x0), "PropZoneData::maCells @0x0");
        CGS_STATIC_ASSERT((offsetof(PropZoneData, muNumCells) == 0x4), "PropZoneData::muNumCells @0x4");
        CGS_STATIC_ASSERT((offsetof(PropZoneData, maInstances) == 0x8), "PropZoneData::maInstances @0x8");
        CGS_STATIC_ASSERT((offsetof(PropZoneData, muSizeInBytes) == 0xC), "PropZoneData::muSizeInBytes @0xC");
        CGS_STATIC_ASSERT((offsetof(PropZoneData, muNumberOfInstances) == 0x10), "PropZoneData::muNumberOfInstances @0x10");
        CGS_STATIC_ASSERT((offsetof(PropZoneData, muNumberOfProps) == 0x14), "PropZoneData::muNumberOfProps @0x14");
        CGS_STATIC_ASSERT((offsetof(PropZoneData, muZoneId) == 0x18), "PropZoneData::muZoneId @0x18");
        CGS_STATIC_ASSERT((sizeof(PropZoneData) == 0x1C), "native prop zone header size");
        CGS_STATIC_ASSERT((__alignof(PropZoneData) == 4), "native prop zone header alignment");

        PropZoneData*   lpZone  = static_cast<PropZoneData*>(lpResource);
        const uintptr_t luDelta = CgsResource::GetLoadBase64(lrResource);

        if (lpZone->maInstances)
            lpZone->maInstances = reinterpret_cast<PropInstanceData*>(
                reinterpret_cast<uintptr_t>(lpZone->maInstances) + luDelta);

        if (lpZone->maCells)
            lpZone->maCells = reinterpret_cast<PropCellData*>(
                reinterpret_cast<uintptr_t>(lpZone->maCells) + luDelta);
    }
}
}
