#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ModelResourceType::FixDown                       @ 0x828A8548
//   CgsResource::ModelResourceType::GetImportCount                @ 0x828A7D48
//   CgsResource::ModelResourceType::GetImportPointer              @ 0x828A7D08
//   CgsResource::ModelResourceType::GetSerialisedResourceDescriptor @ 0x828A9418
//   CgsResource::ModelResourceType::GetTypeID                     @ 0x828A7A60
//
// FixDown rebases the three pointers at offsets 0/4/8 of the model resource by the
// relocation delta. GetImportCount/GetImportPointer read the import table (count at
// +16, pointer base at +0). GetSerialisedResourceDescriptor builds a five-entry rw
// descriptor whose first entry's size is the rounded sum of three sub-block sizes
// (each derived from the field counts at +16/+18, *4 and 4-byte aligned) plus 20.

namespace CgsResource
{
    namespace
    {
        // Rotate-left by 2, matching the X360 __ROL4__ idiom (== *4 for in-range counts).
        inline u32 RotL2(u32 luValue)
        {
            return (luValue << 2) | (luValue >> 30);
        }

        inline u32 Align4(u32 luValue)
        {
            return (luValue + 3) & ~static_cast<u32>(3);
        }
    }

    class ModelResourceType
    {
    public:
        void* FixDown(void* pResource, const int* pDelta);
        int   GetImportCount(const void* pResource) { return *reinterpret_cast<const u16*>(reinterpret_cast<uintptr_t>(pResource) + 16); }
        void* GetImportPointer(void* pResource, u32 luIndex, u32* pOutOffset, u32* pOutValue);
        void* GetSerialisedResourceDescriptor(void* pOut, const void* pResource);
        int   GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 42;
    };

    void* ModelResourceType::FixDown(void* pResource, const int* pDelta)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(pResource);
        const u32 luDelta = static_cast<u32>(*pDelta);

        u32 luP1 = *reinterpret_cast<u32*>(lBase + 4) - luDelta;
        u32 luP2 = *reinterpret_cast<u32*>(lBase + 8) - luDelta;
        *reinterpret_cast<u32*>(lBase + 0) -= luDelta;
        *reinterpret_cast<u32*>(lBase + 4) = luP1;
        *reinterpret_cast<u32*>(lBase + 8) = luP2;
        return pResource;
    }

    void* ModelResourceType::GetImportPointer(void* pResource, u32 luIndex, u32* pOutOffset, u32* pOutValue)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(pResource);
        u32 luCount = *reinterpret_cast<const u16*>(lBase + 16);

        if (luIndex >= luCount)
        {
            *pOutValue = 0;
            *pOutOffset = 0;
        }
        else
        {
            uintptr_t lTable = *reinterpret_cast<u32*>(lBase + 0);
            *pOutValue = *reinterpret_cast<u32*>(lTable + 4 * luIndex);
            *pOutOffset = 4 * (luIndex + 5);
        }
        return pResource;
    }

    void* ModelResourceType::GetSerialisedResourceDescriptor(void* pOut, const void* pResource)
    {
        u32* lpOut = reinterpret_cast<u32*>(pOut);
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);

        u32 luField18 = *reinterpret_cast<const u16*>(lRes + 18);
        u32 luField16x4 = RotL2(*reinterpret_cast<const u16*>(lRes + 16));

        lpOut[1] = 1;
        lpOut[3] = 1;
        lpOut[5] = 1;
        lpOut[7] = 1;
        lpOut[9] = 1;
        lpOut[2] = 0;
        lpOut[4] = 0;
        lpOut[6] = 0;
        lpOut[8] = 0;

        // 64-bit store: high word = total rounded size, low word = 4.
        u32 luTotal = Align4(luField16x4)
                    + Align4(RotL2(luField18))
                    + Align4(luField18)
                    + 20;
        lpOut[0] = luTotal;
        lpOut[1] = 4;
        return lpOut;
    }
}
