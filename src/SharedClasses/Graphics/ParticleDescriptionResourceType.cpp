#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::ParticleDescriptionCollectionResourceType::FixUp            @ 0x8267DF40
//   BrnParticle::ParticleDescriptionCollectionResourceType::GetImportPointer @ 0x826757E8
//   BrnParticle::ParticleDescriptionCollectionResourceType::GetTypeID        @ 0x826757C8
//   BrnParticle::ParticleDescriptionCollectionResourceType::Serialise        @ 0x826782B8
//   BrnParticle::ParticleDescriptionResourceType::DeSerialise                @ 0x82675868
//   BrnParticle::ParticleDescriptionResourceType::GetTypeID                  @ 0x82675858
//   BrnParticle::ParticleDescriptionResourceType::Serialise                  @ 0x8267C220
//
// Resource-type handlers for particle descriptions. The collection is a serialised
// table of per-description pointers (count at +4, table at +0); single descriptions
// wrap a LionFX binary payload. Serialised tables are accessed by offset.

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
    class ParticleDescriptionCollectionResourceType
    {
    public:
        void* FixUp(void* pResource);
        void  GetImportPointer(const void* pResource, int liIndex, u32* pOutOffset, u32* pOutValue);
        int   GetTypeID() { return 65544; }
        void* Serialise(void* pResource, void** ppDest);
    };

    void* ParticleDescriptionCollectionResourceType::FixUp(void* pResource)
    {
        // The leading table pointer is stored file-relative; rebase it to the
        // loaded address.
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);
        *reinterpret_cast<u32*>(lRes) += static_cast<u32>(lRes);
        return pResource;
    }

    void ParticleDescriptionCollectionResourceType::GetImportPointer(const void* pResource, int liIndex,
                                                                     u32* pOutOffset, u32* pOutValue)
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);
        u32 luTable = *reinterpret_cast<const u32*>(lRes);
        *pOutValue = *reinterpret_cast<const u32*>(luTable + 4 * liIndex);
        *pOutOffset = 4 * (liIndex + 2);
    }

    void* ParticleDescriptionCollectionResourceType::Serialise(void* pResource, void** ppDest)
    {
        u32* lpDst = reinterpret_cast<u32*>(*ppDest);
        u32* lpSrc = reinterpret_cast<u32*>(pResource);

        u32 luCount = lpSrc[1];
        lpDst[1] = luCount;
        lpDst[0] = reinterpret_cast<u32>(lpDst + 2);
        if (luCount)
        {
            u32* lpEntries = reinterpret_cast<u32*>(lpDst[0]);
            u32* lpSrcEntries = reinterpret_cast<u32*>(lpSrc[0]);
            for (u32 lu = 0; lu < luCount; ++lu)
                lpEntries[lu] = lpSrcEntries[lu];
        }
        return *ppDest;
    }

    class ParticleDescriptionResourceType
    {
    public:
        int   DeSerialise(void* pContent, void* pResource);
        int   GetTypeID() { return 65565; }
        void* Serialise(void* pResource, void** ppDest);
    };

    int ParticleDescriptionResourceType::DeSerialise(void* pContent, void* pResource)
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);
        *reinterpret_cast<u32*>(lRes + 4) =
            reinterpret_cast<u32>(cLionFX::BinLoad(reinterpret_cast<void*>(*reinterpret_cast<u32*>(lRes + 4))));
        return CgsSound::Playback::Content::DoOnPostLoad(pContent);
    }

    void* ParticleDescriptionResourceType::Serialise(void* pResource, void** ppDest)
    {
        u32* lpDst = reinterpret_cast<u32*>(*ppDest);
        u32* lpSrc = reinterpret_cast<u32*>(pResource);

        u32 luPayload = reinterpret_cast<u32>(lpDst) + 16;
        lpDst[0] = lpSrc[0];
        // Two-entry LionFX save stream descriptor: vtable + payload pointer.
        extern void* off_820A7F30;
        u32 laStream[4] = {};
        laStream[0] = reinterpret_cast<u32>(&off_820A7F30);
        laStream[1] = luPayload;
        lpDst[1] = luPayload;
        cLionFX::BinSave(reinterpret_cast<void*>(lpSrc[1]), 1, laStream);
        return *ppDest;
    }
}

void* off_820A7F30 = nullptr;
