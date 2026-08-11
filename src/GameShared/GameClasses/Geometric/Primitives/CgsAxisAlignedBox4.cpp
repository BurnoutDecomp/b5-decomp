#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// CgsGeometric::AxisAlignedBox4 -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   GetAxisAlignedBox @ 0x8283AA68
// ============================================================================

namespace CgsGeometric
{
    // ------------------------------------------------------------------------
    // GetAxisAlignedBox @0x8283AA68
    //
    //   if (a3 >= 4) assert("liBoxNum >= 0 && liBoxNum < 4",
    //                       ".../CgsAxisAlignedBox4.h", 225)
    //   r11 = 4 * liBoxNum
    //   lvsl v0, 0, r11 ; vspltw v0, v0, 0     <- a broadcast control for word liBoxNum
    //   min: vperm of [this+0], [this+16], [this+32]
    //   max: vperm of [this+48], [this+64], [this+80]
    //
    // The X360 compares the index UNSIGNED (`if (a3 >= 4)`), so the assert is a
    // single unsigned bound even though the message reads as two signed ones.
    // ------------------------------------------------------------------------
    AxisAlignedBox AxisAlignedBox4::GetAxisAlignedBox(u32 lu32BoxNum) const
    {
        CGS_ASSERT(lu32BoxNum < 4u, "liBoxNum >= 0 && liBoxNum < 4");

        AxisAlignedBox lBox;

        lBox.mMin.x = mafMinX[lu32BoxNum];
        lBox.mMin.y = mafMinY[lu32BoxNum];
        lBox.mMin.z = mafMinZ[lu32BoxNum];
        // The console's vperm/vrlimi merge leaves the w lane holding a copy of the X
        // component; nothing downstream reads it. Reproduced so the 16-byte store is
        // fully defined rather than leaving an indeterminate lane.
        lBox.mMin.w = mafMinX[lu32BoxNum];

        lBox.mMax.x = mafMaxX[lu32BoxNum];
        lBox.mMax.y = mafMaxY[lu32BoxNum];
        lBox.mMax.z = mafMaxZ[lu32BoxNum];
        lBox.mMax.w = mafMaxX[lu32BoxNum];

        return lBox;
    }
}
