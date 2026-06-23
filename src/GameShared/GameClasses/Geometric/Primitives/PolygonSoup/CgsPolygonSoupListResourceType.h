#ifndef CGS_POLYGON_SOUP_LIST_RESOURCE_TYPE_H
#define CGS_POLYGON_SOUP_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised CgsGeometric::PolygonSoupList. Derives from
// CgsResource::Type; GetTypeID/FixUp are virtual overrides. From DecFIGS DWARF.
class PolygonSoupListResourceType : public Type
{
public:
    uint32_t           GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
