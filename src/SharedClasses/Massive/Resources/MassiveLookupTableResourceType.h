#ifndef MASSIVE_LOOKUP_TABLE_RESOURCE_TYPE_H
#define MASSIVE_LOOKUP_TABLE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised BrnMassive::MassiveLookupTable blob.
// Derives from the real CgsResource::Type; GetTypeID/FixDown/FixUp are virtual
// overrides. Serialise is the type's own virtual (not on the base). Layout/base
// recovered from the DecFIGS DWARF (MassiveLookupTableResourceType.h).
class MassiveLookupTableResourceType : public Type
{
public:
    uint32_t           GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;

    // The X360 build's Serialise takes (resource, destination) and computes the
    // blob size itself; the DecFIGS PS3 build passes a third ResourceDescriptor
    // size argument (version drift) — we follow the X360 spine.
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
