#ifndef BRN_TRIGGER_RESOURCE_TYPE_H
#define BRN_TRIGGER_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnTrigger
{
struct TriggerData
{
    int FixUp(int liDelta);
    int FixDown(int liDelta);
};

class TriggerResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
