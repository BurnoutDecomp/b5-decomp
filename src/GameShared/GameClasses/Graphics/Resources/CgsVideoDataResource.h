#ifndef CGS_VIDEO_DATA_RESOURCE_H
#define CGS_VIDEO_DATA_RESOURCE_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"   // Type, ResourceDescriptor (+ rw::Resource)

namespace CgsResource
{
    // The movie metadata resource (type 0x42 = 66; DecFIGS GameShared/.../Graphics/Resources/
    // CgsVideoDataResource). Per-language video-file names + localized-sound availability. The actual
    // movie is a disk-streamed file referenced by VideoFile::mpcName -- this resource is just the
    // name/language table. The X360 stores each mpcName as a self-relative offset (FixUp rebases it),
    // with the name strings appended inline after the 6 VideoFile structs.
    class VideoDataResource
    {
    public:
        enum EVideoLanguage
        {
            E_VIDEOLANGUAGE_ENGLISH  = 0,
            E_VIDEOLANGUAGE_FRENCH   = 1,
            E_VIDEOLANGUAGE_ITALIAN  = 2,
            E_VIDEOLANGUAGE_GERMAN   = 3,
            E_VIDEOLANGUAGE_SPANISH  = 4,
            E_VIDEOLANGUAGE_JAPANESE = 5,
            E_VIDEOLANGUAGE_COUNT    = 6,
        };

        // One localized video: the movie file name + which languages have a localized sound stream.
        // [x64: mpcName is an 8-byte pointer, so VideoFile is 16 bytes -- the X360 layout is 12.]
        class VideoFile
        {
        public:
            const char* GetName() const { return mpcName; }
            bool        IsLocalisedSoundAvailable(EVideoLanguage leLanguage) const;
            u32         GetSoundStreamCount() const;
            void        FixUp();   // rebase mpcName (self-relative offset -> absolute pointer)

            const char* mpcName;                                        // +0x00  movie file name
            bool        mabAvailableLanguages[E_VIDEOLANGUAGE_COUNT];   // +0x08  localized-sound flags
        };

        void       FixUp();                                  // rebase the 6 names (X360 0x827FB340)
        u32        GetSizeOfResource() const;                // the structs + appended name strings
        VideoFile* GetVideoFile(EVideoLanguage leLanguage);  // the per-language video

        VideoFile maVideoFiles[E_VIDEOLANGUAGE_COUNT];       // 6 x 16 = 96 bytes (x64)
    };

    // Resource-type handler for serialised video data (id 0x42). Derives from CgsResource::Type;
    // GetTypeID/GetSerialisedResourceDescriptor/FixUp are virtual overrides. From DecFIGS DWARF +
    // the X360 ARTIST bodies (GetTypeID 0x827EB0E8, GetSerialisedResourceDescriptor 0x827F8308,
    // FixUp 0x827FB340).
    class VideoDataResourceType : public Type
    {
    public:
        uint32_t           GetTypeID() const override;
        ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
        void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    };

    void RegisterVideoDataResourceType();
}

#endif
