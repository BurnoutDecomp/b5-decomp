#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::VideoDataResourceType::FixUp     @ 0x827FB340
//   CgsResource::VideoDataResourceType::GetTypeID @ 0x827EB0E8
//
// FixUp relocates six self-relative pointer fields (stride 12 bytes, offsets
// 0,12,24,36,48,60) from load-relative offsets into absolute addresses: each
// stored value gets the address of its own slot added to it (`*p += p`), the
// standard post-load pointer fixup for a serialised resource. `this` is unused.

namespace CgsResource
{
    class VideoDataResourceType
    {
    public:
        void* FixUp(void* pResource);
        int   GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 66;
        static const u32 KU_FIELD_COUNT = 6;
        static const u32 KU_FIELD_STRIDE = 12;
    };

    void* VideoDataResourceType::FixUp(void* pResource)
    {
        uintptr_t base = reinterpret_cast<uintptr_t>(pResource);
        for (u32 i = 0; i < KU_FIELD_COUNT; ++i)
        {
            uintptr_t fieldAddr = base + i * KU_FIELD_STRIDE;
            char*& rpField = *reinterpret_cast<char**>(fieldAddr);
            rpField += fieldAddr;   // load-relative offset -> absolute pointer
        }

        return pResource;
    }
}
