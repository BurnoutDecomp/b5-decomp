#ifndef VEHICLE_LIST_RESOURCE_TYPE_H
#define VEHICLE_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnResource
{
class VehicleListResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
