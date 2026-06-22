#include "GameShared/GameClasses/System/Resource/CgsResourceIdListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::IdListResourceType::FixDown @ 0x828F75D8
//   CgsResource::IdListResourceType::FixUp   @ 0x828F75F8
//
// The serialised resource is a CgsResource::ResourceIdList header whose leading
// word (mpaIds) is a file-relative pointer. FixDown/FixUp rebase that one pointer
// by the load-base delta, which the X360 reads from the first word of the
// rw::Resource arg (rw::Resource::m_baseResources[0]). The pointer is only adjusted
// when non-zero (a null id-array stays null). The header is accessed by its leading
// dword (the serialised layout, not a host struct), matching the X360 pseudocode.

namespace CgsResource
{
    // FixDown: file-relative-ise the id-array pointer (subtract the delta), only if
    // non-zero. @ 0x828F75D8
    void IdListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        u32* lpIds = reinterpret_cast<u32*>(lpResource);
        if (*lpIds)
            *lpIds -= reinterpret_cast<u32>(lrResource.m_baseResources[0]);
    }

    // FixUp: rebase the id-array pointer to the loaded address (add the delta), only
    // if non-zero. @ 0x828F75F8
    void IdListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        u32* lpIds = reinterpret_cast<u32*>(lpResource);
        if (*lpIds)
            *lpIds += reinterpret_cast<u32>(lrResource.m_baseResources[0]);
    }
}
