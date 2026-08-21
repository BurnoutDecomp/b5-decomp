// =============================================================================
// BrnTrafficPvs.cpp -- owning .cpp for BrnTraffic::Pvs. Layout in BrnTrafficPvs.h.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::Pvs::GetHullIndexForPoint(Vector3)            @ 0x82208090
//   BrnTraffic::Pvs::GetHullIndexForPoint(Vector3, s32&, s32&) @ 0x827106B8
//   BrnTraffic::Pvs::GetHullPvs                               @ 0x82705FB0
//   BrnTraffic::Pvs::FixUp / FixDown            @ 0x827623E8 / @ 0x827624A0
// GetHullIndexForIndices has no standalone symbol; see its body below.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficPvs.h"
#include <cstdint>   // uintptr_t (relocation arithmetic)

namespace BrnTraffic
{

// X360 @ 0x82208090. The point arrives in a VMX register; the grid offset is a
// vector subtract and a componentwise multiply by the reciprocal cell size, then
// the X and Z lanes are truncated to int (fctiwz) and clamped into the grid.
// Written as scalar float math for host parity: only .x and .z are consumed.
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

// ----------------------------------------------------------------------------
// Pvs::GetHullIndexForPoint(Vector3, s32&, s32&) @ 0x827106B8 (DWARF BrnTrafficPvs.h:64).
// The rectangle-walk form: the same point-to-cell mapping as the one-argument overload,
// plus the two clamped grid coordinates handed back so the caller can enumerate a box of
// cells. Assert baked at BrnTrafficPvs.h:171.
//
// THE WRITE-BACKS ARE LOAD-BEARING. The console stores each clamp back through the
// reference (`stw r11,0(r4)` then re-loads), so the caller receives the CLAMPED pair.
// UpdateRaceCarHulls walks [minX..maxX] x [minZ..maxZ] with those values; without the
// write-back a sim box whose corner fell off the map would walk cells that do not exist.
// ----------------------------------------------------------------------------
u32 Pvs::GetHullIndexForPoint(Vector3 lPoint, s32& lriOutX, s32& lriOutZ) const
{
    const f32 lfScaledX = (lPoint.x - mGridMin.x) * mRecipCellSize.x;
    const f32 lfScaledZ = (lPoint.z - mGridMin.z) * mRecipCellSize.z;

    lriOutX = static_cast<s32>(lfScaledX);
    if (lriOutX <= 0)
    {
        lriOutX = 0;
    }
    if (lriOutX >= static_cast<s32>(muNumCells_X) - 1)
    {
        lriOutX = static_cast<s32>(muNumCells_X) - 1;
    }

    lriOutZ = static_cast<s32>(lfScaledZ);
    if (lriOutZ <= 0)
    {
        lriOutZ = 0;
    }
    if (lriOutZ >= static_cast<s32>(muNumCells_Z) - 1)
    {
        lriOutZ = static_cast<s32>(muNumCells_Z) - 1;
    }

    const u32 luIndex = muNumCells_X * static_cast<u32>(lriOutZ) + static_cast<u32>(lriOutX);

    CGS_ASSERT(luIndex < muNumCells, "luIndex < muNumCells");

    return luIndex;
}

// ----------------------------------------------------------------------------
// Pvs::GetHullIndexForIndices(s32, s32) (DWARF BrnTrafficPvs.h:70). NO standalone X360
// symbol -- every caller inlines it, so this is an inlining reversal recovered from
// TrafficEntityModule::UpdateRaceCarHulls' rectangle walk at 0x827216A4..0x827216D4.
// Assert baked at BrnTrafficPvs.h:188.
//
// No clamp here, matching the console: callers arrive with coordinates the :64 overload
// already clamped, and the assert catches one that has not.
// ----------------------------------------------------------------------------
u32 Pvs::GetHullIndexForIndices(s32 liX, s32 liZ) const
{
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

// -----------------------------------------------------------------------------
// Pvs::FixUp @0x827623E8 / Pvs::FixDown @0x827624A0 -- load-time pointer relocation.
// The grid holds exactly one serialised pointer (mpaHullPvs). The console guards that
// `this` is 16-byte aligned (the three leading Vector3 lanes use aligned VMX loads),
// rebases the slot, then walks every cell asserting its Set was Constructed. FixDown
// runs the walk BEFORE un-rebasing, since the walk dereferences the pointer.
// Asserts baked at BrnTrafficPvs.cpp:45 and CgsSet.h:430 (FixUp) / :453 (FixDown).
// -----------------------------------------------------------------------------
void Pvs::FixUp(const void* lpBaseData)
{
    CGS_ASSERT((reinterpret_cast<uintptr_t>(this) & 0xFu) == 0u, "Pvs is not 16-byte aligned");

    mpaHullPvs = reinterpret_cast<Set<u16, 8>*>(
        reinterpret_cast<uintptr_t>(mpaHullPvs) + reinterpret_cast<uintptr_t>(lpBaseData));

    for (u32 luCell = 0; luCell < muNumCells; ++luCell)
    {
        // Local typedef because `Set<u16, 8>` inside a macro argument splits on its comma.
        typedef Set<u16, 8> HullPvsSet;
        CGS_ASSERT(mpaHullPvs[luCell].GetLength() != HullPvsSet::KU_INVALID,
                   "Set used before Construct/Clear was called");
    }
}

void Pvs::FixDown(const void* lpBaseData)
{
    for (u32 luCell = 0; luCell < muNumCells; ++luCell)
    {
        // Local typedef because `Set<u16, 8>` inside a macro argument splits on its comma.
        typedef Set<u16, 8> HullPvsSet;
        CGS_ASSERT(mpaHullPvs[luCell].GetLength() != HullPvsSet::KU_INVALID,
                   "Set used before Construct/Clear was called");
    }

    mpaHullPvs = reinterpret_cast<Set<u16, 8>*>(
        reinterpret_cast<uintptr_t>(mpaHullPvs) - reinterpret_cast<uintptr_t>(lpBaseData));
}

}
