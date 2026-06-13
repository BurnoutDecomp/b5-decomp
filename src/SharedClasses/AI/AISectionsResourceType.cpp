#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnAI::AISectionsResourceType::FixDown                       @ 0x8267F4A0
//   BrnAI::AISectionsResourceType::FixUp                         @ 0x8267DB28
//   BrnAI::AISectionsResourceType::GetSerialisedResourceDescriptor @ 0x8267AE88
//   BrnAI::AISectionsResourceType::GetTypeID                     @ 0x82674D50
//
// FixUp/FixDown forward to BrnAI::AISectionsData (forward-declared, separate TU).
// GetSerialisedResourceDescriptor fills a five-entry rw resource descriptor table:
// the first entry is {size=16, count=N}, the remaining four are {size=N, align=1},
// where N is the section count read from the resource (offset 60).

namespace BrnAI
{
    struct AISectionsData
    {
        int FixUp(int delta);
        int FixDown(int delta);
    };

    class AISectionsResourceType
    {
    public:
        int FixDown(AISectionsData* pData, int* pDelta) { return pData->FixDown(*pDelta); }
        int FixUp(AISectionsData* pData, int delta)     { return pData->FixUp(delta); }

        void* GetSerialisedResourceDescriptor(void* pOut, const void* pResource);

        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 65537;
    };

    void* AISectionsResourceType::GetSerialisedResourceDescriptor(void* pOut, const void* pResource)
    {
        uintptr_t out = reinterpret_cast<uintptr_t>(pOut);
        int liCount = *reinterpret_cast<const int*>(reinterpret_cast<uintptr_t>(pResource) + 60);

        *reinterpret_cast<int*>(out + 12) = 1;
        *reinterpret_cast<int*>(out + 20) = 1;
        *reinterpret_cast<int*>(out + 28) = 1;
        *reinterpret_cast<int*>(out + 36) = 1;

        *reinterpret_cast<int*>(out + 8)  = liCount;
        *reinterpret_cast<int*>(out + 16) = liCount;
        *reinterpret_cast<int*>(out + 24) = liCount;
        *reinterpret_cast<int*>(out + 32) = liCount;

        *reinterpret_cast<int*>(out + 0) = 16;
        *reinterpret_cast<int*>(out + 4) = liCount;

        return pOut;
    }
}
