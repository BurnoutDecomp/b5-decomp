#ifndef BRN_TRAFFIC_DATA_RESOURCE_TYPE_H
#define BRN_TRAFFIC_DATA_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnTraffic
{
struct TrafficData
{
    int FixDown();
};

class TrafficDataResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
