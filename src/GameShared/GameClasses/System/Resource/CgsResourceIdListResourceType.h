#ifndef CGS_RESOURCE_ID_LIST_RESOURCE_TYPE_H
#define CGS_RESOURCE_ID_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised ResourceIdList, deriving from
// CgsResource::Type. The serialised payload is a ResourceIdList header whose
// leading word (mpaIds) is a file-relative pointer; FixUp/FixDown rebase that
// single pointer by the load-base delta. GetTypeID/GetSerialisedResourceDescriptor
// are sibling virtuals owned by other recon passes; FixDown/FixUp are owned here.
// Base/signatures recovered from the DecFIGS DWARF (CgsResourceIdListResourceType.h).
class IdListResourceType : public CgsResource::Type
{
public:
    uint32_t           GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void               FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
