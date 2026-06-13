#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::MaterialStateResourceType::FixDown   @ 0x828A8A80
//   CgsResource::MaterialStateResourceType::FixUp     @ 0x828A8AB8
//   CgsResource::MaterialStateResourceType::GetTypeID @ 0x828A8108
//
// FixUp rebases three pointer fields (offsets 0, 4, 8) by the delta. FixDown
// reverses that and additionally flags the pointed-to material-state object (read
// from offset 8 *before* it is un-rebased) as needing a rebuild (its byte at +40).

namespace CgsResource
{
    class MaterialStateResourceType
    {
    public:
        void* FixDown(void* pResource, int* pDelta);
        void* FixUp(void* pResource, int* pDelta);
        int   GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 15;
    };

    void* MaterialStateResourceType::FixDown(void* pResource, int* pDelta)
    {
        int delta = *pDelta;
        uintptr_t base = reinterpret_cast<uintptr_t>(pResource);

        uintptr_t pMaterialState = *reinterpret_cast<uintptr_t*>(base + 8);

        *reinterpret_cast<uintptr_t*>(base + 0) -= delta;
        *reinterpret_cast<uintptr_t*>(base + 4) -= delta;
        *reinterpret_cast<u8*>(pMaterialState + 40) = 1;
        *reinterpret_cast<uintptr_t*>(base + 8) -= delta;

        return pResource;
    }

    void* MaterialStateResourceType::FixUp(void* pResource, int* pDelta)
    {
        int delta = *pDelta;
        uintptr_t base = reinterpret_cast<uintptr_t>(pResource);

        *reinterpret_cast<uintptr_t*>(base + 0) += delta;
        *reinterpret_cast<uintptr_t*>(base + 4) += delta;
        *reinterpret_cast<uintptr_t*>(base + 8) += delta;

        return pResource;
    }
}
