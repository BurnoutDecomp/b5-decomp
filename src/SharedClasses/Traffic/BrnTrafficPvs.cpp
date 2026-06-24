// =============================================================================
// BrnTrafficPvs.cpp  (owning .cpp for BrnTraffic::Pvs)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Bodies the two attested standalone
// accessors of the traffic PVS grid:
//   BrnTraffic::Pvs::GetHullIndexForPoint  @ 0x82208090
//   BrnTraffic::Pvs::GetHullPvs            @ 0x82705FB0
// Layout in BrnTrafficPvs.h.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficPvs.h"

namespace BrnTraffic
{

// X360 @ 0x82208090. The point arrives in a VMX register; the grid offset is a
// vector subtract + componentwise multiply by the reciprocal cell size, then the
// X and Z lanes are truncated to int (fctiwz) and clamped into the grid. Done as
// scalar float math for host parity -- only the .x and .z lanes are consumed (the
// stack spill reads the +0 and +8 components, i.e. x and z).
u32 Pvs::GetHullIndexForPoint(Vector3 lPoint) const
{
    const f32 lfScaledX = (lPoint.x - mGridMin.x) * mRecipCellSize.x;
    const f32 lfScaledZ = (lPoint.z - mGridMin.z) * mRecipCellSize.z;

    s32 liX = static_cast<s32>(lfScaledX);
    if (liX <= 0)
    {
        liX = 0;
    }
    if (liX >= static_cast<s32>(muNumCells_X) - 1)
    {
        liX = static_cast<s32>(muNumCells_X) - 1;
    }

    s32 liZ = static_cast<s32>(lfScaledZ);
    if (liZ <= 0)
    {
        liZ = 0;
    }
    if (liZ >= static_cast<s32>(muNumCells_Z) - 1)
    {
        liZ = static_cast<s32>(muNumCells_Z) - 1;
    }

    const u32 luIndex = muNumCells_X * static_cast<u32>(liZ) + static_cast<u32>(liX);

    CGS_ASSERT(luIndex < muNumCells, "luIndex < muNumCells");

    return luIndex;
}

// X360 @ 0x82705FB0. Bounds-asserts the cell index against muNumCells, then returns
// &mpaHullPvs[luIndex] (20-byte stride == sizeof(Set<uint16_t,8u>)).
const Set<u16, 8>& Pvs::GetHullPvs(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muNumCells, "luIndex < muNumCells");

    return mpaHullPvs[luIndex];
}

}
