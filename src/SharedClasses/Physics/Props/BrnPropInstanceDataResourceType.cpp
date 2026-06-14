#include "SharedClasses/Physics/Props/BrnPropInstanceDataResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
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

    uint32_t PropInstanceDataResourceType::GetTypeID() const
    {
        return KU_PROP_INSTANCE_DATA_RESOURCE_TYPE_ID;
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
