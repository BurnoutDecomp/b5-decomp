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
    uint32_t GetTypeID() const ;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const ;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const ;
};
}

#endif
