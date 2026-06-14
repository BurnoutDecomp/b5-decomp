#ifndef ZONE_LIST_RESOURCE_TYPE_H
#define ZONE_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised CgsSceneManager::ZoneList blob. Derives
// from CgsResource::Type; GetTypeID/FixDown/FixUp/GetSerialisedResourceDescriptor
// are virtual overrides. Serialise is the type's own virtual. Base/signatures
// recovered from the DecFIGS DWARF (ZoneListResourceType.h).
class ZoneListResourceType : public Type
{
public:
    uint32_t           GetTypeID() const override;
    void               FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    virtual void*      Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
