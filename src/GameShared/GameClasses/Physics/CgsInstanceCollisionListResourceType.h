#ifndef CGS_INSTANCE_COLLISION_LIST_RESOURCE_TYPE_H
#define CGS_INSTANCE_COLLISION_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsPhysics
{
// Resource-type handler for a serialised instance collision list. Derives from
// CgsResource::Type; GetTypeID/FixDown/FixUp are virtual overrides. From DecFIGS DWARF.
class InstanceCollisionListResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
