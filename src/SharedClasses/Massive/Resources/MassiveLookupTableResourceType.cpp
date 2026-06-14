#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::MassiveLookupTableResourceType::FixDown   @ 0x8267F210
//   CgsResource::MassiveLookupTableResourceType::FixUp     @ 0x8267F200
//   CgsResource::MassiveLookupTableResourceType::GetTypeID @ 0x826767A8
//   CgsResource::MassiveLookupTableResourceType::Serialise @ 0x8267F198
//
// Resource-type handler for a serialised BrnMassive::MassiveLookupTable blob.
// FixDown/FixUp forward to the table's own relocation. Serialise relocates the
// source to file-relative pointers, copies the whole blob, then re-relocates both
// the destination and the source. The blob spans base + field0*64 + field4.

namespace BrnMassive
{
    // Own TU; trap stubs until it lands.
    class MassiveLookupTable
    {
    public:
        static void* FixDown(void* pTable, int liDelta);
        static void* FixUp(void* pTable, int liDelta);
    };

    void* MassiveLookupTable::FixDown(void*, int) { __debugbreak(); return nullptr; }
    void* MassiveLookupTable::FixUp(void*, int)   { __debugbreak(); return nullptr; }
}

namespace CgsResource
{
    class MassiveLookupTableResourceType
    {
    public:
        void* FixDown(void* pResource, const int* pDelta)
        {
            return BrnMassive::MassiveLookupTable::FixDown(pResource, *pDelta);
        }
        void* FixUp(void* pResource, const int* pDelta)
        {
            return BrnMassive::MassiveLookupTable::FixUp(pResource, *pDelta);
        }
        int   GetTypeID() { return KI_TYPE_ID; }
        void* Serialise(void* pResource, void** ppDest);

    private:
        static const int KI_TYPE_ID = 65562;
    };

    void* MassiveLookupTableResourceType::Serialise(void* pResource, void** ppDest)
    {
        uintptr_t lSrc  = reinterpret_cast<uintptr_t>(pResource);
        void*     lpDst = *ppDest;
        usize     luSize = ((*reinterpret_cast<const u32*>(lSrc) << 6)
                            + *reinterpret_cast<const u32*>(lSrc + 4)) - lSrc;

        BrnMassive::MassiveLookupTable::FixDown(pResource, 0);
        std::memcpy(lpDst, pResource, luSize);
        BrnMassive::MassiveLookupTable::FixUp(lpDst, reinterpret_cast<int>(lpDst));
        BrnMassive::MassiveLookupTable::FixUp(pResource, static_cast<int>(lSrc));
        return *ppDest;
    }
}
