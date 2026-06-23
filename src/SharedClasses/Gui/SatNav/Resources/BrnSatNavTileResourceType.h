#ifndef BRN_SAT_NAV_TILE_RESOURCE_TYPE_H
#define BRN_SAT_NAV_TILE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Both derive from CgsResource::Type. The directory relocates a single pointer; the
// tile relocates an import table and exposes its imports. From DecFIGS DWARF.
class SatNavTileDirectoryResourceType : public Type
{
public:
    uint32_t           GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void               FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};

class SatNavTileResourceType : public Type
{
public:
    uint32_t           GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void               FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    uint32_t           GetImportCount(const void* lpResource) const override;
    void               GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
};
}

#endif
