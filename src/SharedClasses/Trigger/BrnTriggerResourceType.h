#ifndef BRN_TRIGGER_RESOURCE_TYPE_H
#define BRN_TRIGGER_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
// ⚠️ This header used to carry its OWN two-method `struct BrnTrigger::TriggerData` stub --
// an ODR fork of the real one in BrnTriggerData.h. It silently kept the pre-widening
// `int FixUp(int)` signature alive, so the forwarders below linked against a function that
// no longer exists once the delta became uintptr_t. Include the real owning header instead.
#include "SharedClasses/Trigger/BrnTriggerData.h"   // BrnTrigger::TriggerData (the one true layout)

namespace BrnTrigger
{
class TriggerResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
