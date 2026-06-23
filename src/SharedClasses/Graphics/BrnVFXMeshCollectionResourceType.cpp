#include "SharedClasses/Graphics/BrnVFXMeshCollectionResourceType.h"
#include "rw/rwcore_structs.h"                            // rw::Resource complete for FixUp
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "pc/gcm/renderengine/VertexBuffer.h"             // renderengine::VertexBuffer::Xbox2CheckPhysicalMemoryFlags
#include "pc/gcm/renderengine/IndexBuffer.h"              // renderengine::IndexBuffer::Xbox2CheckPhysicalMemoryFlags
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::BrnVFXMeshCollectionResourceType::GetTypeID  @ 0x82675908
//   BrnParticle::BrnVFXMeshCollectionResourceType::FixUp      @ 0x82678490
//   BrnParticle::BrnVFXMeshCollectionResourceType::FixDown    @ 0x82675918
//   BrnParticle::BrnVFXMeshCollectionResourceType::Serialise  @ 0x826758C8
//
// A serialised BrnVFXMeshCollection is a fixed dword header:
//   [ 0]      muVersion (must equal E_VERSION_CURRENT = 2)
//   [ 1..32]  mafRadius[32]            (the 32-entry debris-radius table)
//   [33] 0x84 mpMeshHelper             (file-relative, rebased to load base)
//   [34] 0x88 muNumIndices
//   [35] 0x8C muNumVertices
//   [36] 0x90 mMaterial.mpTextureName  (file-relative, rebased to load base)
// The collection is accessed by raw dword offset (the serialised header layout, not
// a host struct) -- the documented external-serialised-data exception to the
// by-name rule (matches the VFXPropsResourceType precedent).
//
// The embedded MeshHelper is a small dword header:
//   [0] GetNumIndexBuffers()  (must be 1)
//   [1] GetNumVertexBuffers() (must be 1)
//   [2] 0x8 m_buffers[0] -> IndexBuffer  (file-relative, rebased to load base)
//   [3] 0xC m_buffers[1] -> VertexBuffer (file-relative, rebased to load base)

namespace BrnParticle
{
    static const uint32_t KU_VFX_MESH_COLLECTION_RESOURCE_TYPE_ID = 65561;  // 0x10019
    static const uint32_t KU_VFX_MESH_COLLECTION_VERSION_CURRENT  = 2;      // E_VERSION_CURRENT

    enum EVFXMeshCollectionDword
    {
        E_MESHCOLLECTION_VERSION       = 0,
        E_MESHCOLLECTION_MESHHELPER    = 33,   // 0x84
        E_MESHCOLLECTION_TEXTURE_NAME  = 36    // 0x90
    };

    enum EMeshHelperDword
    {
        E_MESHHELPER_NUM_INDEX_BUFFERS  = 0,
        E_MESHHELPER_NUM_VERTEX_BUFFERS = 1,
        E_MESHHELPER_INDEX_BUFFER       = 2,   // 0x8
        E_MESHHELPER_VERTEX_BUFFER      = 3    // 0xC
    };

    uint32_t BrnVFXMeshCollectionResourceType::GetTypeID() const
    {
        return KU_VFX_MESH_COLLECTION_RESOURCE_TYPE_ID;
    }

    void BrnVFXMeshCollectionResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        // The rw::Resource arg supplies the rebase bases. On the X360 (32-bit) its
        // first two base-resource words are *a3 / a3[2]:
        //   m_baseResources[0] -> the file-relative pointer delta (applied to every
        //                         stored pointer in the header / mesh helper),
        //   m_baseResources[1] -> the buffer-address delta (added to each GPU buffer's
        //                         base-address dword).
        // All rebase arithmetic is done in u32 space (matching the X360 32-bit address
        // model and the VFXProps precedent), the pointer being formed only to
        // dereference a sub-field.
        u32  luDelta        = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));
        u32  luAddressDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[1]));
        u32* lpHeader       = reinterpret_cast<u32*>(lpResource);

        CGS_ASSERT(lpHeader[E_MESHCOLLECTION_VERSION] == KU_VFX_MESH_COLLECTION_VERSION_CURRENT,
                   "lpBrnVFXMeshCollection->muVersion == BrnVFXMeshCollection::E_VERSION_CURRENT");

        // Rebase the MeshHelper pointer and the material texture-name pointer.
        lpHeader[E_MESHCOLLECTION_MESHHELPER]   += luDelta;
        lpHeader[E_MESHCOLLECTION_TEXTURE_NAME] += luDelta;

        u32* lpMeshHelper = reinterpret_cast<u32*>(static_cast<uintptr_t>(lpHeader[E_MESHCOLLECTION_MESHHELPER]));

        CGS_ASSERT(lpMeshHelper[E_MESHHELPER_NUM_INDEX_BUFFERS] == 1u &&
                   lpMeshHelper[E_MESHHELPER_NUM_VERTEX_BUFFERS] == 1u,
                   "( lpMeshHelper->GetNumIndexBuffers() == 1 ) && ( lpMeshHelper->GetIndexBuffer( 0 ) == "
                   "reinterpret_cast<const renderengine::IndexBuffer*>( lpMeshHelper->m_buffers[0] ) ) && "
                   "( lpMeshHelper->GetNumVertexBuffers() == 1 ) && ( lpMeshHelper->GetVertexBuffer( 0 ) == "
                   "reinterpret_cast<const renderengine::VertexBuffer*>( lpMeshHelper->m_buffers[1] ) )");

        // Vertex buffer first (the X360 order). Rebase the buffer pointer, then patch
        // its base-address dword at +0x18: preserve the low 2 flag bits, add the
        // resource arg's third word to the address, then re-apply the bits.
        lpMeshHelper[E_MESHHELPER_VERTEX_BUFFER] += luDelta;
        u32* lpVertexBuffer = reinterpret_cast<u32*>(static_cast<uintptr_t>(lpMeshHelper[E_MESHHELPER_VERTEX_BUFFER]));

        u32 luVbBaseAddr = lpVertexBuffer[6];                                    // 0x18(r3)
        lpVertexBuffer[6] = (luVbBaseAddr & 3u) | (((luVbBaseAddr & 0xFFFFFFFCu) + luAddressDelta) & 0xFFFFFFFCu);
        renderengine::VertexBuffer::Xbox2CheckPhysicalMemoryFlags(lpVertexBuffer);

        // Index buffer. Rebase the buffer pointer, then add the address delta to its
        // base-address dword at +0x18 (no flag-bit preservation here).
        lpMeshHelper[E_MESHHELPER_INDEX_BUFFER] += luDelta;
        u32* lpIndexBuffer = reinterpret_cast<u32*>(static_cast<uintptr_t>(lpMeshHelper[E_MESHHELPER_INDEX_BUFFER]));

        lpIndexBuffer[6] += luAddressDelta;                                      // 0x18(r3) += a3[2]
        renderengine::IndexBuffer::Xbox2CheckPhysicalMemoryFlags(lpIndexBuffer);
    }

    // Both FixDown and Serialise are non-tool-build code paths: the X360 fires a
    // single assert and (for Serialise) returns null.
    void BrnVFXMeshCollectionResourceType::FixDown(void* /*lpResource*/, const rw::Resource& /*lrResource*/) const
    {
        CGS_ASSERT(false, "This code is not expected at runtime.");
    }

    void* BrnVFXMeshCollectionResourceType::Serialise(const void* /*lpResource*/, const rw::Resource& /*lrDest*/) const
    {
        CGS_ASSERT(false, "This code is not expected at runtime.");
        return nullptr;
    }
}
