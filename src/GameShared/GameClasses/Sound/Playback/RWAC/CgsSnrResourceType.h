#ifndef CGS_SNR_RESOURCE_TYPE_H
#define CGS_SNR_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
class SnrResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
