#ifndef BRN_PROP_PHYSICS_LIST_RESOURCE_TYPE_H
#define BRN_PROP_PHYSICS_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnPhysics
{
namespace Props
{
// Resource-type handler for a serialised PropPhysicsDataHeader, deriving from
// CgsResource::Type. FixUp/FixDown forward straight to the header's own
// FixUp/FixDown (the resource payload IS a PropPhysicsDataHeader).
// GetTypeID/GetSerialisedResourceDescriptor are sibling virtuals owned by other
// recon passes. Base/signatures recovered from the DecFIGS DWARF
// (BrnPropPhysicsListResourceType.h) and the X360 pseudocode.
class PropPhysicsResourceType : public CgsResource::Type
{
public:
    uint32_t                   GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                       FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                       FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}
}

#endif
