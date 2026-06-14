#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::StreamedDeformationSpecResourceType::FixUp     @ 0x826776F0
//   BrnResource::StreamedDeformationSpecResourceType::GetTypeID @ 0x82675608
//   BrnResource::StreamedDeformationSpecResourceType::Serialise @ 0x82677680
//
// Thin resource-type wrapper that forwards relocation to BrnPhysics::Deformation::
// StreamedDeformationSpec. Serialise makes the source position-independent (FixDown), copies
// the whole streamed block to the destination, rebases the copy to its new address, then
// rebases the source back. The block size is the spec header's array (80-byte entries) end.
// StreamedDeformationSpec lives in a separate TU (forward-declared).

namespace BrnPhysics
{
namespace Deformation
{
    struct StreamedDeformationSpec
    {
        void* FixDown();
        void* FixUp(int liDelta);

        u8  mPad0[52];
        int miEntryCount;   // +52
        u32 muDataEnd;      // +56  absolute end address of the streamed block
    };
}
}

namespace BrnResource
{
    class StreamedDeformationSpecResourceType
    {
        typedef BrnPhysics::Deformation::StreamedDeformationSpec Spec;

    public:
        void* FixUp(void* pResource, const int* pDelta)
        {
            return static_cast<Spec*>(pResource)->FixUp(*pDelta);
        }

        int GetTypeID() { return KI_TYPE_ID; }

        void* Serialise(void* pResource, int* pDestination);

    private:
        static const int KI_TYPE_ID = 65564;
    };

    void* StreamedDeformationSpecResourceType::Serialise(void* pResource, int* pDestination)
    {
        Spec*     lpSource = static_cast<Spec*>(pResource);
        uintptr_t lDest    = static_cast<uintptr_t>(*pDestination);

        uintptr_t lEnd  = 80 * static_cast<u32>(lpSource->miEntryCount) + lpSource->muDataEnd;
        size_t    luLen = lEnd - reinterpret_cast<uintptr_t>(lpSource);

        lpSource->FixDown();
        memcpy(reinterpret_cast<void*>(lDest), lpSource, luLen);
        reinterpret_cast<Spec*>(lDest)->FixUp(static_cast<int>(lDest));
        lpSource->FixUp(static_cast<int>(reinterpret_cast<uintptr_t>(lpSource)));

        return reinterpret_cast<void*>(lDest);
    }
}
