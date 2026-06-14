#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSchemaResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::AttribSysSchemaResourceType::FixUp     @ 0x828E1598
//   CgsResource::AttribSysSchemaResourceType::GetTypeID @ 0x828D7970
//
// FixUp rebases two pointer fields (offsets 0 and 8) by the relocation delta (the
// rw::Resource's load base), leaving null fields untouched.

namespace CgsResource
{
    static const uint32_t KU_ATTRIB_SYS_SCHEMA_RESOURCE_TYPE_ID = 27;

    uint32_t AttribSysSchemaResourceType::GetTypeID() const
    {
        return KU_ATTRIB_SYS_SCHEMA_RESOURCE_TYPE_ID;
    }

    void AttribSysSchemaResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        int       liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));
        uintptr_t lBase   = reinterpret_cast<uintptr_t>(lpResource);

        uintptr_t& lrField0 = *reinterpret_cast<uintptr_t*>(lBase + 0);
        if (lrField0)
            lrField0 += liDelta;

        uintptr_t& lrField8 = *reinterpret_cast<uintptr_t*>(lBase + 8);
        if (lrField8)
            lrField8 += liDelta;
    }
}
