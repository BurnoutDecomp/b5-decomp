#ifndef BRN_PROP_INSTANCE_DATA_RESOURCE_TYPE_H
#define BRN_PROP_INSTANCE_DATA_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnPhysics
{
namespace Props
{
class PropInstanceDataResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const ;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const ;
};
}
}

#endif
