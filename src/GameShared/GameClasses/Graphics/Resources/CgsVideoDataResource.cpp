#include "GameShared/GameClasses/Graphics/Resources/CgsVideoDataResource.h"
#include "rw/rwcore_structs.h"   // rw::Resource (unused here, kept for the base type)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::VideoDataResourceType::FixUp     @ 0x827FB340
//   CgsResource::VideoDataResourceType::GetTypeID @ 0x827EB0E8
//
// FixUp relocates six self-relative pointer fields (stride 12, offsets 0/12/.../60):
// each stored value gets the address of its own slot added to it (`*p += p`), the
// standard post-load pointer fixup. It uses no external delta, so the rw::Resource
// argument is unused.

namespace CgsResource
{
    static const uint32_t KU_VIDEO_DATA_RESOURCE_TYPE_ID = 66;
    static const u32 KU_FIELD_COUNT  = 6;
    static const u32 KU_FIELD_STRIDE = 12;

    uint32_t VideoDataResourceType::GetTypeID() const
    {
        return KU_VIDEO_DATA_RESOURCE_TYPE_ID;
    }

    void VideoDataResourceType::FixUp(void* lpResource, const rw::Resource&) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        for (u32 li = 0; li < KU_FIELD_COUNT; ++li)
        {
            uintptr_t lFieldAddr = lBase + li * KU_FIELD_STRIDE;
            char*& lrpField = *reinterpret_cast<char**>(lFieldAddr);
            lrpField += lFieldAddr;   // load-relative offset -> absolute pointer
        }
    }
}
