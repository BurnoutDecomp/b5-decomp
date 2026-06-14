#ifndef BRN_PROGRESSION_RESOURCE_TYPE_H
#define BRN_PROGRESSION_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnProgression
{
struct ProgressionData
{
    int FixUp(int liDelta);
    int FixDown(int liDelta);
};

class ProgressionResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
