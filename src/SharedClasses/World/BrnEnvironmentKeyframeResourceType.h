#ifndef BRN_ENVIRONMENT_KEYFRAME_RESOURCE_TYPE_H
#define BRN_ENVIRONMENT_KEYFRAME_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnWorld
{
namespace EnvironmentSettings
{
class KeyframeResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}
}

#endif
