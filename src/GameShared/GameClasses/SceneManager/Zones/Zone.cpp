#include "GameShared/GameClasses/SceneManager/Zones/Zone.h"

// CgsSceneManager::Zone / Neighbour out-of-line member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU homes the X360-emitted functions:
//
//   Neighbour::GetZone() const                 @ 0x828AC430 -> mpZone (Neighbour +0)
//   Neighbour::IsFlagSet(eNeighbourFlags) const @ 0x828AC438 -> flag == (muFlags & flag)
//   Neighbour::FixUp(void*)                    (X360 inlines it; DecFIGS PS3 @ 0xC4D7DC, home Zone.cpp)
//   Zone::FixUp(void*)                         @ 0x828CBA20 -> rebase the zone's pointers up by
//                                                              lpBaseAddress (serialise-in fixup)
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

    // SEAM (platform-4 ZoneList link prerequisite): the resource type now calls the REAL
    // ZoneList::FixUp (ZoneList.cpp @0x828D05B8), whose per-zone walk calls Zone::FixUp
    // @0x828CBA20 -- declared here as homed by Zone.cpp but never bodied. The ARTIST export
    // set has no JSON for 0x828CBA20 (gap between 0x828CB9A0 and 0x828CBAE8), so the body is
    // reconstructed from the DecFIGS PS3 build (Zone::FixUp @0xC4D820, DWARF home_file
    // Zone.cpp), which is the source-shaped form: rebase mpPoints, then for each neighbour
    // array rebase the array pointer FIRST and delegate each element to Neighbour::FixUp
    // (the X360 compiler inlines that call in FixDown; the source calls it).

    // DecFIGS PS3 0xC4D7DC (X360: inlined). Rebase the neighbour's mpZone, only if non-null.
    void Neighbour::FixUp(void* lpBaseAddress)
    {
        if (mpZone != 0)
        {
            mpZone = reinterpret_cast<Zone*>(
                reinterpret_cast<usize>(mpZone) + reinterpret_cast<usize>(lpBaseAddress));
        }
    }

    // X360 0x828CBA20 (behaviour from DecFIGS PS3 0xC4D820). Rebase the zone's base-relative
    // pointers to absolute (serialise-in). Unlike FixDown there is no mpPoints assert, the
    // neighbour-array pointers are rebased BEFORE the walk, and each element goes through
    // Neighbour::FixUp.
    void Zone::FixUp(void* lpBaseAddress)
    {
        const usize luBase = reinterpret_cast<usize>(lpBaseAddress);

        mpPoints = reinterpret_cast<rw::math::vpu::Vector2*>(
            reinterpret_cast<usize>(mpPoints) + luBase);

        if (mpSafeNeighbours != 0)
        {
            mpSafeNeighbours = reinterpret_cast<Neighbour*>(
                reinterpret_cast<usize>(mpSafeNeighbours) + luBase);
            for (s16 liIndex = 0; liIndex < miNumSafeNeighbours; ++liIndex)
            {
                mpSafeNeighbours[liIndex].FixUp(lpBaseAddress);
            }
        }

        if (mpUnsafeNeighbours != 0)
        {
            mpUnsafeNeighbours = reinterpret_cast<Neighbour*>(
                reinterpret_cast<usize>(mpUnsafeNeighbours) + luBase);
            for (s16 liIndex = 0; liIndex < miNumUnsafeNeighbours; ++liIndex)
            {
                mpUnsafeNeighbours[liIndex].FixUp(lpBaseAddress);
            }
        }
    }

    // X360 0x828CBAE8. Rebase the zone's absolute pointers to base-relative offsets.
    void Zone::FixDown(void* lpBaseAddress)
    {
        CGS_ASSERT(mpPoints != 0,
                   "Points have not yet been specified for zone so can't fix it down!\n");

        const usize luBase = reinterpret_cast<usize>(lpBaseAddress);

        // Capture the pre-fixup neighbour-array pointers (the X360 reads mpSafeNeighbours into v6
        // before rebasing mpPoints, and only rebases the arrays when the captured pointer is set).
        Neighbour* lpSafeNeighbours   = mpSafeNeighbours;
        Neighbour* lpUnsafeNeighbours = mpUnsafeNeighbours;

        mpPoints = reinterpret_cast<rw::math::vpu::Vector2*>(
            reinterpret_cast<usize>(mpPoints) - luBase);

        if (lpSafeNeighbours != 0)
        {
            for (s16 liIndex = 0; liIndex < miNumSafeNeighbours; ++liIndex)
            {
                Zone* lpZone = lpSafeNeighbours[liIndex].mpZone;
                if (lpZone != 0)
                {
                    lpSafeNeighbours[liIndex].mpZone =
                        reinterpret_cast<Zone*>(reinterpret_cast<usize>(lpZone) - luBase);
                }
            }
            mpSafeNeighbours = reinterpret_cast<Neighbour*>(
                reinterpret_cast<usize>(mpSafeNeighbours) - luBase);
        }

        if (lpUnsafeNeighbours != 0)
        {
            for (s16 liIndex = 0; liIndex < miNumUnsafeNeighbours; ++liIndex)
            {
                Zone* lpZone = lpUnsafeNeighbours[liIndex].mpZone;
                if (lpZone != 0)
                {
                    lpUnsafeNeighbours[liIndex].mpZone =
                        reinterpret_cast<Zone*>(reinterpret_cast<usize>(lpZone) - luBase);
                }
            }
            mpUnsafeNeighbours = reinterpret_cast<Neighbour*>(
                reinterpret_cast<usize>(mpUnsafeNeighbours) - luBase);
        }
    }

    // ARTIST 822BACE8: full-register accumulation and two reciprocal refinements.
    rw::math::vpu::Vector2 Zone::CalculateCentre() const
    {
        if (miNumPoints == 4)
            return CalculateCentre4();
        __vector4 lvCentre = __vspltisw(0);
        for (s16 liIndex = 0; liIndex < miNumPoints; ++liIndex)
            lvCentre = __vaddfp(lvCentre, mpPoints[liIndex].mV);
        const f32 lfCount = static_cast<f32>(miNumPoints);
        CGS_ALIGN(16) const f32 lafCount[4] = { lfCount, lfCount, lfCount, lfCount };
        const __vector4 lvCount = __lvx(lafCount, 0);
        const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
        __vector4 lvReciprocal = __vrefp(lvCount);
        __vector4 lvError = __vnmsubfp(lvReciprocal, lvCount, lvOne);
        lvReciprocal = __vmaddfp(lvReciprocal, lvError, lvReciprocal);
        lvError = __vnmsubfp(lvReciprocal, lvCount, lvOne);
        lvReciprocal = __vmaddfp(lvReciprocal, lvError, lvReciprocal);
        rw::math::vpu::Vector2 lResult;
        lResult.mV = __vmulfp(lvCentre, lvReciprocal);
        return lResult;
    }

    // ARTIST 822A3CA0: retain initial +zero and the four ordered vector adds.
    rw::math::vpu::Vector2 Zone::CalculateCentre4() const
    {
        CGS_ASSERT(miNumPoints == 4,
                   "This optimised calc centre function only works for 4 point units\n");
        __vector4 lvCentre = __vspltisw(0);
        for (u32 i = 0; i < 4; ++i)
            lvCentre = __vaddfp(lvCentre, mpPoints[i].mV);
        const __vector4 lvHalfNegative = __vcfsx(__vspltisw(-1), 1);
        const __vector4 lvQuarter = __vmulfp(lvHalfNegative, lvHalfNegative);
        rw::math::vpu::Vector2 lResult;
        lResult.mV = __vmulfp(lvCentre, lvQuarter);
        return lResult;
    }

}
