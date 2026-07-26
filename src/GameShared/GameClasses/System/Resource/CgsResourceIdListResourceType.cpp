#include "GameShared/GameClasses/System/Resource/CgsResourceIdListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"  // GetLoadBase64
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"        // CgsResource::ID
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::IdListResourceType::FixDown @ 0x828F75D8
//   CgsResource::IdListResourceType::FixUp   @ 0x828F75F8
//
// The serialised resource is a CgsResource::ResourceIdList header whose leading
// member (mpaIds) is a file-relative pointer. FixDown/FixUp rebase that one pointer
// by the load-base delta, which the X360 reads from the first word of the
// rw::Resource arg (rw::Resource::m_baseResources[0]). The pointer is only adjusted
// when non-zero (a null id-array stays null).
//
// SEAM (platform-4 widened blob): the converted x64 data widens the header to
// { ID* mpaIds u64 slot @+0, u32 muNumIds @+8, pad; the u64 id array follows @+0x10 },
// matching the committed CgsResource::ResourceIdList (CgsResourceIdList.h -- the type's
// primary consumer). The X360 u32-slot pokes (*(u32*)res +/- delta) become the natural
// pointer rebase on the NAMED member, mirroring ZoneListResourceType's corrected
// pattern; the delta is the full-width GetLoadBase64 (the x64 heap base does not fit
// the console's 32-bit delta).

namespace CgsResource
{
    namespace
    {
        // File-local view of the widened serialised header. Field names mirror the
        // committed CgsResource::ResourceIdList (CgsResourceIdList.h: mpaIds @+0,
        // muNumIds @+8 after widening); modelled locally because that class keeps
        // its members private.
        struct SerialisedIdList
        {
            ID* mpaIds;   // +0x00  file-relative -> loaded id-array pointer (u64 slot)
            u32 muNumIds; // +0x08  number of ids (the u64 id array follows @+0x10)
        };
    }

    // FixDown: file-relative-ise the id-array pointer (subtract the delta), only if
    // non-zero. @ 0x828F75D8
    void IdListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        SerialisedIdList* lpList = static_cast<SerialisedIdList*>(lpResource);
        if (lpList->mpaIds)
            lpList->mpaIds = reinterpret_cast<ID*>(
                reinterpret_cast<uintptr_t>(lpList->mpaIds) - CgsResource::GetLoadBase64(lrResource));
    }

    // FixUp: rebase the id-array pointer to the loaded address (add the delta), only
    // if non-zero. @ 0x828F75F8
    void IdListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        SerialisedIdList* lpList = static_cast<SerialisedIdList*>(lpResource);
        if (lpList->mpaIds)
            lpList->mpaIds = reinterpret_cast<ID*>(
                reinterpret_cast<uintptr_t>(lpList->mpaIds) + CgsResource::GetLoadBase64(lrResource));
    }
}
