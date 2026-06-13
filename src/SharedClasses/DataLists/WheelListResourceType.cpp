#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::WheelListResourceType::FixUp     @ 0x8267DF28
//   BrnResource::WheelListResourceType::GetTypeID @ 0x826757B8
//
// FixUp rebases the pointer field at offset 4 by the relocation delta.

namespace BrnResource
{
    class WheelListResourceType
    {
    public:
        void* FixUp(void* pResource, int* pDelta)
        {
            *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pResource) + 4) += *pDelta;
            return pResource;
        }

        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 65545;
    };
}
