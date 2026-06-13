#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::PolygonSoupListResourceType::FixUp     @ 0x82845EB0
//   CgsResource::PolygonSoupListResourceType::GetTypeID @ 0x82839FA0
//
// FixUp simply forwards to CgsGeometric::PolygonSoupList::FixUp (reconstructed in
// CgsPolygonSoupList.cpp), passing the list (a2) and the relocation delta loaded
// from a3. The resource-type `this` is unused. PolygonSoupList is forward-declared
// here (it lives in another TU and has no shared header yet).

namespace CgsGeometric
{
    struct PolygonSoupList
    {
        PolygonSoupList* FixUp(int delta);
    };
}

namespace CgsResource
{
    class PolygonSoupListResourceType
    {
    public:
        CgsGeometric::PolygonSoupList* FixUp(CgsGeometric::PolygonSoupList* pList, u32* pDelta)
        {
            return pList->FixUp(static_cast<int>(*pDelta));
        }

        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 67;
    };
}
