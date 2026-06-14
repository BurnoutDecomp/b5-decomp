#include "SharedClasses/Graphics/ParticleDescriptionResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::ParticleDescriptionCollectionResourceType::FixUp            @ 0x8267DF40
//   BrnParticle::ParticleDescriptionCollectionResourceType::GetImportPointer @ 0x826757E8
//   BrnParticle::ParticleDescriptionCollectionResourceType::GetTypeID        @ 0x826757C8
//   BrnParticle::ParticleDescriptionCollectionResourceType::Serialise        @ 0x826782B8
//   BrnParticle::ParticleDescriptionResourceType::DeSerialise                @ 0x82675868
//   BrnParticle::ParticleDescriptionResourceType::GetTypeID                  @ 0x82675858
//   BrnParticle::ParticleDescriptionResourceType::Serialise                  @ 0x8267C220
//
// The collection is a serialised table of per-description pointers (count at +4,
// table at +0); single descriptions wrap a LionFX binary payload. Serialise writes
// to the destination resource's buffer (the leading word of the rw::Resource).
// Serialised tables are accessed by offset.

namespace cLionFX
{
    void* BinLoad(void* pData);
    int   BinSave(void* pData, int liFlag, void* pStream);
    void* BinLoad(void*) { __debugbreak(); return nullptr; }
    int   BinSave(void*, int, void*) { __debugbreak(); return 0; }
}

namespace CgsSound { namespace Playback { namespace Content
{
    int DoOnPostLoad(void* pContent);
    int DoOnPostLoad(void*) { __debugbreak(); return 0; }
}}}

namespace BrnParticle
{
    static const uint32_t KU_PARTICLE_DESCRIPTION_COLLECTION_RESOURCE_TYPE_ID = 65544;
    static const uint32_t KU_PARTICLE_DESCRIPTION_RESOURCE_TYPE_ID = 65565;

    uint32_t ParticleDescriptionCollectionResourceType::GetTypeID() const
    {
        return KU_PARTICLE_DESCRIPTION_COLLECTION_RESOURCE_TYPE_ID;
    }

    // The table pointer is stored file-relative; FixUp rebases it to the loaded
    // address (the resource's own base); the rw::Resource argument is unused.
    void ParticleDescriptionCollectionResourceType::FixUp(void* lpResource, const rw::Resource&) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        *reinterpret_cast<u32*>(lRes) += static_cast<u32>(lRes);
    }

    void ParticleDescriptionCollectionResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                                                     uint32_t* lpuOffset, const void** lppValue) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        u32 luTable = *reinterpret_cast<const u32*>(lRes);
        *lppValue = reinterpret_cast<const void*>(*reinterpret_cast<const u32*>(luTable + 4 * luIndex));
        *lpuOffset = 4 * (luIndex + 2);
    }

    void* ParticleDescriptionCollectionResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        u32*       lpDst = reinterpret_cast<u32*>(lrDest.m_baseResources[0]);
        const u32* lpSrc = reinterpret_cast<const u32*>(lpResource);

        u32 luCount = lpSrc[1];
        lpDst[1] = luCount;
        lpDst[0] = reinterpret_cast<u32>(lpDst + 2);
        if (luCount)
        {
            u32*       lpEntries    = reinterpret_cast<u32*>(lpDst[0]);
            const u32* lpSrcEntries = reinterpret_cast<const u32*>(lpSrc[0]);
            for (u32 lu = 0; lu < luCount; ++lu)
                lpEntries[lu] = lpSrcEntries[lu];
        }
        return lpDst;
    }

    uint32_t ParticleDescriptionResourceType::GetTypeID() const
    {
        return KU_PARTICLE_DESCRIPTION_RESOURCE_TYPE_ID;
    }

    // The X360 passes `this` (the resource type) to Content::DoOnPostLoad after the
    // LionFX payload is loaded in place.
    bool ParticleDescriptionResourceType::DeSerialise(void* lpResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        *reinterpret_cast<u32*>(lRes + 4) =
            reinterpret_cast<u32>(cLionFX::BinLoad(reinterpret_cast<void*>(*reinterpret_cast<u32*>(lRes + 4))));
        return CgsSound::Playback::Content::DoOnPostLoad(
            const_cast<ParticleDescriptionResourceType*>(this)) != 0;
    }

    void* ParticleDescriptionResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        u32*       lpDst = reinterpret_cast<u32*>(lrDest.m_baseResources[0]);
        const u32* lpSrc = reinterpret_cast<const u32*>(lpResource);

        u32 luPayload = reinterpret_cast<u32>(lpDst) + 16;
        lpDst[0] = lpSrc[0];
        // Two-entry LionFX save stream descriptor: vtable + payload pointer.
        extern void* off_820A7F30;
        u32 laStream[4] = {};
        laStream[0] = reinterpret_cast<u32>(&off_820A7F30);
        laStream[1] = luPayload;
        lpDst[1] = luPayload;
        cLionFX::BinSave(reinterpret_cast<void*>(lpSrc[1]), 1, laStream);
        return lpDst;
    }
}

void* off_820A7F30 = nullptr;
