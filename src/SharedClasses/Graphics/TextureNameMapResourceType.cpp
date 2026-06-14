#include "types.hpp"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::TextureNameMapResourceType::Serialise @ 0x82678310
//   BrnParticle::TextureNameMapResourceType::FixUp     @ 0x826783F0
//   BrnParticle::TextureNameMapResourceType::FixDown   @ 0x82678440
//   BrnParticle::TextureNameMapResourceType::GetTypeID @ 0x826758B8

namespace BrnParticle
{
    static const u32 KI_TEXTURE_NAME_MAP_RESOURCE_TYPE_ID = 65547;

    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    static u32 AddressFromPointer(const void* lpPointer)
    {
        return static_cast<u32>(reinterpret_cast<uintptr_t>(lpPointer));
    }

    static uintptr_t Align16(uintptr_t luValue)
    {
        return (luValue + 15) & ~static_cast<uintptr_t>(15);
    }

    struct TextureNameMap
    {
        struct Entry
        {
            u32 muHashedLionTextureName;
            u32 mpGDBTextureName;
        };

        Entry* GetEntries() const;

        u32 mpEntries;
        u32 muEntryCount;
    };

    class TextureNameMapResourceType
    {
    public:
        TextureNameMap* Serialise(const TextureNameMap* lpMap, TextureNameMap** lppDestination) const;
        TextureNameMap* FixUp(TextureNameMap* lpMap, const s32* lpiDelta) const;
        TextureNameMap* FixDown(TextureNameMap* lpMap, const s32* lpiDelta) const;
        u32 GetTypeID() const;
    };

    TextureNameMap::Entry* TextureNameMap::GetEntries() const
    {
        return PointerFromU32<Entry>(mpEntries);
    }

    TextureNameMap* TextureNameMapResourceType::Serialise(
        const TextureNameMap* lpMap,
        TextureNameMap** lppDestination) const
    {
        TextureNameMap* lpSerialisedMap = *lppDestination;
        u8* lpEntryWrite = reinterpret_cast<u8*>(lpSerialisedMap + 1);

        lpSerialisedMap->muEntryCount = lpMap->muEntryCount;
        lpSerialisedMap->mpEntries = AddressFromPointer(lpEntryWrite);

        const u32 luEntryBytes = sizeof(TextureNameMap::Entry) * lpMap->muEntryCount;
        TextureNameMap::Entry* lpSourceEntries = lpMap->GetEntries();
        TextureNameMap::Entry* lpSerialisedEntries = lpSerialisedMap->GetEntries();
        std::memcpy(lpSerialisedEntries, lpSourceEntries, luEntryBytes);

        u8* lpStringWrite = reinterpret_cast<u8*>(
            Align16(reinterpret_cast<uintptr_t>(lpEntryWrite + luEntryBytes)));

        for (u32 luEntry = 0; luEntry < lpSerialisedMap->muEntryCount; ++luEntry)
        {
            const char* lpcTextureName = PointerFromU32<char>(lpSourceEntries[luEntry].mpGDBTextureName);
            const u32 luStringBytes = static_cast<u32>(std::strlen(lpcTextureName) + 1);
            std::memcpy(lpStringWrite, lpcTextureName, luStringBytes);
            lpSerialisedEntries[luEntry].mpGDBTextureName = AddressFromPointer(lpStringWrite);
            lpStringWrite = reinterpret_cast<u8*>(
                Align16(reinterpret_cast<uintptr_t>(lpStringWrite + luStringBytes)));
        }

        return lpSerialisedMap;
    }

    TextureNameMap* TextureNameMapResourceType::FixUp(TextureNameMap* lpMap, const s32* lpiDelta) const
    {
        lpMap->mpEntries += static_cast<u32>(*lpiDelta);
        TextureNameMap::Entry* lpEntries = lpMap->GetEntries();

        for (u32 luEntry = 0; luEntry < lpMap->muEntryCount; ++luEntry)
        {
            lpEntries[luEntry].mpGDBTextureName += static_cast<u32>(*lpiDelta);
        }

        return lpMap;
    }

    TextureNameMap* TextureNameMapResourceType::FixDown(TextureNameMap* lpMap, const s32* lpiDelta) const
    {
        TextureNameMap::Entry* lpEntries = lpMap->GetEntries();

        for (u32 luEntry = 0; luEntry < lpMap->muEntryCount; ++luEntry)
        {
            lpEntries[luEntry].mpGDBTextureName -= static_cast<u32>(*lpiDelta);
        }

        lpMap->mpEntries -= static_cast<u32>(*lpiDelta);
        return lpMap;
    }

    u32 TextureNameMapResourceType::GetTypeID() const
    {
        return KI_TEXTURE_NAME_MAP_RESOURCE_TYPE_ID;
    }
}
