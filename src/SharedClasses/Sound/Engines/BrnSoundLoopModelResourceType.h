#ifndef BRN_SOUND_LOOP_MODEL_RESOURCE_TYPE_H
#define BRN_SOUND_LOOP_MODEL_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{
struct LoopModelData
{
    int FixUp(int liDelta);
    int FixDown(int liDelta);
};

class LoopModelResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}
}
}

#endif
