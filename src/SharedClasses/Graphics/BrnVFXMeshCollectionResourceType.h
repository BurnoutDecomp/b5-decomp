#ifndef BRN_VFX_MESH_COLLECTION_RESOURCE_TYPE_H
#define BRN_VFX_MESH_COLLECTION_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnParticle
{
// Resource-type handler for the VFX mesh collection (registry type id 0x10019 =
// 65561, confirmed in the X360 asm at GetTypeID @0x82675908: lis r3,1 / ori
// r3,r3,0x19). Derives from CgsResource::Type. GetTypeID/FixUp/FixDown/Serialise
// are virtual overrides. A serialised BrnVFXMeshCollection carries a version word,
// a 32-entry debris-radius table, a MeshHelper pointer (one index buffer + one
// vertex buffer) and a single material; FixUp rebases the MeshHelper and material
// pointers and the buffer descriptors to the loaded address. Serialise/FixDown are
// non-tool-build stubs that fire a single assert.
//
// Base/signatures recovered from the DecFIGS DWARF
// (BrnVFXMeshCollectionResourceType.h) and the X360 pseudocode/asm.
class BrnVFXMeshCollectionResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
