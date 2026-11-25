#include "GameShared/GameClasses/Language/Resources/CgsLanguageResourceType.h"

#include "GameShared/GameClasses/Fonts/CgsUnicode.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#define CGS_ARTIST_LANGUAGE_RESOURCE_FILE "..\\..\\..\\GameShared\\GameClasses\\Language/Resources/CgsLanguageResourceType.cpp"

namespace CgsResource
{
    inline u32 LanguageResourceHashEntry::GetSize() const
    {
        return CgsUnicode::ByteLength(mpString) + 1;
    }

    // The serialized pointer slots carry native 32-bit offsets until FixUp rebases them.
    inline void LanguageResourceHashEntry::FixDown(const rw::Resource& lrResource)
    {
        const u32 luBase = GetLoadBase(lrResource);
        CGS_ASSERT_AT(luBase != 0, "No Memory Resource", CGS_ARTIST_LANGUAGE_RESOURCE_FILE, 46);
        mpString = reinterpret_cast<u8*>(reinterpret_cast<uintptr_t>(mpString) - luBase);
    }

    inline void LanguageResourceHashEntry::FixUp(const rw::Resource& lrResource)
    {
        const u32 luBase = GetLoadBase(lrResource);
        CGS_ASSERT_AT(luBase != 0, "No Memory Resource", CGS_ARTIST_LANGUAGE_RESOURCE_FILE, 66);
        mpString = reinterpret_cast<u8*>(reinterpret_cast<uintptr_t>(mpString) + luBase);
    }

    u32 LanguageResourceType::GetTypeID() const
    {
        return 39;
    }

    ResourceDescriptor LanguageResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        CGS_ASSERT_AT(lpResource != 0, "NULL resource passed to GetSerialisedResourceDescriptor",
                      CGS_ARTIST_LANGUAGE_RESOURCE_FILE, 117);
        const LanguageResource& lrResource = *static_cast<const LanguageResource*>(lpResource);
        u32 luSize = sizeof(LanguageResource);
        for (u32 luIndex = 0; luIndex < lrResource.muSize; ++luIndex)
        {
            luSize += lrResource.mpEntries[luIndex].GetSize() + sizeof(LanguageResourceHashEntry);
        }
        const rw::BaseResourceDescriptor lMemoryDescriptor(luSize, 1);
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0] = lMemoryDescriptor;
        return lDescriptor;
    }

    void LanguageResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        LanguageResource& lrTable = *static_cast<LanguageResource*>(lpResource);
        const u32 luBase = GetLoadBase(lrResource);
        for (u32 luIndex = 0; luIndex < lrTable.muSize; ++luIndex)
            lrTable.mpEntries[luIndex].FixDown(lrResource);
        lrTable.mpEntries = reinterpret_cast<LanguageResourceHashEntry*>(
            reinterpret_cast<uintptr_t>(lrTable.mpEntries) - luBase);
    }

    void LanguageResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        LanguageResource& lrTable = *static_cast<LanguageResource*>(lpResource);
        lrTable.mpEntries = reinterpret_cast<LanguageResourceHashEntry*>(
            reinterpret_cast<uintptr_t>(lrTable.mpEntries) + GetLoadBase(lrResource));
        for (u32 luIndex = 0; luIndex < lrTable.muSize; ++luIndex)
            lrTable.mpEntries[luIndex].FixUp(lrResource);
    }
}

#undef CGS_ARTIST_LANGUAGE_RESOURCE_FILE
