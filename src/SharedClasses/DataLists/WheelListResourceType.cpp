#include "SharedClasses/DataLists/WheelListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::WheelListResourceType::FixUp     @ 0x8267DF28
//   BrnResource::WheelListResourceType::GetTypeID @ 0x826757B8
//
// FixUp rebases the pointer field at offset 4 by the delta (the rw::Resource's load base).

namespace BrnResource
{
    static const uint32_t KU_WHEEL_LIST_RESOURCE_TYPE_ID = 65545;

    uint32_t WheelListResourceType::GetTypeID() const
    {
        return KU_WHEEL_LIST_RESOURCE_TYPE_ID;
    }

    void WheelListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(lpResource) + 4) +=
            static_cast<int>(CgsResource::GetLoadBase(lrResource));
    }
}
