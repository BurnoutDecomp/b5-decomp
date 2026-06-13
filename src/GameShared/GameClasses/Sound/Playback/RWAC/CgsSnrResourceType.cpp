#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::SnrResourceType::FixUp     @ 0x826C2258
//   CgsResource::SnrResourceType::GetTypeID @ 0x82689E10
//
// FixUp rebases a single leading pointer field (offset 0) by the relocation delta.

namespace CgsResource
{
    class SnrResourceType
    {
    public:
        void* FixUp(void* pResource, int* pDelta)
        {
            *reinterpret_cast<uintptr_t*>(pResource) += *pDelta;
            return pResource;
        }

        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 25;
    };
}
