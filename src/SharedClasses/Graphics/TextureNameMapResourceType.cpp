#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::TextureNameMapResourceType::FixDown   @ 0x82678440
//   BrnParticle::TextureNameMapResourceType::FixUp     @ 0x826783F0
//   BrnParticle::TextureNameMapResourceType::GetTypeID @ 0x826758B8
//   BrnParticle::TextureNameMapResourceType::Serialise @ 0x82678310
//
// The texture-name map is a {entries pointer, entry count} header over an array of
// {key, name-pointer} entries. FixUp/FixDown rebase the entries-array pointer and each
// entry's name pointer by the load delta. Serialise packs the map into a destination
// buffer: a 16-byte-aligned header, then the entry array, then each name string copied and
// 16-byte aligned, rewriting every entry's name pointer to its packed location. Addresses
// inside the serialised blob are 32-bit by format, so relocatable fields are u32.

namespace BrnParticle
{
namespace
{
    inline u32 Align16(u32 luValue) { return (luValue + 15) & ~static_cast<u32>(15); }
}

struct NameMapEntry
{
    u32 muKey;    // +0
    u32 muName;   // +4  relocatable address of the name string
};

struct TextureNameMap
{
    u32 muEntries;   // +0  relocatable address of the entry array
    u32 muCount;     // +4
};

class TextureNameMapResourceType
{
public:
    void* FixDown(void* pResource, const int* pDelta);
    void* FixUp(void* pResource, const int* pDelta);
    int   GetTypeID() { return KI_TYPE_ID; }
    void* Serialise(void* pResource, void* pSource, void** ppDestination);

private:
    static const int KI_TYPE_ID = 65547;
};

void* TextureNameMapResourceType::FixDown(void* pResource, const int* pDelta)
{
    TextureNameMap* lpMap = static_cast<TextureNameMap*>(pResource);
    const u32 luDelta = static_cast<u32>(*pDelta);

    NameMapEntry* lpEntries = reinterpret_cast<NameMapEntry*>(static_cast<uintptr_t>(lpMap->muEntries));
    for (u32 luIndex = 0; luIndex < lpMap->muCount; ++luIndex)
        lpEntries[luIndex].muName -= luDelta;

    lpMap->muEntries -= luDelta;
    return pResource;
}

void* TextureNameMapResourceType::FixUp(void* pResource, const int* pDelta)
{
    TextureNameMap* lpMap = static_cast<TextureNameMap*>(pResource);
    const u32 luDelta = static_cast<u32>(*pDelta);

    lpMap->muEntries += luDelta;
    NameMapEntry* lpEntries = reinterpret_cast<NameMapEntry*>(static_cast<uintptr_t>(lpMap->muEntries));
    for (u32 luIndex = 0; luIndex < lpMap->muCount; ++luIndex)
        lpEntries[luIndex].muName += luDelta;

    return pResource;
}

void* TextureNameMapResourceType::Serialise(void* /*pResource*/, void* pSource, void** ppDestination)
{
    const TextureNameMap* lpSrc = static_cast<const TextureNameMap*>(pSource);
    u8* lpBase = static_cast<u8*>(*ppDestination);

    // 16-byte-aligned header, then the entry array.
    NameMapEntry*   lpEntries = reinterpret_cast<NameMapEntry*>(lpBase + 16);
    TextureNameMap* lpDest    = reinterpret_cast<TextureNameMap*>(lpBase);
    lpDest->muCount   = lpSrc->muCount;
    lpDest->muEntries = static_cast<u32>(reinterpret_cast<uintptr_t>(lpEntries));
    memcpy(lpEntries, reinterpret_cast<const void*>(static_cast<uintptr_t>(lpSrc->muEntries)), 8 * lpSrc->muCount);

    // Pack each name string after the entry array, 16-byte aligned, repointing the entry.
    u8* lpStrings = reinterpret_cast<u8*>(lpEntries) + Align16(8 * lpSrc->muCount);
    for (u32 luIndex = 0; luIndex < lpDest->muCount; ++luIndex)
    {
        const char* lpName = reinterpret_cast<const char*>(static_cast<uintptr_t>(lpEntries[luIndex].muName));
        u32 luLen = static_cast<u32>(strlen(lpName)) + 1;
        memcpy(lpStrings, lpName, luLen);
        lpEntries[luIndex].muName = static_cast<u32>(reinterpret_cast<uintptr_t>(lpStrings));
        lpStrings += Align16(luLen);
    }

    return lpBase;
}
}
