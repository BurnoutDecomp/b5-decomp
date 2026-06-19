#ifndef CGS_INSTANCE_LIST_RESOURCE_TYPE_H
#define CGS_INSTANCE_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsGraphics
{
// Resource-type handler for the instance-list resource (type id 35 / 0x23).
// The CgsResource::Type base virtuals are all NON-PURE, so overriding only the
// three recovered for this TU (GetTypeID/FixDown/PostFixUp) keeps the class
// concrete. FixUp is a separate TU (CgsInstanceListResourceType.cpp:203 per the
// DWARF) and is intentionally NOT defined here — it falls through to the base.
// GetSerialisedResourceDescriptor / CalculateSizeOfResource (DWARF-declared) are
// likewise deferred — not part of this TU's recovered slice.
struct InstanceListResourceType : public CgsResource::Type
{
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     PostFixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
