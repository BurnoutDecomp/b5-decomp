#include "SharedClasses/Graphics/TextureNameMapResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::TextureNameMapResourceType::GetSerialisedResourceDescriptor @ 0x8267C350
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

    struct SerialisedTextureNameMap
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

    // GetSerialisedResourceDescriptor @ 0x8267C350 (store-for-store). The serialised payload packs a
    // 16-byte TextureNameMap header, then the 16-byte-aligned Entry table, then each entry's
    // GDB-name string 16-byte-aligned. The X360:
    //   r11 = a3[1] (muEntryCount); r8 = align16(8 * count) + 16   -- header + Entry table
    //   for each entry: r9 = strlen(entry.mpGDBTextureName); r8 += align16(strlen + 1)
    //   -> entry0 size = align16(8*count) + 16 + sum( align16(strlen(name)+1) )
    // entry0 align = 16; entry1..4 = {0,1}. Entry stride is sizeof(Entry) = 8; the per-entry name
    // pointer is the entry's mpGDBTextureName word (read via the local serialised struct).
    CgsResource::ResourceDescriptor
    TextureNameMapResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const SerialisedTextureNameMap* lpMap = static_cast<const SerialisedTextureNameMap*>(lpResource);

        u32 luSize = static_cast<u32>(Align16(sizeof(SerialisedTextureNameMap::Entry) * lpMap->muEntryCount)) + 16u;

        const SerialisedTextureNameMap::Entry* lpEntries = lpMap->GetEntries();
        for (u32 luEntry = 0; luEntry < lpMap->muEntryCount; ++luEntry)
        {
            const char* lpcName = PointerFromU32<const char>(lpEntries[luEntry].mpGDBTextureName);
            const u32   luStringBytes = static_cast<u32>(std::strlen(lpcName) + 1);
            luSize += static_cast<u32>(Align16(luStringBytes));
        }

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;   // entry0 size
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;      // entry0 align
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    void TextureNameMapResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));
        SerialisedTextureNameMap* lpMap = static_cast<SerialisedTextureNameMap*>(lpResource);

        lpMap->mpEntries += luDelta;
        SerialisedTextureNameMap::Entry* lpEntries = lpMap->GetEntries();
        for (u32 luEntry = 0; luEntry < lpMap->muEntryCount; ++luEntry)
            lpEntries[luEntry].mpGDBTextureName += luDelta;
    }

    void TextureNameMapResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]));
        SerialisedTextureNameMap* lpMap = static_cast<SerialisedTextureNameMap*>(lpResource);

        SerialisedTextureNameMap::Entry* lpEntries = lpMap->GetEntries();
        for (u32 luEntry = 0; luEntry < lpMap->muEntryCount; ++luEntry)
            lpEntries[luEntry].mpGDBTextureName -= luDelta;
        lpMap->mpEntries -= luDelta;
    }

    void* TextureNameMapResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        const SerialisedTextureNameMap* lpMap = static_cast<const SerialisedTextureNameMap*>(lpResource);
        SerialisedTextureNameMap* lpSerialisedMap = static_cast<SerialisedTextureNameMap*>(lrDest.m_baseResources[0]);
        u8* lpEntryWrite = reinterpret_cast<u8*>(lpSerialisedMap + 1);

        lpSerialisedMap->muEntryCount = lpMap->muEntryCount;
        lpSerialisedMap->mpEntries = AddressFromPointer(lpEntryWrite);

        const u32 luEntryBytes = sizeof(SerialisedTextureNameMap::Entry) * lpMap->muEntryCount;
        SerialisedTextureNameMap::Entry* lpSourceEntries = lpMap->GetEntries();
        SerialisedTextureNameMap::Entry* lpSerialisedEntries = lpSerialisedMap->GetEntries();
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
