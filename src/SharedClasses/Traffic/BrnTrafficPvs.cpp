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
#include <cstdint>   // uintptr_t (relocation arithmetic)

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

// ----------------------------------------------------------------------------
// Pvs::GetHullIndexForPoint(Vector3, s32&, s32&)  @ 0x827106B8   (DWARF BrnTrafficPvs.h:64)
//
// ADDITIVE 2026-08-21 (wave T1 round 2, cluster R2A). The rectangle-walk form: the same
// point->cell mapping as the one-argument overload, plus the two clamped grid coordinates
// handed back so the caller can enumerate a BOX of cells. It is the single missing piece
// that round 1 recorded as "the unnamed Pvs helper at 0x827106B8"; the DWARF names it.
//
// Instruction for instruction (0x827106C8..0x82710768):
//     lvx128    v0, r0, r3          ; mGridMin        (this + 0x00)
//     vsubfp    v0, v1, v0          ; lPoint - mGridMin   (the point arrives in v1)
//     lvx128    v13, r3, 0x20       ; mRecipCellSize  (this + 0x20)
//     vmulfp128 v0, v0, v13
//     stvx128   v0, r0, sp          ; spill, then read lanes X and Z only
//     lfs/fctiwz/stfiwx -> *r4      ; liX = (s32)scaled.x
//     lfs/fctiwz/stfiwx -> *r5      ; liZ = (s32)scaled.z
//     clamp *r4 into [0, muNumCells_X - 1]   with a WRITE-BACK after each step
//     clamp *r5 into [0, muNumCells_Z - 1]   with a WRITE-BACK after each step
//     r31 = muNumCells_X * (*r5) + (*r4)
//     assert(r31 < muNumCells)      ; BrnTrafficPvs.h:171   (li r5, 0xAB)
//     return r31
//
// ⚠️ THE WRITE-BACKS ARE LOAD-BEARING and are the whole reason this overload exists: the
// console re-loads `*r4` / `*r5` after each clamp (`stw r11,0(r4)` then `lwz`), so what the
// caller receives is the CLAMPED pair. UpdateRaceCarHulls walks
// [minX..maxX] x [minZ..maxZ] with those values; without the clamp write-back a sim box
// whose corner fell off the map would walk cells that do not exist.
//
// Only lanes X and Z are consumed -- exactly as in the one-argument overload -- so this is
// written as scalar float math for host parity, matching that body's style.
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
// Pvs::GetHullIndexForIndices(s32, s32)  (DWARF BrnTrafficPvs.h:70)
//
// ADDITIVE 2026-08-21 (wave T1 round 2, cluster R2A). NO standalone X360 symbol -- every
// caller inlines it, so this is an inlining reversal recovered from the one call site this
// tree needs: TrafficEntityModule::UpdateRaceCarHulls' rectangle walk at
// 0x827216A4..0x827216D4 (mullw muNumCells_X * liZ, add liX, compare against muNumCells,
// FireAssert "luIndex < muNumCells" at BrnTrafficPvs.h:188).
//
// NOTE the absent clamp: the console does not re-clamp here. Both callers have already been
// handed clamped coordinates by the :64 overload above, and the assert is what catches a
// caller that has not.
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
//
// The grid holds exactly ONE serialised pointer (mpaHullPvs). The console guards that
// `this` is 16-byte aligned (BrnTrafficPvs.cpp:45 -- the three leading Vector3 lanes are
// loaded with aligned VMX loads), then rebases the slot, then walks every cell asserting
// its Set was Constructed (CgsSet.h:430 on the FixUp side, :453 on the FixDown side).
// FixDown runs the walk BEFORE un-rebasing, since the walk dereferences the pointer.
// -----------------------------------------------------------------------------
void Pvs::FixUp(const void* lpBaseData)
{
    CGS_ASSERT((reinterpret_cast<uintptr_t>(this) & 0xFu) == 0u, "Pvs is not 16-byte aligned");

    mpaHullPvs = reinterpret_cast<Set<u16, 8>*>(
        reinterpret_cast<uintptr_t>(mpaHullPvs) + reinterpret_cast<uintptr_t>(lpBaseData));

    for (u32 luCell = 0; luCell < muNumCells; ++luCell)
    {
        // NOTE the local typedef: `Set<u16, 8>` inside a macro argument would split on its
        // comma, so the set type is named once here (CgsSet's own asserts do the same).
        typedef Set<u16, 8> HullPvsSet;
        CGS_ASSERT(mpaHullPvs[luCell].GetLength() != HullPvsSet::KU_INVALID,
                   "Set used before Construct/Clear was called");
    }
}

void Pvs::FixDown(const void* lpBaseData)
{
    for (u32 luCell = 0; luCell < muNumCells; ++luCell)
    {
        // NOTE the local typedef: `Set<u16, 8>` inside a macro argument would split on its
        // comma, so the set type is named once here (CgsSet's own asserts do the same).
        typedef Set<u16, 8> HullPvsSet;
        CGS_ASSERT(mpaHullPvs[luCell].GetLength() != HullPvsSet::KU_INVALID,
                   "Set used before Construct/Clear was called");
    }

    mpaHullPvs = reinterpret_cast<Set<u16, 8>*>(
        reinterpret_cast<uintptr_t>(mpaHullPvs) - reinterpret_cast<uintptr_t>(lpBaseData));
}

}
