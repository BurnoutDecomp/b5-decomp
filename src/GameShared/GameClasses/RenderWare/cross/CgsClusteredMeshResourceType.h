#ifndef CGS_CLUSTERED_MESH_RESOURCE_TYPE_H
#define CGS_CLUSTERED_MESH_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Serialised collision ClusteredMesh handler. Derives from CgsResource::Type; the
// listed virtuals are overrides (GetImportPointer asserts — clustered meshes have no
// import pointers). GetSerialisedResourceDescriptor/FixDown/FixUp/GetImportPointer/
// GetTypeID are defined in CgsClusteredMeshResourceType.cpp; ReBase in
// CgsResourceType.cpp. Base/signatures recovered from the DecFIGS DWARF
// (CgsClusteredMeshResourceType.h).
class ClusteredMeshResourceType : public Type
{
public:
    uint32_t           GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void               FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void               GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
    void               ReBase(void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest, ResourceDescriptor& lrSize, s32 liMemType) const override;
};
}

#endif
