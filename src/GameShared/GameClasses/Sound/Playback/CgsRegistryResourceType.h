#pragma once

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised registry. Derives from CgsResource::Type;
// GetTypeID/GetSerialisedResourceDescriptor are virtual overrides. From DecFIGS DWARF.
class RegistryResourceType : public Type
{
public:
    uint32_t           GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
};
}
