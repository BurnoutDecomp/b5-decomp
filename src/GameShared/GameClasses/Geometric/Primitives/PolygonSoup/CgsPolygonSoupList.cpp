#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"

namespace CgsGeometric
{
    void PolygonSoupList::FixUp(const void* lpLoadBase)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpLoadBase);
        mpaPolySoupBoxes = reinterpret_cast<AxisAlignedBox4*>(
            luBase + reinterpret_cast<uintptr_t>(mpaPolySoupBoxes));
        mpapPolySoups = reinterpret_cast<PolygonSoup**>(
            luBase + reinterpret_cast<uintptr_t>(mpapPolySoups));

        for (s32 liSoup = 0; liSoup < miNumPolySoups; ++liSoup)
        {
            mpapPolySoups[liSoup] = reinterpret_cast<PolygonSoup*>(
                luBase + reinterpret_cast<uintptr_t>(mpapPolySoups[liSoup]));
            mpapPolySoups[liSoup]->FixUp(lpLoadBase);
        }
    }
}
