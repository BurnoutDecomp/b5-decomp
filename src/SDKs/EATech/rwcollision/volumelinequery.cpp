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
        class VolumeLineQuery
        {
        public:
            int   AddPrimitiveRef(int liVolume, const void* pTransform, int liTag, char lcFlag);
            int   AddVolumeRef(int liVolume, const void* pTransform, int liTag, char lcFlag);
            int   GetAllIntersections();
            void* GetResourceDescriptor(void* pOut, int liVolumes, int liResults);
            void* Initialize(void** ppBuffer, int liVolumes, int liResults);

            // Defined in another TU.
            int   GetIntersections();
        };

        int VolumeLineQuery::AddPrimitiveRef(int liVolume, const void* pTransform, int liTag, char lcFlag)
        {
            u32* lpThis = reinterpret_cast<u32*>(this);
            u32 luIndex = lpThis[55];
            if (luIndex >= lpThis[56])
                return 0;

            uintptr_t lEntry = (luIndex << 7) + lpThis[54];
            *reinterpret_cast<int*>(lEntry) = liVolume;

            if (pTransform)
            {
                memcpy(reinterpret_cast<void*>(lEntry + 16), pTransform, 64);
                *reinterpret_cast<u32*>(lEntry + 4) = static_cast<u32>(lEntry + 16);
            }
            else
            {
                *reinterpret_cast<u32*>(lEntry + 4) = 0;
            }

            *reinterpret_cast<int*>(lEntry + 112) = liTag;
            *reinterpret_cast<char*>(lEntry + 116) = lcFlag;
            ++lpThis[55];
            return 1;
        }

        int VolumeLineQuery::AddVolumeRef(int liVolume, const void* pTransform, int liTag, char lcFlag)
        {
            // Volumes whose type word is not 6 are stored as primitive refs.
            uintptr_t lVolAddr = static_cast<uintptr_t>(static_cast<u32>(liVolume));
            int* lpType = *reinterpret_cast<int**>(lVolAddr + 64);
            if (*lpType != 6)
                return AddPrimitiveRef(liVolume, pTransform, liTag, lcFlag);

            u32* lpThis = reinterpret_cast<u32*>(this);
            u32 luIndex = lpThis[52];
            if (luIndex >= lpThis[53])
                return 0;

            uintptr_t lEntry = (luIndex << 7) + lpThis[17];
            *reinterpret_cast<int*>(lEntry) = liVolume;

            if (pTransform)
            {
                memcpy(reinterpret_cast<void*>(lEntry + 16), pTransform, 64);
                *reinterpret_cast<u32*>(lEntry + 4) = static_cast<u32>(lEntry + 16);
            }
            else
            {
                *reinterpret_cast<u32*>(lEntry + 4) = 0;
            }

            *reinterpret_cast<int*>(lEntry + 112) = liTag;
            *reinterpret_cast<char*>(lEntry + 116) = lcFlag;
            ++lpThis[52];
            return 1;
        }

        int VolumeLineQuery::GetAllIntersections()
        {
            u32* lpThis = reinterpret_cast<u32*>(this);
            u32 luTotal = lpThis[7];
            lpThis[64] = 0;
            lpThis[6] = luTotal;
            return GetIntersections();
        }

        void* VolumeLineQuery::GetResourceDescriptor(void* pOut, int liVolumes, int liResults)
        {
            u32* lpOut = reinterpret_cast<u32*>(pOut);
            for (int liEntry = 0; liEntry < 5; ++liEntry)
            {
                lpOut[0] = 0;
                lpOut[1] = 1;
                lpOut += 2;
            }
            u32* lpBase = reinterpret_cast<u32*>(pOut);
            // 64-bit store {high = total backing size, low = 16}.
            lpBase[0] = static_cast<u32>(432 * liResults + (liVolumes << 7) + 10640);
            lpBase[1] = 16;
            return pOut;
        }

        void* VolumeLineQuery::Initialize(void** ppBuffer, int liVolumes, int liResults)
        {
            u32* lpQuery = reinterpret_cast<u32*>(*ppBuffer);
            if (!lpQuery)
                return nullptr;

            lpQuery[53] = static_cast<u32>(liVolumes);
            lpQuery[56] = static_cast<u32>(liResults);
            lpQuery[7]  = static_cast<u32>(liResults);
            lpQuery[59] = static_cast<u32>(liResults);

            uintptr_t lVolumeBase = reinterpret_cast<uintptr_t>(&lpQuery[32 * liVolumes + 68]);
            lpQuery[17] = static_cast<u32>(reinterpret_cast<uintptr_t>(&lpQuery[68]));
            lpQuery[54] = static_cast<u32>(lVolumeBase);

            uintptr_t lAfterEntries = (static_cast<uintptr_t>(liResults) << 7) + lVolumeBase;
            lpQuery[57] = static_cast<u32>(lAfterEntries);

            uintptr_t lResultList = 96 * liResults + lAfterEntries;
            lpQuery[4] = static_cast<u32>(lResultList);
            lpQuery[61] = static_cast<u32>(208 * liResults + lResultList);
            return lpQuery;
        }
    }
}
