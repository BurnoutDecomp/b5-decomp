#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::PolygonSoupListResourceType::FixUp     @ 0x82845EB0
//   CgsResource::PolygonSoupListResourceType::GetTypeID @ 0x82839FA0
//
// FixUp forwards to CgsGeometric::PolygonSoupList::FixUp (own TU), passing the list
// (the resource) and the relocation delta (the rw::Resource's load base).

namespace CgsGeometric
{
    struct PolygonSoupList
    {
        PolygonSoupList* FixUp(int liDelta);
    };
}

namespace CgsResource
{
    static const uint32_t KU_POLYGON_SOUP_LIST_RESOURCE_TYPE_ID = 67;

    uint32_t PolygonSoupListResourceType::GetTypeID() const
    {
        return KU_POLYGON_SOUP_LIST_RESOURCE_TYPE_ID;
    }

    void PolygonSoupListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGeometric::PolygonSoupList*>(lpResource)->FixUp(
            static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }
}
