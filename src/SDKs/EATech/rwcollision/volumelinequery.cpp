#include "types.hpp"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::collision::VolumeLineQuery::AddPrimitiveRef      @ 0x82BB3230
//   rw::collision::VolumeLineQuery::AddVolumeRef         @ 0x82BB3300
//   rw::collision::VolumeLineQuery::GetAllIntersections  @ 0x82BB3820
//   rw::collision::VolumeLineQuery::GetResourceDescriptor@ 0x82BB3838
//   rw::collision::VolumeLineQuery::Initialize           @ 0x82BB3888
//
// The query holds two parallel ref arrays of 128-byte entries: a "primitive" array
// (base word[54], count word[55], capacity word[56]) and a "volume" array (base
// word[17], count word[52], capacity word[53]). Each entry: [+0] the volume/primitive
// pointer, [+4] a pointer to the optional 64-byte transform copied in at [+16], [+112]
// an int tag, [+116] a byte flag. AddVolumeRef routes non-type-6 volumes to the
// primitive array. Initialize partitions the backing buffer into the entry arrays and
// the result list. GetResourceDescriptor sizes the backing buffer for a2 volumes /
// a3 results.

namespace rw
{
    namespace collision
    {
        struct ResourceDescriptorEntry
        {
            u32 muSize;
            u32 muAlignment;
        };

        struct QueryRefEntry
        {
            int   miVolume;
            u32   muTransformPtr;
            u8    mPad8[8];
            u8    maTransform[64];
            u8    mPad80[32];
            int   miTag;
            char  mcFlag;
            u8    mPad117[11];
        };

        static_assert(sizeof(QueryRefEntry) == 128, "QueryRefEntry must match X360 stride");

        struct VolumeHeader
        {
            u8   mPad0[64];
            int* mpType;
        };

        class VolumeLineQuery
        {
        public:
            int   AddPrimitiveRef(VolumeHeader* pVolume, const void* pTransform, int liTag, char lcFlag);
            int   AddVolumeRef(VolumeHeader* pVolume, const void* pTransform, int liTag, char lcFlag);
            int   GetAllIntersections();
            void* GetResourceDescriptor(void* pOut, int liVolumes, int liResults);
            void* Initialize(void** ppBuffer, int liVolumes, int liResults);

            // Defined in another TU.
            int   GetIntersections();

        private:
            u8  mPad0[16];
            void* mpResultList;
            u8  mPad20[4];
            u32 muActiveIntersectionCount;
            u32 muTotalIntersectionCount;
            u8  mPad32[36];
            QueryRefEntry* mpVolumeRefs;
            u8  mPad72[136];
            u32 muVolumeRefCount;
            u32 muVolumeRefCapacity;
            QueryRefEntry* mpPrimitiveRefs;
            u32 muPrimitiveRefCount;
            u32 muPrimitiveRefCapacity;
            void* mpScratchBase;
            u8  mPad232[4];
            u32 muResultCapacity;
            u8  mPad240[4];
            void* mpTailBuffer;
            u8  mPad248[8];
            u32 muIntersectionWriteCount;
        };

        int VolumeLineQuery::AddPrimitiveRef(VolumeHeader* pVolume, const void* pTransform, int liTag, char lcFlag)
        {
            if (muPrimitiveRefCount >= muPrimitiveRefCapacity)
                return 0;

            QueryRefEntry* lpEntry = &mpPrimitiveRefs[muPrimitiveRefCount];
            lpEntry->miVolume = static_cast<int>(reinterpret_cast<uintptr_t>(pVolume));

            if (pTransform)
            {
                memcpy(lpEntry->maTransform, pTransform, sizeof(lpEntry->maTransform));
                lpEntry->muTransformPtr = static_cast<u32>(reinterpret_cast<uintptr_t>(lpEntry->maTransform));
            }
            else
            {
                lpEntry->muTransformPtr = 0;
            }

            lpEntry->miTag = liTag;
            lpEntry->mcFlag = lcFlag;
            ++muPrimitiveRefCount;
            return 1;
        }

        int VolumeLineQuery::AddVolumeRef(VolumeHeader* pVolume, const void* pTransform, int liTag, char lcFlag)
        {
            if (*pVolume->mpType != 6)
                return AddPrimitiveRef(pVolume, pTransform, liTag, lcFlag);

            if (muVolumeRefCount >= muVolumeRefCapacity)
                return 0;

            QueryRefEntry* lpEntry = &mpVolumeRefs[muVolumeRefCount];
            lpEntry->miVolume = static_cast<int>(reinterpret_cast<uintptr_t>(pVolume));

            if (pTransform)
            {
                memcpy(lpEntry->maTransform, pTransform, sizeof(lpEntry->maTransform));
                lpEntry->muTransformPtr = static_cast<u32>(reinterpret_cast<uintptr_t>(lpEntry->maTransform));
            }
            else
            {
                lpEntry->muTransformPtr = 0;
            }

            lpEntry->miTag = liTag;
            lpEntry->mcFlag = lcFlag;
            ++muVolumeRefCount;
            return 1;
        }

        int VolumeLineQuery::GetAllIntersections()
        {
            muIntersectionWriteCount = 0;
            muActiveIntersectionCount = muTotalIntersectionCount;
            return GetIntersections();
        }

        void* VolumeLineQuery::GetResourceDescriptor(void* pOut, int liVolumes, int liResults)
        {
            ResourceDescriptorEntry* lpOut = static_cast<ResourceDescriptorEntry*>(pOut);
            for (ResourceDescriptorEntry* lpEntry = lpOut; lpEntry != lpOut + 5; ++lpEntry)
            {
                lpEntry->muSize = 0;
                lpEntry->muAlignment = 1;
            }
            lpOut[0].muSize = static_cast<u32>(432 * liResults + (liVolumes << 7) + 10640);
            lpOut[0].muAlignment = 16;
            return pOut;
        }

        void* VolumeLineQuery::Initialize(void** ppBuffer, int liVolumes, int liResults)
        {
            VolumeLineQuery* lpQuery = static_cast<VolumeLineQuery*>(*ppBuffer);
            if (!lpQuery)
                return nullptr;

            lpQuery->muVolumeRefCapacity = static_cast<u32>(liVolumes);
            lpQuery->muPrimitiveRefCapacity = static_cast<u32>(liResults);
            lpQuery->muTotalIntersectionCount = static_cast<u32>(liResults);
            lpQuery->muResultCapacity = static_cast<u32>(liResults);

            u8* lpBacking = reinterpret_cast<u8*>(lpQuery);
            QueryRefEntry* lpVolumeBase = reinterpret_cast<QueryRefEntry*>(lpBacking + 272);
            QueryRefEntry* lpPrimitiveBase = lpVolumeBase + liVolumes;
            lpQuery->mpVolumeRefs = lpVolumeBase;
            lpQuery->mpPrimitiveRefs = lpPrimitiveBase;

            u8* lpScratchBase = reinterpret_cast<u8*>(lpPrimitiveBase + liResults);
            lpQuery->mpScratchBase = lpScratchBase;

            u8* lpResultList = lpScratchBase + 96 * liResults;
            lpQuery->mpResultList = lpResultList;
            lpQuery->mpTailBuffer = lpResultList + 208 * liResults;
            return lpQuery;
        }
    }
}
