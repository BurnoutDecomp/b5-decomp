#include "SharedClasses/Graphics/TextureNameMapResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::TextureNameMapResourceType::Serialise @ 0x82678310
//   BrnParticle::TextureNameMapResourceType::FixUp     @ 0x826783F0
//   BrnParticle::TextureNameMapResourceType::FixDown   @ 0x82678440
//   BrnParticle::TextureNameMapResourceType::GetTypeID @ 0x826758B8
//
// FixUp/FixDown rebase the entry table and each entry's GDB-name pointer by the delta
// (the rw::Resource's load base). Serialise packs the map + entries + 16-byte-aligned
// name strings into the destination resource's buffer.

namespace BrnParticle
{
    template <typename T>
    static T* PointerFromU32(u32 luAddress) { return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress)); }
    static u32 AddressFromPointer(const void* lpPointer) { return static_cast<u32>(reinterpret_cast<uintptr_t>(lpPointer)); }
    static uintptr_t Align16(uintptr_t luValue) { return (luValue + 15) & ~static_cast<uintptr_t>(15); }

    struct TextureNameMap
    {
        struct Entry
        {
            u32 muHashedLionTextureName;
            u32 mpGDBTextureName;
        };

        Entry* GetEntries() const { return PointerFromU32<Entry>(mpEntries); }

        u32 mpEntries;
        u32 muEntryCount;
    };

    static const uint32_t KU_TEXTURE_NAME_MAP_RESOURCE_TYPE_ID = 65547;

    uint32_t TextureNameMapResourceType::GetTypeID() const
    {
        return KU_TEXTURE_NAME_MAP_RESOURCE_TYPE_ID;
    }

    void TextureNameMapResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));
        TextureNameMap* lpMap = static_cast<TextureNameMap*>(lpResource);

        lpMap->mpEntries += luDelta;
        TextureNameMap::Entry* lpEntries = lpMap->GetEntries();
        for (u32 luEntry = 0; luEntry < lpMap->muEntryCount; ++luEntry)
            lpEntries[luEntry].mpGDBTextureName += luDelta;
    }

    void TextureNameMapResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));
        TextureNameMap* lpMap = static_cast<TextureNameMap*>(lpResource);

        TextureNameMap::Entry* lpEntries = lpMap->GetEntries();
        for (u32 luEntry = 0; luEntry < lpMap->muEntryCount; ++luEntry)
            lpEntries[luEntry].mpGDBTextureName -= luDelta;
        lpMap->mpEntries -= luDelta;
    }

    void* TextureNameMapResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        const TextureNameMap* lpMap = static_cast<const TextureNameMap*>(lpResource);
        TextureNameMap* lpSerialisedMap = static_cast<TextureNameMap*>(lrDest.m_baseResources[0]);
        u8* lpEntryWrite = reinterpret_cast<u8*>(lpSerialisedMap + 1);

        lpSerialisedMap->muEntryCount = lpMap->muEntryCount;
        lpSerialisedMap->mpEntries = AddressFromPointer(lpEntryWrite);

        const u32 luEntryBytes = sizeof(TextureNameMap::Entry) * lpMap->muEntryCount;
        TextureNameMap::Entry* lpSourceEntries = lpMap->GetEntries();
        TextureNameMap::Entry* lpSerialisedEntries = lpSerialisedMap->GetEntries();
        std::memcpy(lpSerialisedEntries, lpSourceEntries, luEntryBytes);

        u8* lpStringWrite = reinterpret_cast<u8*>(Align16(reinterpret_cast<uintptr_t>(lpEntryWrite + luEntryBytes)));
        for (u32 luEntry = 0; luEntry < lpSerialisedMap->muEntryCount; ++luEntry)
        {
            const char* lpcTextureName = PointerFromU32<char>(lpSourceEntries[luEntry].mpGDBTextureName);
            const u32 luStringBytes = static_cast<u32>(std::strlen(lpcTextureName) + 1);
            std::memcpy(lpStringWrite, lpcTextureName, luStringBytes);
            lpSerialisedEntries[luEntry].mpGDBTextureName = AddressFromPointer(lpStringWrite);
            lpStringWrite = reinterpret_cast<u8*>(Align16(reinterpret_cast<uintptr_t>(lpStringWrite + luStringBytes)));
        }

        return lpSerialisedMap;
    }
}
