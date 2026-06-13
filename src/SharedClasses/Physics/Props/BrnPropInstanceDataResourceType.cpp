#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnPhysics::Props::PropInstanceDataResourceType::FixUp     @ 0x8267F7C8
//   BrnPhysics::Props::PropInstanceDataResourceType::GetTypeID @ 0x82675638
//
// FixUp rebases two pointer fields (offsets 8 and 0) by the relocation delta,
// skipping either field when it is null.

namespace BrnPhysics
{
    namespace Props
    {
        class PropInstanceDataResourceType
        {
        public:
            void* FixUp(void* pResource, int* pDelta);
            int   GetTypeID() { return KI_TYPE_ID; }

        private:
            static const int KI_TYPE_ID = 65553;
        };

        void* PropInstanceDataResourceType::FixUp(void* pResource, int* pDelta)
        {
            int delta = *pDelta;
            uintptr_t base = reinterpret_cast<uintptr_t>(pResource);

            uintptr_t& rField8 = *reinterpret_cast<uintptr_t*>(base + 8);
            if (rField8)
                rField8 += delta;

            uintptr_t& rField0 = *reinterpret_cast<uintptr_t*>(base + 0);
            if (rField0)
                rField0 += delta;

            return pResource;
        }
    }
}
