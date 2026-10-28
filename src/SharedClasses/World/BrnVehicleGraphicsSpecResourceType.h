#ifndef BRN_VEHICLE_GRAPHICS_SPEC_RESOURCE_TYPE_H
#define BRN_VEHICLE_GRAPHICS_SPEC_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnVehicle
{
// Resource-type handler for the vehicle graphics-spec resource (0x10006).
// Sibling of BrnWheel::GraphicsSpecResourceType. GetSerialisedResourceDescriptor
// is inherited from the non-pure CgsResource::Type base (deferred — not part of
// this TU's recovered slice).
class GraphicsSpecResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const ;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const ;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const ;
};
}

#endif
