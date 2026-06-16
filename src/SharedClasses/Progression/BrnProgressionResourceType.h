#ifndef BRN_PROGRESSION_RESOURCE_TYPE_H
#define BRN_PROGRESSION_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "SharedClasses/Progression/BrnProgressionData.h"   // BrnProgression::ProgressionData (full owning home)

namespace BrnProgression
{
// ProgressionData's single definition now lives in BrnProgressionData.h (included above);
// the former 2-method stub here was consolidated into that owning header. FixUp/FixDown(int)
// keep the same caller contract (see BrnProgressionResourceType.cpp).

class ProgressionResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
