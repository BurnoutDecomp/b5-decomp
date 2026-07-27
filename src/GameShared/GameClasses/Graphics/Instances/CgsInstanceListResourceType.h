#ifndef CGS_INSTANCE_LIST_RESOURCE_TYPE_H
#define CGS_INSTANCE_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsGraphics
{
// Resource-type handler for the instance-list resource (type id 35 / 0x23).
// The CgsResource::Type base virtuals are all NON-PURE, so overriding only the
// recovered ones (GetTypeID/GetSerialisedResourceDescriptor/FixDown/PostFixUp)
// keeps the class concrete. FixUp is a separate TU (CgsInstanceListResourceType.cpp:203
// per the DWARF) and is intentionally NOT defined here — it falls through to the base.
// CalculateSizeOfResource (DWARF-declared) is likewise deferred — not part of a
// recovered slice.
struct InstanceListResourceType : public CgsResource::Type
{
    uint32_t                   GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     PostFixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
