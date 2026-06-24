#ifndef BRN_WHEEL_GRAPHICS_SPEC_RESOURCE_TYPE_H
#define BRN_WHEEL_GRAPHICS_SPEC_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnWheel
{
class GraphicsSpecResourceType : public CgsResource::Type
{
public:
    uint32_t                   GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                       FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
