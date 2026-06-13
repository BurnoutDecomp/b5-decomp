#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSound::Vehicles::Engines::LoopModelResourceType::FixDown   @ 0x826801E0
//   BrnSound::Vehicles::Engines::LoopModelResourceType::FixUp     @ 0x826801D0
//   BrnSound::Vehicles::Engines::LoopModelResourceType::GetTypeID @ 0x82675570
//
// FixUp/FixDown forward to LoopModelData (forward-declared, separate TU). FixDown
// passes the relocation delta by pointer; FixUp by value, matching the X360 calls.

namespace BrnSound
{
    namespace Vehicles
    {
        namespace Engines
        {
            struct LoopModelData
            {
                int FixUp(int delta);
                int FixDown(int delta);
            };

            class LoopModelResourceType
            {
            public:
                int FixDown(LoopModelData* pData, int* pDelta) { return pData->FixDown(*pDelta); }
                int FixUp(LoopModelData* pData, int delta)     { return pData->FixUp(delta); }
                int GetTypeID() { return KI_TYPE_ID; }

            private:
                static const int KI_TYPE_ID = 0x10000;
            };
        }
    }
}
