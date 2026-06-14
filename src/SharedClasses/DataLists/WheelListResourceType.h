#ifndef WHEEL_LIST_RESOURCE_TYPE_H
#define WHEEL_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnResource
{
class WheelListResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
