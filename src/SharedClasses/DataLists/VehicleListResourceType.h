#ifndef VEHICLE_LIST_RESOURCE_TYPE_H
#define VEHICLE_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnResource
{
class VehicleListResourceType : public CgsResource::Type
{
public:
    uint32_t                    GetTypeID() const ;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
    void                        FixDown(void* lpResource, const rw::Resource& lrResource) const ;
    void                        FixUp(void* lpResource, const rw::Resource& lrResource) const ;
};
}

#endif
