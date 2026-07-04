#include "GameShared/GameClasses/SceneManager/Zones/Zone.h"

// CgsSceneManager::Zone / Neighbour out-of-line member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU homes the three X360-emitted functions:
//
//   Neighbour::GetZone() const                 @ 0x828AC430 -> mpZone (Neighbour +0)
//   Neighbour::IsFlagSet(eNeighbourFlags) const @ 0x828AC438 -> flag == (muFlags & flag)
//   Zone::FixDown(void*)                       @ 0x828CBAE8 -> rebase the zone's pointers down by
//                                                              lpBaseAddress (serialise-out fixup)
//
// X360 store-for-store notes:
//   - GetZone: `lwz r3,0(r3); blr` -> returns mpZone (the neighbouring Zone* at Neighbour +0).
//   - IsFlagSet: `lwz r11,4(r3); and r11,r11,r4; subf; cntlzw; extrwi r3,r11,1,26` computes the
//     boolean `(muFlags & flag) == flag` -- i.e. ALL bits of the flag mask are set (NOT a !=0
//     any-bit test). muFlags is Neighbour +4.
//   - FixDown: the serialise-OUT pointer fixup (the inverse of FixUp). Asserts mpPoints != 0
//     ("Points have not yet been specified for zone so can't fix it down!\n"), then subtracts the
//     base address from mpPoints. If the (pre-fixup) mpSafeNeighbours pointer is non-null, it walks
//     the safe-neighbour array (stride 16 == sizeof(Neighbour), miNumSafeNeighbours @ +0x1C) and
//     rebases each element's mpZone (only when non-null), then rebases mpSafeNeighbours itself.
//     The same is done for mpUnsafeNeighbours (miNumUnsafeNeighbours @ +0x1E). All subtractions are
//     32-bit `subf` of the byte base address; modelled on the host with usize pointer arithmetic.
//   - The X360 d:\p4 path/line cite (164) is discarded per project policy; CGS_ASSERT supplies it.

namespace CgsSceneManager
{
    // X360 0x828AC430.
    const Zone* Neighbour::GetZone() const
    {
        return mpZone;
    }

    // X360 0x828AC438. True iff every bit of the flag mask is set in muFlags.
    u8 Neighbour::IsFlagSet(eNeighbourFlags lxNeighbourFlag) const
    {
        const u32 luFlag = static_cast<u32>(lxNeighbourFlag);
        return (muFlags & luFlag) == luFlag;
    }

    // X360 0x828CBAE8. Rebase the zone's absolute pointers to base-relative offsets.
    void Zone::FixDown(void* lpBaseAddress)
    {
        CGS_ASSERT(mpPoints != nullptr,
                   "Points have not yet been specified for zone so can't fix it down!\n");

        const usize luBase = reinterpret_cast<usize>(lpBaseAddress);

        // Capture the pre-fixup neighbour-array pointers (the X360 reads mpSafeNeighbours into v6
        // before rebasing mpPoints, and only rebases the arrays when the captured pointer is set).
        Neighbour* lpSafeNeighbours   = mpSafeNeighbours;
        Neighbour* lpUnsafeNeighbours = mpUnsafeNeighbours;

        mpPoints = reinterpret_cast<rw::math::vpu::Vector2*>(
            reinterpret_cast<usize>(mpPoints) - luBase);

        if (lpSafeNeighbours != nullptr)
        {
            for (s16 liIndex = 0; liIndex < miNumSafeNeighbours; ++liIndex)
            {
                Zone* lpZone = lpSafeNeighbours[liIndex].mpZone;
                if (lpZone != nullptr)
                {
                    lpSafeNeighbours[liIndex].mpZone =
                        reinterpret_cast<Zone*>(reinterpret_cast<usize>(lpZone) - luBase);
                }
            }
            mpSafeNeighbours = reinterpret_cast<Neighbour*>(
                reinterpret_cast<usize>(mpSafeNeighbours) - luBase);
        }

        if (lpUnsafeNeighbours != nullptr)
        {
            for (s16 liIndex = 0; liIndex < miNumUnsafeNeighbours; ++liIndex)
            {
                Zone* lpZone = lpUnsafeNeighbours[liIndex].mpZone;
                if (lpZone != nullptr)
                {
                    lpUnsafeNeighbours[liIndex].mpZone =
                        reinterpret_cast<Zone*>(reinterpret_cast<usize>(lpZone) - luBase);
                }
            }
            mpUnsafeNeighbours = reinterpret_cast<Neighbour*>(
                reinterpret_cast<usize>(mpUnsafeNeighbours) - luBase);
        }
    }

    // X360 0x822BACE8. Centroid of the zone polygon: the average of all corner points.
    // The X360 build hand-vectorises this over VMX -- it sums the full 16-byte point vectors
    // (all four lanes) into an accumulator, then multiplies by a Newton-refined reciprocal of
    // the point count (1/miNumPoints). For the exactly-4-point case it tail-calls the branch-free
    // CalculateCentre4. This is the semantic-parity scalar reconstruction: the same per-lane
    // sum-then-scale on the named Vector2 lanes, not the VMX reciprocal-refinement permute.
    rw::math::vpu::Vector2 Zone::CalculateCentre() const
    {
        if (miNumPoints == 4)
        {
            return CalculateCentre4();
        }

        rw::math::vpu::Vector2 lCentre;
        lCentre.SetZero();

        for (s16 liIndex = 0; liIndex < miNumPoints; ++liIndex)
        {
            lCentre.x += mpPoints[liIndex].x;
            lCentre.y += mpPoints[liIndex].y;
            lCentre.z += mpPoints[liIndex].z;
            lCentre.w += mpPoints[liIndex].w;
        }

        const f32 lfInvCount = 1.0f / static_cast<f32>(miNumPoints);
        lCentre.x *= lfInvCount;
        lCentre.y *= lfInvCount;
        lCentre.z *= lfInvCount;
        lCentre.w *= lfInvCount;

        return lCentre;
    }

    // X360 0x822A3CA0. Branch-free centroid for the 4-point case: (p0+p1+p2+p3) * 0.25.
    // Asserts the zone has exactly 4 points. The X360 forms 1/4 as (-0.5)^2 rather than a literal
    // and sums the four full 16-byte point vectors (all lanes). Semantic-parity scalar form.
    rw::math::vpu::Vector2 Zone::CalculateCentre4() const
    {
        CGS_ASSERT(miNumPoints == 4,
                   "This optimised calc centre function only works for 4 point units\n");

        rw::math::vpu::Vector2 lCentre;
        lCentre.x = (mpPoints[0].x + mpPoints[1].x + mpPoints[2].x + mpPoints[3].x) * 0.25f;
        lCentre.y = (mpPoints[0].y + mpPoints[1].y + mpPoints[2].y + mpPoints[3].y) * 0.25f;
        lCentre.z = (mpPoints[0].z + mpPoints[1].z + mpPoints[2].z + mpPoints[3].z) * 0.25f;
        lCentre.w = (mpPoints[0].w + mpPoints[1].w + mpPoints[2].w + mpPoints[3].w) * 0.25f;

        return lCentre;
    }
}
