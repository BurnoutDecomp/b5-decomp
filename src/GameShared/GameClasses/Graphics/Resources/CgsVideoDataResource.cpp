#include "GameShared/GameClasses/Graphics/Resources/CgsVideoDataResource.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"        // E_RESOURCETYPE_VIDEODATA
#include "rw/rwcore_structs.h"                                                // rw::Resource

#include <cstring>   // strlen

// CgsResource::VideoDataResource[Type] -- the movie metadata resource (type 0x42 = 66). Faithful port
// of the X360 ARTIST bodies:
//   GetTypeID                       0x827EB0E8  -> 66
//   GetSerialisedResourceDescriptor 0x827F8308  -> slot0 {struct + 4-aligned name strings, align 4}
//   FixUp                           0x827FB340  -> rebase each VideoFile::mpcName (self-relative offset)
// The X360 FixUp walks the six name pointers at a 12-byte stride; here it walks the x64 layout via the
// VideoFile array (16-byte stride), so it stays correct for the platform-4 (x64) resource.

namespace CgsResource
{
    // ---- VideoFile ---------------------------------------------------------------------
    bool VideoDataResource::VideoFile::IsLocalisedSoundAvailable(EVideoLanguage leLanguage) const
    {
        return (leLanguage >= 0 && leLanguage < E_VIDEOLANGUAGE_COUNT) && mabAvailableLanguages[leLanguage];
    }

    u32 VideoDataResource::VideoFile::GetSoundStreamCount() const
    {
        u32 luCount = 0;
        for (u32 li = 0; li < E_VIDEOLANGUAGE_COUNT; ++li)
            if (mabAvailableLanguages[li])
                ++luCount;
        return luCount;
    }

    void VideoDataResource::VideoFile::FixUp()
    {
        // X360 0x827FB340: *(field) += field_address, unconditionally (no null check in the ASM --
        // straight-line code for all 6 fields). mpcName sits at offset 0 of the VideoFile, so its
        // stored self-relative offset is rebased by &mpcName (== the VideoFile address) -> absolute.
        mpcName = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(&mpcName)
                                                + reinterpret_cast<uintptr_t>(mpcName));
    }

    // ---- VideoDataResource -------------------------------------------------------------
    void VideoDataResource::FixUp()
    {
        for (u32 li = 0; li < E_VIDEOLANGUAGE_COUNT; ++li)
            maVideoFiles[li].FixUp();
    }

    u32 VideoDataResource::GetSizeOfResource() const
    {
        u32 luSize = static_cast<u32>(sizeof(VideoDataResource));   // the 6 VideoFile structs
        for (u32 li = 0; li < E_VIDEOLANGUAGE_COUNT; ++li)
            if (maVideoFiles[li].mpcName != 0)
            {
                const u32 luLen = static_cast<u32>(std::strlen(maVideoFiles[li].mpcName)) + 1u;  // incl NUL
                luSize += (luLen + 3u) & ~3u;                                                    // 4-aligned
            }
        return luSize;
    }

    VideoDataResource::VideoFile* VideoDataResource::GetVideoFile(EVideoLanguage leLanguage)
    {
        if (leLanguage < 0 || leLanguage >= E_VIDEOLANGUAGE_COUNT)
            leLanguage = E_VIDEOLANGUAGE_ENGLISH;
        return &maVideoFiles[leLanguage];
    }

    // ---- VideoDataResourceType (id 0x42) ----------------------------------------------
    uint32_t VideoDataResourceType::GetTypeID() const
    {
        return E_RESOURCETYPE_VIDEODATA;   // X360 0x827EB0E8 -> 66
    }

    ResourceDescriptor VideoDataResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // X360 0x827F8308: the whole resource lives in slot0 (main memory) -- size = the 6 VideoFile
        // structs + each non-null name (NUL-terminated, 4-aligned) appended inline; align 4. slots 1-4
        // are empty {0, align 1}. (rw::BaseResourceDescriptors<5> = 5 x {u32 size, u32 align}.)
        const VideoDataResource* lpVideo = static_cast<const VideoDataResource*>(lpResource);
        ResourceDescriptor lDescriptor;
        u32* lpDesc = reinterpret_cast<u32*>(&lDescriptor);

        u32 luSize = static_cast<u32>(sizeof(VideoDataResource));
        for (u32 li = 0; li < VideoDataResource::E_VIDEOLANGUAGE_COUNT; ++li)
        {
            const char* lpName = lpVideo->maVideoFiles[li].mpcName;
            if (lpName != 0)
            {
                const u32 luLen = static_cast<u32>(std::strlen(lpName)) + 1u;
                luSize += (luLen + 3u) & ~3u;
            }
        }

        lpDesc[0] = luSize; lpDesc[1] = 4u;   // slot0 = {size, align 4}
        lpDesc[2] = 0u; lpDesc[3] = 1u;       // slot1 (unused)
        lpDesc[4] = 0u; lpDesc[5] = 1u;       // slot2 (unused)
        lpDesc[6] = 0u; lpDesc[7] = 1u;       // slot3 (unused)
        lpDesc[8] = 0u; lpDesc[9] = 1u;       // slot4 (unused)
        return lDescriptor;
    }

    void VideoDataResourceType::FixUp(void* lpResource, const rw::Resource&) const
    {
        static_cast<VideoDataResource*>(lpResource)->FixUp();
    }
}
