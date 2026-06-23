#include "SharedClasses/Physics/Props/BrnPropInstanceDataResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnPhysics::Props::PropInstanceDataResourceType::GetSerialisedResourceDescriptor @ 0x8267B0C8
//   BrnPhysics::Props::PropInstanceDataResourceType::FixUp     @ 0x8267F7C8
//   BrnPhysics::Props::PropInstanceDataResourceType::GetTypeID @ 0x82675638
//
// FixUp rebases two pointer fields (offsets 8 and 0) by the delta (the rw::Resource's
// load base), skipping either field when it is null.

namespace BrnPhysics
{
namespace Props
{
    static const uint32_t KU_PROP_INSTANCE_DATA_RESOURCE_TYPE_ID = 65553;
    static const u32      KU_PROP_ZONE_DATA_HEADER_SIZE          = 28;  // 0x1C: PropZoneData header bytes

    uint32_t PropInstanceDataResourceType::GetTypeID() const
    {
        return KU_PROP_INSTANCE_DATA_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x8267B0C8 (store-for-store). The serialised payload is a
    // BrnPhysics::Props::PropZoneData (DWARF: const PropZoneData* lpPropZoneData; u32 luSizeInBytes).
    // The X360 reads the PropZoneData::muSizeInBytes word at +0xC (the byte size of the trailing
    // instance/cell payload) and adds 0x1C (the 28-byte PropZoneData header):
    //   r9 = *(a3 + 0xC) (muSizeInBytes); r11 = r9 + 0x1C   -> entry0 size = muSizeInBytes + 28
    // entry0 align = 16; entry1..4 = {0,1}. The payload is read by its serialised dword offset
    // (muSizeInBytes is the 4th dword) rather than via the PropZoneData host struct (private field).
    CgsResource::ResourceDescriptor
    PropInstanceDataResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32* lpZone = static_cast<const u32*>(lpResource);
        const u32  luSizeInBytes = lpZone[0xC / 4];   // PropZoneData::muSizeInBytes @ +0xC
        const u32  luSize = luSizeInBytes + KU_PROP_ZONE_DATA_HEADER_SIZE;

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

    void PropInstanceDataResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        int       liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));
        uintptr_t lBase   = reinterpret_cast<uintptr_t>(lpResource);

        uintptr_t& lrField8 = *reinterpret_cast<uintptr_t*>(lBase + 8);
        if (lrField8)
            lrField8 += liDelta;

        uintptr_t& lrField0 = *reinterpret_cast<uintptr_t*>(lBase + 0);
        if (lrField0)
            lrField0 += liDelta;
    }
}
}
