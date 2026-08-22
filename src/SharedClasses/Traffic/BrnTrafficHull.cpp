// =============================================================================
// BrnTrafficHull.cpp -- owning .cpp for the BrnTraffic::Hull accessor family.
// The layout and the bounds-checked element accessors are in BrnTrafficHull.h;
// this file forces them out-of-line so the TU has a real object-code presence.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::Hull::GetNeighbour      @ 0x821F5358
//   BrnTraffic::Hull::GetStaticVehicle  @ 0x82705C90
//   BrnTraffic::Hull::GetStopLine       @ 0x82705C20
// GetSection @ 0x821F52E0 stays inline in the header and is not re-emitted here.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficSection.h"   // real Section/LaneRung (must precede Hull.h)
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include <cstdint>                                     // uintptr_t

namespace BrnTraffic
{
    // -------------------------------------------------------------------------
    // Hull::FixUp @0x827620A0 / Hull::FixDown @0x827622E0.
    //
    // Twelve consecutive pointer slots (console +0x10 .. +0x3C) rebased against the
    // resource block base, unconditionally -- an empty per-hull array serialises as a
    // one-past-the-end offset, never as null, so there is nothing to guard.
    //
    // The console walks NO sub-elements: Section, LaneRung, Neighbour, SectionSpan,
    // SectionFlow, StopLine, StaticTrafficVehicle, LightTrigger, LightTriggerStartData
    // and JunctionLogicBox hold no pointers, and Section::muRungOffset /
    // muNeighbourOffset / muStopLineOffset are INDICES into the hull's own tables, so
    // they must not be relocated or widened. That also means nothing here byte-swaps a
    // record, which is correct: the shipped PC payload is already little-endian
    // (BrnTrafficStaticTraffic.cpp). Adding a StaticTrafficVehicle::FixUp call here
    // would corrupt correct data.
    //
    // Asserts, in the asm's order: muNumJunctions < 16 (baked BrnTrafficHull.cpp:143),
    // then the two 16-byte rung-table alignment guards (cpp:146 / :147). FixDown checks
    // the same two on the offset form (cpp:259 / :260), same slot order.
    // -------------------------------------------------------------------------
    namespace
    {
        template <typename T>
        inline void Rebase(T*& lrpPointer, uintptr_t luBase)
        {
            lrpPointer = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lrpPointer) + luBase);
        }

        template <typename T>
        inline void UnBase(T*& lrpPointer, uintptr_t luBase)
        {
            lrpPointer = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lrpPointer) - luBase);
        }

        inline bool Is16Aligned(const void* lpAddress)
        {
            return (reinterpret_cast<uintptr_t>(lpAddress) & 0xFu) == 0u;
        }
    }

    // KU_MAX_JUNCTIONS_PER_HULL comes from its canonical home,
    // BrnTrafficSharedConstants.h, reached through BrnTrafficHull.h -> BrnTrafficSection.h.

    void Hull::FixUp(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        Rebase(mpaSections, luBase);
        Rebase(mpaRungs, luBase);
        Rebase(mpafCumulativeRungLengths, luBase);
        Rebase(mpaNeighbourData, luBase);
        Rebase(mpaSectionSpans, luBase);
        Rebase(mpaStaticTrafficVehicles, luBase);
        Rebase(mpaSectionFlows, luBase);
        Rebase(mpaJunctions, luBase);
        Rebase(mpaStopLines, luBase);
        Rebase(mpaLightTriggers, luBase);
        Rebase(mpaLightTriggerStartData, luBase);
        Rebase(mpaLightTriggerJunctionLookup, luBase);

        CGS_ASSERT(muNumJunctions < KU_MAX_JUNCTIONS_PER_HULL, "Hull has too many junctions");
        CGS_ASSERT(Is16Aligned(mpaRungs), "Rungs are not aligned");
        CGS_ASSERT(Is16Aligned(mpafCumulativeRungLengths), "Cumulative rung lengths are not aligned");
    }

    void Hull::FixDown(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        CGS_ASSERT(Is16Aligned(mpaRungs), "Rungs are not aligned");
        CGS_ASSERT(Is16Aligned(mpafCumulativeRungLengths), "Cumulative rung lengths are not aligned");

        UnBase(mpaSections, luBase);
        UnBase(mpaRungs, luBase);
        UnBase(mpafCumulativeRungLengths, luBase);
        UnBase(mpaNeighbourData, luBase);
        UnBase(mpaSectionSpans, luBase);
        UnBase(mpaStaticTrafficVehicles, luBase);
        UnBase(mpaSectionFlows, luBase);
        UnBase(mpaJunctions, luBase);
        UnBase(mpaStopLines, luBase);
        UnBase(mpaLightTriggers, luBase);
        UnBase(mpaLightTriggerStartData, luBase);
        UnBase(mpaLightTriggerJunctionLookup, luBase);
    }

    // Out-of-line forwarders pin the three inline accessors into this TU.
    const void* Hull_GetNeighbour(const Hull& lrHull, u32 luIndex)
    {
        return lrHull.GetNeighbour(luIndex);
    }

    const StaticTrafficVehicle* Hull_GetStaticVehicle(const Hull& lrHull, u32 luIndex)
    {
        return lrHull.GetStaticVehicle(luIndex);
    }

    const StopLine* Hull_GetStopLine(const Hull& lrHull, u32 luIndex)
    {
        return lrHull.GetStopLine(luIndex);
    }
}
