#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::MassiveLookupTableResourceType::FixDown   @ 0x8267F210
//   CgsResource::MassiveLookupTableResourceType::FixUp     @ 0x8267F200
//   CgsResource::MassiveLookupTableResourceType::GetTypeID @ 0x826767A8
//   CgsResource::MassiveLookupTableResourceType::Serialise @ 0x8267F198
//
// Thin resource-type wrapper forwarding relocation to BrnMassive::MassiveLookupTable.
// Serialise relativises the source (FixDown), copies the whole block to the destination,
// rebases the copy to its new address, then rebases the source back. Block size is the
// header's {count<<6} entries plus the trailing data end. MassiveLookupTable is a separate
// TU (forward-declared).

namespace BrnMassive
{
    struct MassiveLookupTable
    {
        void* FixDown(int liDelta = 0);
        void* FixUp(int liDelta);

        u32 muCount;     // +0
        u32 muDataEnd;   // +4  absolute end address of the block
    };
}

namespace CgsResource
{
    class MassiveLookupTableResourceType
    {
        typedef BrnMassive::MassiveLookupTable Table;

    public:
        void* FixDown(void* pResource, const int* pDelta)
        {
            return static_cast<Table*>(pResource)->FixDown(*pDelta);
        }

        void* FixUp(void* pResource, const int* pDelta)
        {
            return static_cast<Table*>(pResource)->FixUp(*pDelta);
        }

        int GetTypeID() { return KI_TYPE_ID; }

        void* Serialise(void* pResource, int* pDestination);

    private:
        static const int KI_TYPE_ID = 65562;
    };

    void* MassiveLookupTableResourceType::Serialise(void* pResource, int* pDestination)
    {
        Table*    lpSource = static_cast<Table*>(pResource);
        uintptr_t lDest    = static_cast<uintptr_t>(*pDestination);

        uintptr_t lEnd  = (static_cast<uintptr_t>(lpSource->muCount) << 6) + lpSource->muDataEnd;
        size_t    luLen = lEnd - reinterpret_cast<uintptr_t>(lpSource);

        lpSource->FixDown();
        memcpy(reinterpret_cast<void*>(lDest), lpSource, luLen);
        reinterpret_cast<Table*>(lDest)->FixUp(static_cast<int>(lDest));
        lpSource->FixUp(static_cast<int>(reinterpret_cast<uintptr_t>(lpSource)));

        return reinterpret_cast<void*>(lDest);
    }
}
