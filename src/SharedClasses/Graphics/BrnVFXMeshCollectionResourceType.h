#ifndef BRN_VFX_MESH_COLLECTION_RESOURCE_TYPE_H
#define BRN_VFX_MESH_COLLECTION_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include <cstddef>

namespace renderengine { class MeshHelper; }

namespace BrnParticle
{
    // DecFIGS BrnVFXMeshCollectionResourceType.h:46 and ARTIST 82678490.
    struct BrnVFXMeshCollection
    {
        struct Material
        {
            char* mpTextureName;
        };

        const Material& GetMaterial() const { return mMaterial; }
        const renderengine::MeshHelper* GetMeshHelper() const { return mpMeshHelper; }
        u32 GetNumIndices() const { return muNumIndices; }

        u32 muVersion;
        f32 mafRadius[32];
        renderengine::MeshHelper* mpMeshHelper;
        u32 muNumIndices;
        u32 muNumVertices;
        Material mMaterial;
    };
    CGS_STATIC_ASSERT((offsetof(BrnVFXMeshCollection, mpMeshHelper) == 0x84), "ARTIST debris mesh");
    CGS_STATIC_ASSERT((offsetof(BrnVFXMeshCollection, muNumIndices) == 0x88), "ARTIST debris index count");
    CGS_STATIC_ASSERT((offsetof(BrnVFXMeshCollection, mMaterial) == 0x90), "ARTIST debris material");
    CGS_STATIC_ASSERT((sizeof(BrnVFXMeshCollection) == 0x94), "ARTIST VFX mesh collection");

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
    uint32_t GetTypeID() const ;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const ;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const ;
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest) const;

};
}

#endif
