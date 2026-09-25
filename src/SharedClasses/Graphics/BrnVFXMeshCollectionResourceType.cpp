#include "SharedClasses/Graphics/BrnVFXMeshCollectionResourceType.h"
#include "rw/rwcore_structs.h"                            // rw::Resource / BaseResourceDescriptors complete
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // the stale-asset line (IsUnconvertedPC)
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
    // E_VERSION_CURRENT as a big-endian (unconverted) collection's version word reads on this host.
    static const uint32_t KU_VFX_MESH_COLLECTION_VERSION_UNCONVERTED = 0x02000000u;

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

    // GetSerialisedResourceDescriptor @ 0x8267C3F8. This handler does not compute a
    // build-time allocation descriptor at runtime: the X360 body fires the single
    // "not expected at runtime" assert, then returns a five-entry descriptor. Slots 1..4
    // are {size=0, alignment=1} (no main allocation; the mesh/buffers come in as imports);
    // slot0 is then zeroed wholesale by a trailing `std r9,0(r31)` (r9 = 64-bit 0), so
    // slot0 ends {size=0, alignment=0}.
    CgsResource::ResourceDescriptor
    BrnVFXMeshCollectionResourceType::GetSerialisedResourceDescriptor(const void* /*lpResource*/) const
    {
        CGS_ASSERT(false, "This code is not expected at runtime.");

        CgsResource::ResourceDescriptor lDescriptor;
        for (uint32_t luIndex = 0; luIndex < 5; ++luIndex)
        {
            lDescriptor.m_baseResourceDescriptors[luIndex].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1u;
        }
        // Trailing `std r9,0(r31)` (r9 = 64-bit 0) re-writes slot0's full qword, zeroing
        // BOTH m_size and m_alignment -- so slot0 ends {0,0}, not {0,1}.
        lDescriptor.m_baseResourceDescriptors[0].m_size      = 0u;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0u;
        return lDescriptor;
    }

    // =====================================================================================
    // FLAG PC platform leaf -- STALE-ASSET TOLERANCE, not game logic, NOT IN THE X360 BINARY.
    //
    // Players convert their own game data with the project's tools. A PARTICLES.BUNDLE converted
    // before the debris-mesh port (tools/assets/bundles/particles_transcode.py, FX-CRASHVFX C3)
    // carries its three collections BIG-ENDIAN, and the console's walk below would assert on the
    // version and then follow a byte-reversed MeshHelper offset into nothing. Such a collection is
    // recognised by its version word -- E_VERSION_CURRENT byte-reversed -- and REFUSED: FixUp leaves
    // it untouched, ParticleModule::LoadFXBundle stage 6 does not bind it, and the debris meshes stay
    // undrawn exactly as they were before the port. One always-on line says so, once per run, with
    // the command that re-converts the bundle.
    // =====================================================================================
    bool BrnVFXMeshCollectionResourceType::IsUnconvertedPC(const void* lpResource)
    {
        return lpResource != 0
            && reinterpret_cast<const u32*>(lpResource)[E_MESHCOLLECTION_VERSION] == KU_VFX_MESH_COLLECTION_VERSION_UNCONVERTED;
    }

    static void ReportUnconvertedPC()
    {
        static bool sbReported = false;
        if (sbReported)
            return;
        sbReported = true;
        CgsDev::Log::WriteToLog(
            "[particles] PARTICLES.BUNDLE holds an UNCONVERTED (big-endian) debris mesh collection: it was "
            "converted before the debris-mesh port, so the debris meshes stay unbound and undrawn. Re-convert "
            "it with: py tools/assets/build_game_data.py \"<your X360 game folder>\" --only PARTICLES.BUNDLE\n");
    }

    void BrnVFXMeshCollectionResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        u32* lpHeader = reinterpret_cast<u32*>(lpResource);

        // FLAG PC platform leaf (see IsUnconvertedPC): checked before anything is dereferenced.
        if (IsUnconvertedPC(lpHeader))
        {
            ReportUnconvertedPC();
            return;
        }

        // The rw::Resource arg supplies the rebase bases. The X360 rw::Resource lanes are 4-byte
        // words, and FixUp reads two of them: `lwz r29, 0(r30)` (0x826784A8) and `lwz r9, 8(r30)` /
        // `lwz r10, 8(r30)` (0x82678544 / 0x82678568) -- lanes 0 and 2:
        //   m_baseResources[0] -> the main memory the header was loaded into: the delta added to
        //                         every stored offset in the header and the mesh helper;
        //   m_baseResources[2] -> the graphics memory the body (the index and vertex data) was
        //                         loaded into: added to each GPU buffer's base-address dword.
        // Pool::FixUpEntry's ConvertToRWResource puts the loaded graphics block in lane 2
        // (CgsSmallResource.cpp: rw[2] = small[1]) and leaves lane 1 empty -- RwRasterResourceType and
        // RwRenderableResourceType read lane 2 for the same reason. (This used to read lane 1: every
        // buffer kept its body OFFSET for an address.)
        // All rebase arithmetic is done in u32 space (matching the X360 32-bit address model and the
        // VFXProps precedent), the pointer being formed only to dereference a sub-field.
        u32 luDelta        = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));
        u32 luAddressDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[2]));

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
        // graphics lane (rw lane 2, the X360's third word) to the address, then re-apply the bits.
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
