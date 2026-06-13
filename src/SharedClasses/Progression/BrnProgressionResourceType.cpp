#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnProgression::ProgressionResourceType::FixDown   @ 0x8267F480
//   BrnProgression::ProgressionResourceType::FixUp     @ 0x8267F490
//   BrnProgression::ProgressionResourceType::GetTypeID @ 0x82676B88
//
// FixUp/FixDown forward to BrnProgression::ProgressionData (forward-declared,
// separate TU).

namespace BrnProgression
{
    struct ProgressionData
    {
        int FixUp(int delta);
        int FixDown(int delta);
    };

    class ProgressionResourceType
    {
    public:
        int FixDown(ProgressionData* pData, int* pDelta) { return pData->FixDown(*pDelta); }
        int FixUp(ProgressionData* pData, int* pDelta)   { return pData->FixUp(*pDelta); }
        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 65550;
    };
}
