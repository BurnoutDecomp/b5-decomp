#ifndef BRN_WHEEL_GRAPHICS_SPEC_RESOURCE_TYPE_H
#define BRN_WHEEL_GRAPHICS_SPEC_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnWheel
{
class GraphicsSpecResourceType : public CgsResource::Type
{
public:
    uint32_t                   GetTypeID() const ;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
    void                       FixUp(void* lpResource, const rw::Resource& lrResource) const ;
};
}

#endif
