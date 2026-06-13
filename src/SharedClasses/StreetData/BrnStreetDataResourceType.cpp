#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnStreetData::StreetDataResourceType::FixDown   @ 0x8267F0B8
//   BrnStreetData::StreetDataResourceType::FixUp     @ 0x8267F0C8
//   BrnStreetData::StreetDataResourceType::GetTypeID @ 0x82676798
//
// FixUp/FixDown forward to BrnStreetData::StreetData (relocate by the delta read
// through the third argument). StreetData is forward-declared (separate TU).

namespace BrnStreetData
{
    struct StreetData
    {
        int FixUp(int delta);
        int FixDown(int delta);
    };

    class StreetDataResourceType
    {
    public:
        int FixDown(StreetData* pData, int* pDelta) { return pData->FixDown(*pDelta); }
        int FixUp(StreetData* pData, int* pDelta)   { return pData->FixUp(*pDelta); }
        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 65560;
    };
}
