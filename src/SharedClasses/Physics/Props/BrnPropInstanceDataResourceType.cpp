#include "SharedClasses/Physics/Props/BrnPropInstanceDataResourceType.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "types.hpp"

#include <cstddef>   // offsetof (porter-contract asserts)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnPhysics::Props::PropInstanceDataResourceType::GetSerialisedResourceDescriptor @ 0x8267B0C8
//   BrnPhysics::Props::PropInstanceDataResourceType::FixUp     @ 0x8267F7C8
//   BrnPhysics::Props::PropInstanceDataResourceType::GetTypeID @ 0x82675638
//
// The serialised payload is a BrnPhysics::Props::PropZoneData whose HOST layout is the
// platform-4 on-disk form (see BrnPhysicsPropZoneData.h + the porter contract asserts in
// FixUp below). FixUp relocates the two table pointers (maInstances then maCells, the X360
// order, null-preserving) and the descriptor sizes the resource from muSizeInBytes plus the
// header -- both by NAMED member access (this handler is a friend of PropZoneData), replacing
// the earlier raw-offset pokes that mixed X360 byte offsets with 8-byte reads.

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
    // On x64 the same fields sit at their host offsets (muSizeInBytes @0x18, header 0x28 ==
    // sizeof(PropZoneData) -- the porter emits muSizeInBytes relative to that header).
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
    // maCells at +0). The 64-bit base replaces the console's 32-bit delta arithmetic.
    void PropInstanceDataResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        // Porter-contract pins (world_type_transcode.py transcode_propinstancedata): the host
        // layout is the serialised form these relocations walk.
        static_assert(offsetof(PropZoneData, maCells)          == 0x00, "PropZoneData::maCells @ +0");
        static_assert(offsetof(PropZoneData, muNumCells)       == 0x08, "PropZoneData::muNumCells @ +8");
        static_assert(offsetof(PropZoneData, maInstances)      == 0x10, "PropZoneData::maInstances @ +0x10");
        static_assert(offsetof(PropZoneData, muSizeInBytes)    == 0x18, "PropZoneData::muSizeInBytes @ +0x18");
        static_assert(offsetof(PropZoneData, muZoneId)         == 0x24, "PropZoneData::muZoneId @ +0x24");
        static_assert(sizeof(PropZoneData) == 0x28, "PropZoneData header 0x28 (porter contract)");

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
