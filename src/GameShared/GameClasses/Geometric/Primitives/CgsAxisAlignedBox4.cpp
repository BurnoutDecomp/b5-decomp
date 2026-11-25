#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Geometric/Primitives/CgsAxisAlignedBox4.h"

namespace CgsGeometric
{
    AxisAlignedBox AxisAlignedBox4::GetAxisAlignedBox(s32 liBoxNum) const
    {
        CGS_ASSERT_AT(liBoxNum >= 0 && liBoxNum < 4, "liBoxNum >= 0 && liBoxNum < 4",
                      CGS_ARTIST_FILE, 225);

        AxisAlignedBox lBox;
        lBox.mMin = Vector3(mAabbMinX.GetComponent(liBoxNum),
                            mAabbMinY.GetComponent(liBoxNum),
                            mAabbMinZ.GetComponent(liBoxNum));
        lBox.mMax = Vector3(mAabbMaxX.GetComponent(liBoxNum),
                            mAabbMaxY.GetComponent(liBoxNum),
                            mAabbMaxZ.GetComponent(liBoxNum));
        return lBox;
    }
}

#undef CGS_ARTIST_FILE
