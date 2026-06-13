#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTrigger::TriggerResourceType::FixDown   @ 0x826800C8
//   BrnTrigger::TriggerResourceType::FixUp     @ 0x826800D8
//   BrnTrigger::TriggerResourceType::GetTypeID @ 0x...
//
// FixUp/FixDown forward to BrnTrigger::TriggerData (relocate the data by the delta
// read through the third argument). TriggerData is forward-declared (separate TU).

namespace BrnTrigger
{
    struct TriggerData
    {
        int FixUp(int delta);
        int FixDown(int delta);
    };

    class TriggerResourceType
    {
    public:
        int FixDown(TriggerData* pData, int* pDelta) { return pData->FixDown(*pDelta); }
        int FixUp(TriggerData* pData, int* pDelta)   { return pData->FixUp(*pDelta); }
        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 65539;
    };
}
