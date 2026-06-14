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
// The collection resource is a {entry-array pointer, count} header; FixUp rebases the entry
// pointer self-relative, GetImportPointer reports an entry, and Serialise packs the entry
// array after an 8-byte header. The single-description resource wraps a cLionFX binary blob:
// DeSerialise reloads it and runs the sound-content post-load; Serialise writes it back out
// through cLionFX::BinSave. Foreign helpers (cLionFX, sound Content) are in other TUs.

namespace cLionFX
{
    u32 BinLoad(u32 luData);
    int BinSave(u32 luData, int liMode, void* pDescriptor);
}

namespace CgsSound
{
namespace Playback
{
    struct Content
    {
        static int DoOnPostLoad(void* pContent);
    };
}
}

// Particle-description save callback table (off_820A7F30).
extern u8 off_820A7F30;

namespace BrnParticle
{
namespace
{
    struct ParticleDescriptionCollection
    {
        u32 muEntries;   // +0  relocatable pointer to the entry array
        u32 muCount;     // +4
    };

    struct ParticleDescription
    {
        u32 muField0;    // +0
        u32 muLionFX;    // +4  cLionFX binary blob pointer
    };
}

class ParticleDescriptionCollectionResourceType
{
public:
    void* FixUp(void* pResource)
    {
        ParticleDescriptionCollection* lpCollection = static_cast<ParticleDescriptionCollection*>(pResource);
        lpCollection->muEntries += static_cast<u32>(reinterpret_cast<uintptr_t>(pResource));
        return pResource;
    }

    void* GetImportPointer(void* pResource, int liIndex, u32* pOutOffset, u32* pOutValue)
    {
        const ParticleDescriptionCollection* lpCollection = static_cast<const ParticleDescriptionCollection*>(pResource);
        const u32* lpEntries = reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpCollection->muEntries));
        *pOutValue  = lpEntries[liIndex];
        *pOutOffset = 4 * (liIndex + 2);
        return pResource;
    }

    int   GetTypeID() { return KI_TYPE_ID; }
    void* Serialise(void* pResource, void* pSource, void** ppDestination);

private:
    static const int KI_TYPE_ID = 65544;
};

void* ParticleDescriptionCollectionResourceType::Serialise(void* /*pResource*/, void* pSource, void** ppDestination)
{
    const ParticleDescriptionCollection* lpSrc = static_cast<const ParticleDescriptionCollection*>(pSource);
    ParticleDescriptionCollection* lpDest = static_cast<ParticleDescriptionCollection*>(*ppDestination);

    const u32 luCount = lpSrc->muCount;
    u32* lpDestEntries = reinterpret_cast<u32*>(reinterpret_cast<u8*>(lpDest) + 8);
    lpDest->muCount   = luCount;
    lpDest->muEntries = static_cast<u32>(reinterpret_cast<uintptr_t>(lpDestEntries));

    const u32* lpSrcEntries = reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpSrc->muEntries));
    for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        lpDestEntries[luIndex] = lpSrcEntries[luIndex];

    return *ppDestination;
}

class ParticleDescriptionResourceType
{
public:
    int   DeSerialise(void* pResource);
    int   GetTypeID() { return KI_TYPE_ID; }
    void* Serialise(void* pResource, void* pSource, void** ppDestination);

private:
    static const int KI_TYPE_ID = 65565;
};

int ParticleDescriptionResourceType::DeSerialise(void* pResource)
{
    ParticleDescription* lpDesc = static_cast<ParticleDescription*>(pResource);
    lpDesc->muLionFX = cLionFX::BinLoad(lpDesc->muLionFX);
    return CgsSound::Playback::Content::DoOnPostLoad(this);
}

void* ParticleDescriptionResourceType::Serialise(void* /*pResource*/, void* pSource, void** ppDestination)
{
    const ParticleDescription* lpSrc = static_cast<const ParticleDescription*>(pSource);
    u32* lpDest = static_cast<u32*>(*ppDestination);
    u8*  lpBody = reinterpret_cast<u8*>(lpDest) + 16;

    lpDest[0] = lpSrc->muField0;
    lpDest[1] = static_cast<u32>(reinterpret_cast<uintptr_t>(lpBody));

    void* laDescriptor[4];
    laDescriptor[0] = &off_820A7F30;
    laDescriptor[1] = lpBody;
    cLionFX::BinSave(lpSrc->muLionFX, 1, laDescriptor);

    return *ppDestination;
}
}
